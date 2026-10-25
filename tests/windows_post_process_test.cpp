#include "../native/post_process.h"
#include "../native/existing_quality.h"
#include "../native/conversion_source.h"
#include <shellapi.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
namespace fs=std::filesystem;
namespace history { std::wstring WindowsPath(const std::wstring& path){return path;} }
using namespace history;
static void write(const fs::path& p,const std::string& bytes){std::ofstream f(p,std::ios::binary);f<<bytes;assert(f.good());}
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static std::wstring env(const wchar_t* name){wchar_t b[32768]{};GetEnvironmentVariableW(name,b,32768);return b;}
static void hang(const std::wstring& exe) {
    std::wstring command=L"\""+exe+L"\" child";STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    assert(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi));
    write(fs::path(env(L"SGF_TEST_PID")),std::to_string(pi.dwProcessId));CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    Sleep(60000);
}
// Identification header for the controlled converter, not a playable fixture.
// Real codec fixtures are generated and fully decoded in the FFmpeg section.
static std::string identifiedFlac() {
    std::string b(42,'\0');b.replace(0,4,"fLaC");b[4]=char(128);b[7]=34;
    b[8]=b[10]=16;
    const uint64_t packed=(uint64_t(44100)<<44)|(uint64_t(1)<<41)|(uint64_t(15)<<36)|44100;
    for(unsigned i=0;i<8;++i)b[18+i]=char(packed>>(56-8*i));
    return b+"controlled-valid-audio";
}
static void wav(const fs::path& p) {
    std::string b(44+44100*2,'\0');
    auto put=[&](unsigned at,unsigned value,unsigned n){for(unsigned i=0;i<n;++i)b[at+i]=char(value>>(i*8));};
    b.replace(0,4,"RIFF");put(4,unsigned(b.size()-8),4);b.replace(8,8,"WAVEfmt ");put(16,16,4);
    put(20,1,2);put(22,1,2);put(24,44100,4);put(28,88200,4);put(32,2,2);put(34,16,2);
    b.replace(36,4,"data");put(40,88200,4);write(p,b);
}
int main() {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);assert(argv);
    std::vector<std::wstring> args(argv,argv+argc);LocalFree(argv);
    if(argc>1&&args[1]==L"child"){Sleep(60000);return 0;}
    if(argc>1&&args[1]==L"-hide_banner") {
        if(env(L"SGF_TEST_MODE")==L"validation-hang")hang(args[0]);
        auto input=std::find(args.begin(),args.end(),L"-i");assert(input!=args.end());
        const auto bytes=read(fs::path(*++input));
        return bytes=="valid-audio"||bytes==identifiedFlac()?0:2;
    }
    if(argc>1&&args[1]==L"-y") {
        assert(std::find(args.begin(),args.end(),L"-xerror")!=args.end());
        const auto mode=env(L"SGF_TEST_MODE");
        if(mode==L"source-lock") {
            auto input=std::find(args.begin(),args.end(),L"-i");assert(input!=args.end());
            const auto path=*++input;
            HANDLE writer=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
            assert(writer==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_SHARING_VIOLATION);
            assert(!MoveFileExW(path.c_str(),(path+L".replaced").c_str(),0));
        }
        if(mode==L"no-fdk"&&std::find(args.begin(),args.end(),L"libfdk_aac")!=args.end())return 1;
        if(mode==L"hang"||mode==L"cancel") {
            write(fs::path(args.back()),"partial");hang(args[0]);return 0;
        }
        if(mode==L"fail")return 1;
        if(mode==L"race")write(fs::path(env(L"SGF_TEST_FINAL")),"other-file");
        if(mode==L"directory")fs::create_directory(fs::path(env(L"SGF_TEST_FINAL")));
        const auto log=env(L"SGF_TEST_ARGS");
        if(!log.empty()) {
            std::string joined;
            for(const auto& arg:args){joined+=std::string(arg.begin(),arg.end());joined+='\n';}
            write(fs::path(log),joined);
        }
        write(fs::path(args.back()),mode==L"empty"?"":mode==L"corrupt"?"not-audio":"valid-audio");
        return 0;
    }
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    const auto base=fs::temp_directory_path()/(L"soggfy-conversion-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    assert(fs::create_directory(base));
    // Exercise the close/reopen deletion gap deterministically.
    const auto identity=base/L"identity.flac",moved=base/L"moved.flac";
    for(bool replace:{true,false}) {
        write(identity,"original");
        post_process_detail::SourceLock source(identity.wstring());
        assert(source.handle!=INVALID_HANDLE_VALUE);source.Close();
        if(replace)fs::rename(identity,moved);
        const auto changed=replace?"replaced":"replacement-bytes";
        write(identity,changed);
        if(replace) {
            // Same size and timestamps: only file identity distinguishes it.
            HANDLE attrs=CreateFileW(identity.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
            assert(attrs!=INVALID_HANDLE_VALUE);
            assert(SetFileTime(attrs,&source.original.ftCreationTime,nullptr,&source.original.ftLastWriteTime));CloseHandle(attrs);
        }
        assert(!source.DeleteValidated(identity.wstring()));source.Close();
        assert(read(identity)==changed);
    }
    Settings s;s.output_ext=L"mp3";s.output_preset=L"MP3 320k";s.ffmpeg_path=executable;s.keep_native_original=false;
    const auto native=base/L"capture.wav",final=base/L"capture.mp3",pidfile=base/L"child.pid";
    SetEnvironmentVariableW(L"SGF_TEST_PID",pidfile.c_str());SetEnvironmentVariableW(L"SGF_TEST_FINAL",final.c_str());
    auto reset=[&]{fs::remove_all(final);write(native,"native-original");};
    Quality incoming{Codec::Flac,192000,2,24,0};
    reset();assert(DecideSave(native.wstring(),incoming)==SaveDecision::Skip);
    assert(DecideSave(final.wstring(),incoming)==SaveDecision::NewFile);
    fs::create_directory(final);assert(DecideSave(final.wstring(),incoming)==SaveDecision::Skip);
    fs::remove(final);write(final,"");assert(DecideSave(final.wstring(),incoming)==SaveDecision::Skip);
    HANDLE locked=CreateFileW(native.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);assert(locked!=INVALID_HANDLE_VALUE);
    assert(DecideSave(native.wstring(),incoming)==SaveDecision::Skip);CloseHandle(locked);
    auto run=[&](const wchar_t* mode,PostProcessControl control=PostProcessControl{}) {
        SetEnvironmentVariableW(L"SGF_TEST_MODE",mode);
        auto result=PostProcessPublishedFile(native.wstring(),s,{},L"",control);
        for(const auto& entry:fs::directory_iterator(base))assert(entry.path().filename().wstring().find(L"soggfy-converting-")==std::wstring::npos);
        return result;
    };
    reset();
    locked=CreateFileW(native.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    assert(locked!=INVALID_HANDLE_VALUE);
    assert(run(L"ok").state==PostProcessResult::State::Failed);
    CloseHandle(locked);assert(read(native)=="native-original"&&!fs::exists(final));
    reset();assert(run(L"source-lock").state==PostProcessResult::State::Converted);assert(!fs::exists(native));
    for(unsigned existing=0;existing<5;++existing) {
        reset();HANDLE locked=INVALID_HANDLE_VALUE;
        if(existing==0)fs::create_directory(final);
        else write(final,existing==1?"":existing==2?"corrupt":"valid-audio");
        if(existing==4)locked=CreateFileW(final.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
        const auto result=run(L"ok");
        assert(result.state==(existing==3?PostProcessResult::State::Skipped:PostProcessResult::State::Failed));
        assert(read(native)=="native-original");if(locked!=INVALID_HANDLE_VALUE)CloseHandle(locked);
    }
    for(auto mode:{L"fail",L"empty",L"corrupt",L"race",L"hang",L"validation-hang",L"cancel"}) {
        reset();fs::remove(pidfile);PostProcessControl control;control.timeout_ms=1500;
        const auto started=GetTickCount64();
        if(std::wstring(mode)==L"cancel")control.canceled=[&]{return GetTickCount64()-started>700;};
        const auto result=run(mode,control);
        assert(result.state==PostProcessResult::State::Failed);assert(!result.error.empty());assert(read(native)=="native-original");
        assert(GetTickCount64()-started<6000);
        if(std::wstring(mode)==L"race")assert(read(final)=="other-file");
        else assert(!fs::exists(final));
        if(std::wstring(mode)==L"hang"||std::wstring(mode)==L"validation-hang"||std::wstring(mode)==L"cancel") {
            assert(fs::exists(pidfile));const DWORD pid=DWORD(std::stoul(read(pidfile)));
            HANDLE child=OpenProcess(SYNCHRONIZE,FALSE,pid);
            if(child){assert(WaitForSingleObject(child,2000)==WAIT_OBJECT_0);CloseHandle(child);}
            assert(result.error.find(std::wstring(mode)==L"cancel"?"canceled":"timed out")!=std::string::npos);
        }
    }
    reset();assert(run(L"ok").state==PostProcessResult::State::Converted);assert(!fs::exists(native));assert(read(final)=="valid-audio");
    reset();s.keep_native_original=true;assert(run(L"ok").state==PostProcessResult::State::Converted);assert(fs::exists(native));
    reset();s.keep_native_original=false;
    // Cancellation observed after the validation child exits still retains the original.
    PostProcessControl canceled;canceled.canceled=[] {return true;};
    assert(run(L"ok",canceled).state==PostProcessResult::State::Failed);assert(fs::exists(native));
    s.output_ext=L".WAV";assert(!ConversionRequested(s,native.wstring()));s.output_ext=L"mp3";
    // An existing native file must no longer bypass conversion after a failure.
    // Keep its exact bytes even when keep_native_original is disabled: the
    // existing file's identity cannot be inferred from its name alone.
    reset();write(native,identifiedFlac());
    assert(DecideSave(native.wstring(),Quality{Codec::Flac,44100,2,16,0})==SaveDecision::Skip);
    SetEnvironmentVariableW(L"SGF_TEST_MODE",L"fail");
    assert(RetryPostProcessExistingFile(native.wstring(),s).state==PostProcessResult::State::Failed);
    assert(read(native)==identifiedFlac()&&!fs::exists(final));
    SetEnvironmentVariableW(L"SGF_TEST_MODE",L"ok");
    assert(RetryPostProcessExistingFile(native.wstring(),s).state==PostProcessResult::State::Converted);
    assert(read(native)==identifiedFlac()&&read(final)=="valid-audio");
    assert(RetryPostProcessExistingFile(native.wstring(),s).state==PostProcessResult::State::Skipped);
    assert(read(native)==identifiedFlac());
    fs::remove(final);write(native,identifiedFlac()+"corrupt");
    assert(RetryPostProcessExistingFile(native.wstring(),s).state==PostProcessResult::State::Failed);
    assert(!fs::exists(final)&&read(native)==identifiedFlac()+"corrupt");
    write(native,"not-a-native-file");
    assert(RetryPostProcessExistingFile(native.wstring(),s).state==PostProcessResult::State::Failed);
    write(native,identifiedFlac());
    assert(RetryPostProcessExistingFile(native.wstring(),s,canceled).state==PostProcessResult::State::Failed);
    locked=CreateFileW(native.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);assert(locked!=INVALID_HANDLE_VALUE);
    assert(RetryPostProcessExistingFile(native.wstring(),s).state==PostProcessResult::State::Failed);CloseHandle(locked);
    assert(read(native)==identifiedFlac()&&!fs::exists(final));
    const auto real=env(L"SGF_REAL_FFMPEG");
    // Legacy preset aliases must get the same native AAC fallback as UI names.
    for(const auto* preset:{L"m4a 256k",L"m4a 224k vbr",L"m4a 160k",L"m4a 256k (fdk aac)",L"m4a 224k vbr (fdk aac)",L"m4a 160k (fdk aac)"}) {
        reset();auto aac=s;aac.output_ext=L"m4a";aac.output_preset=preset;
        SetEnvironmentVariableW(L"SGF_TEST_MODE",L"no-fdk");
        auto result=PostProcessPublishedFile(native.wstring(),aac,{},L"");
        assert(result.state==PostProcessResult::State::Converted&&read(result.path)=="valid-audio");
        fs::remove(result.path);
    }
    reset();auto custom=s;custom.output_ext=L"m4a";custom.output_preset=L"Custom";custom.output_args=L"-c:a libfdk_aac";
    assert(PostProcessPublishedFile(native.wstring(),custom,{},L"").state==PostProcessResult::State::Failed);
    assert(fs::exists(native));
    if(!real.empty()) {
        reset();wav(native);s.ffmpeg_path=real;
        const auto started=GetTickCount64();auto result=run(L"ok");
        if(result.state!=PostProcessResult::State::Converted)std::fprintf(stderr,"Real FFmpeg: %s\n",result.error.c_str());
        assert(result.state==PostProcessResult::State::Converted&&!fs::exists(native));assert(fs::file_size(final)>0);
        wav(native);assert(run(L"ok").state==PostProcessResult::State::Skipped&&fs::exists(native));
        write(final,"corrupt");assert(run(L"ok").state==PostProcessResult::State::Failed&&fs::exists(native));
        wav(final);auto header=read(final).substr(0,44);header[4]=36;header[5]=header[6]=header[7]=0;
        for(unsigned i=40;i<44;++i)header[i]=0;
        write(final,header);assert(run(L"ok").state==PostProcessResult::State::Failed&&fs::exists(native));
        for(const auto& codec:{L"flac",L"ogg"}) {
            reset();wav(native);
            auto generate=s;generate.output_ext=codec;generate.output_preset=L"Custom";
            generate.output_args=std::wstring(codec)==L"flac"?L"-c:a flac":L"-c:a libvorbis";
            const auto fixture=PostProcessPublishedFile(native.wstring(),generate,{},L"");
            assert(fixture.state==PostProcessResult::State::Converted);
            const auto original=read(fixture.path);Quality quality;assert(ReadQuality(fixture.path,quality));
            auto broken=s;broken.output_preset=L"Custom";broken.output_args=L"-c:a nonexistent_soggfy_codec";
            assert(RetryPostProcessExistingFile(fixture.path,broken).state==PostProcessResult::State::Failed);
            assert(read(fixture.path)==original&&!fs::exists(final));
            assert(RetryPostProcessExistingFile(fixture.path,s).state==PostProcessResult::State::Converted);
            assert(read(fixture.path)==original&&fs::file_size(final)>0);
            assert(RetryPostProcessExistingFile(fixture.path,s).state==PostProcessResult::State::Skipped);
            fs::remove(final);
            if(std::wstring(codec)==L"flac") {
                auto damaged=original;assert(damaged.size()>256);
                damaged.replace(damaged.size()-128,128,128,'\0');write(fixture.path,damaged);
                assert(PostProcessPublishedFile(fixture.path,s,{},L"").state==PostProcessResult::State::Failed);
                assert(read(fixture.path)==damaged&&!fs::exists(final));
            }
            write(fixture.path,original.substr(0,42));
            assert(RetryPostProcessExistingFile(fixture.path,s).state==PostProcessResult::State::Failed);
            assert(!fs::exists(final)&&fs::file_size(fixture.path)==42);fs::remove(fixture.path);
        }
        std::printf("PASS: real FFmpeg MP3 320k encode/decode, FLAC/Ogg retry and corrupt-source/output verification (%llu ms)\n",GetTickCount64()-started);
    }
    fs::remove_all(base);
    std::puts("PASS: conversion conflicts, validation, no-overwrite race, timeout/cancel, child cleanup and native retention");
}
