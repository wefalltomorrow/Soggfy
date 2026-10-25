#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include "post_process.h"
#include "existing_quality.h"
#include "history_settings.h"
#include <algorithm>
#include <filesystem>

namespace history {
namespace {
std::wstring Lower(std::wstring value) {
    for(auto& c:value)c=wchar_t(towlower(c));
    return value;
}
std::wstring ExtensionOf(const std::wstring& path) {
    auto ext=std::filesystem::path(path).extension().wstring();
    if(!ext.empty()&&ext.front()==L'.')ext.erase(ext.begin());
    return Lower(ext);
}
std::wstring Quote(const std::wstring& value) {
    std::wstring out=L"\"";
    size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\'){++slashes;continue;}
        if(c==L'"') {
            out.append(slashes*2+1,L'\\');out.push_back(L'"');slashes=0;continue;
        }
        out.append(slashes,L'\\');slashes=0;out.push_back(c);
    }
    out.append(slashes*2,L'\\');out.push_back(L'"');return out;
}
std::wstring FindFFmpeg(const Settings& settings) {
    if(!settings.ffmpeg_path.empty()) {
        DWORD a=GetFileAttributesW(WindowsPath(settings.ffmpeg_path).c_str());
        if(a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY))return settings.ffmpeg_path;
    }
    PWSTR local=nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))) {
        std::wstring bundled=std::wstring(local)+L"\\Soggfy\\ffmpeg\\ffmpeg.exe";
        CoTaskMemFree(local);
        DWORD a=GetFileAttributesW(WindowsPath(bundled).c_str());
        if(a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY))return bundled;
    }
    wchar_t found[32768];DWORD n=SearchPathW(nullptr,L"ffmpeg.exe",nullptr,32768,found,nullptr);
    return n&&n<32768?std::wstring(found,n):std::wstring{};
}
std::wstring PresetArgs(const Settings& settings) {
    auto p=Lower(settings.output_preset);
    if(p==L"mp3 320k")return L"-c:a libmp3lame -b:a 320k -id3v2_version 3";
    if(p==L"mp3 256k")return L"-c:a libmp3lame -b:a 256k -id3v2_version 3";
    if(p==L"mp3 192k")return L"-c:a libmp3lame -b:a 192k -id3v2_version 3";
    if(p==L"m4a 256k"||p==L"m4a 256k (fdk aac)")return L"-c:a libfdk_aac -b:a 256k -cutoff 20k";
    if(p==L"m4a 224k vbr"||p==L"m4a 224k vbr (fdk aac)")return L"-c:a libfdk_aac -vbr 5";
    if(p==L"m4a 160k"||p==L"m4a 160k (fdk aac)")return L"-c:a libfdk_aac -b:a 160k -cutoff 18k";
    if(p==L"opus 160k")return L"-c:a libopus -b:a 160k";
    if(p==L"custom")return settings.output_args;
    return settings.output_args;
}
bool WriteBytes(const std::wstring& path,const std::vector<unsigned char>& data) {
    if(data.empty()||data.size()>MAXDWORD)return false;
    HANDLE f=CreateFileW(WindowsPath(path).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,nullptr);
    if(f==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(f,data.data(),DWORD(data.size()),&written,nullptr)&&written==data.size();
    CloseHandle(f);if(!ok)DeleteFileW(WindowsPath(path).c_str());return ok;
}
bool ReadableAudioCandidate(const std::wstring& path,uint64_t* bytes=nullptr) {
    HANDLE file=CreateFileW(WindowsPath(path).c_str(),GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION info{};
    const bool ok=GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&
        (info.nFileSizeHigh||info.nFileSizeLow);
    if(ok&&bytes)*bytes=(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    CloseHandle(file);return ok;
}
bool Canceled(const PostProcessControl& control) {return control.canceled&&control.canceled();}
bool Run(const std::wstring& exe,const std::wstring& command,DWORD& exit_code,
         const PostProcessControl& control,ULONGLONG deadline,std::string& error) {
    if(Canceled(control)){error="Conversion canceled; native original retained";return false;}
    if(GetTickCount64()>=deadline){error="Conversion timed out; native original retained";return false;}
    HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if(!job){error="Could not create converter job";return false;}
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) {
        CloseHandle(job);error="Could not configure converter job";return false;
    }
    std::wstring mutable_command=Quote(exe)+L" "+command;
    STARTUPINFOW si{};si.cb=sizeof(si);
    PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),mutable_command.data(),nullptr,nullptr,FALSE,
                      CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi)) {
        error="Could not start FFmpeg (Windows error "+std::to_string(GetLastError())+")";
        CloseHandle(job);return false;
    }
    bool ok=AssignProcessToJobObject(job,pi.hProcess)!=0;
    if(ok)ok=ResumeThread(pi.hThread)!=DWORD(-1);
    if(!ok){error="Could not supervise FFmpeg";TerminateProcess(pi.hProcess,1);}
    while(ok) {
        const DWORD waited=WaitForSingleObject(pi.hProcess,100);
        if(waited==WAIT_OBJECT_0){ok=GetExitCodeProcess(pi.hProcess,&exit_code)!=0;break;}
        if(waited!=WAIT_TIMEOUT){error="Could not wait for FFmpeg";ok=false;break;}
        if(Canceled(control)){error="Conversion canceled; native original retained";ok=false;break;}
        if(GetTickCount64()>=deadline){error="Conversion timed out; native original retained";ok=false;break;}
    }
    if(!ok){TerminateJobObject(job,1);WaitForSingleObject(pi.hProcess,2000);}
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(job);
    return ok;
}
bool ValidateAudio(const std::wstring& exe,const std::wstring& path,const PostProcessControl& control,
                   ULONGLONG deadline,std::string& error) {
    if(!ReadableAudioCandidate(path)){error="Output is missing, empty, unreadable, or a directory";return false;}
    DWORD exit_code=~0u;
    const bool ran=Run(exe,L"-hide_banner -loglevel error -xerror -abort_on empty_output -nostdin -i "+Quote(path)+
        L" -map 0:a:0 -f null -",exit_code,control,deadline,error);
    if(!ran)return false;
    if(exit_code){error="Output audio validation failed (FFmpeg exit "+std::to_string(exit_code)+")";return false;}
    if(!ReadableAudioCandidate(path)){error="Output disappeared or became unreadable during validation";return false;}
    return true;
}

}

bool ConversionRequested(const Settings& settings,const std::wstring& native_path) {
    if(settings.output_ext.empty())return false;
    auto ext=Lower(settings.output_ext);
    if(!ext.empty()&&ext.front()==L'.')ext.erase(ext.begin());
    return ext!=ExtensionOf(native_path);
}

static PostProcessResult ProcessFile(const std::wstring& native_path,const Settings& settings,
                                           const std::vector<unsigned char>& cover,
                                           const std::wstring& cover_extension,const PostProcessControl& control,bool existing) {
    PostProcessResult result;result.path=native_path;
    if(!ConversionRequested(settings,native_path))return result;

    auto ffmpeg=FindFFmpeg(settings);
    if(ffmpeg.empty()) {
        result.state=PostProcessResult::State::Failed;
        result.error="FFmpeg not found. Set FFmpeg Path in Soggfy settings or add ffmpeg.exe to PATH.";
        return result;
    }

    auto final=std::filesystem::path(native_path);
    std::wstring ext=settings.output_ext;
    if(!ext.empty()&&ext.front()!=L'.')ext=L"."+ext;
    final.replace_extension(ext);
    result.path=final.wstring();

    uint64_t native_bytes=0;
    ReadableAudioCandidate(native_path,&native_bytes);
    // Five minutes plus one second per 64 KiB, capped at six hours for long media.
    const auto budget=control.timeout_ms?uint64_t(control.timeout_ms):
        std::min<uint64_t>(6*60*60*1000,300000+(native_bytes/65536)*1000);
    const auto deadline=GetTickCount64()+budget;
    DWORD attr=GetFileAttributesW(WindowsPath(result.path).c_str());
    if(attr!=INVALID_FILE_ATTRIBUTES) {
        if(!ValidateAudio(ffmpeg,result.path,control,deadline,result.error)) {
            result.state=PostProcessResult::State::Failed;
            result.error="Existing output conflict: "+result.error+"; native original retained";
        } else result.state=PostProcessResult::State::Skipped;
        // Even decodable pre-existing audio is not proof of this capture's identity.
        // Retain the new native original; never overwrite the existing destination.
        return result;
    }
    const auto attribute_error=GetLastError();
    if(attribute_error!=ERROR_FILE_NOT_FOUND&&attribute_error!=ERROR_PATH_NOT_FOUND) {
        result.state=PostProcessResult::State::Failed;
        result.error="Cannot inspect output destination; native original retained";return result;
    }

    if(existing&&!ValidateAudio(ffmpeg,native_path,control,deadline,result.error)) {
        result.state=PostProcessResult::State::Failed;
        result.error="Existing native validation failed: "+result.error+"; original retained";
        return result;
    }
    static volatile LONG seq=0;
    const LONG id=InterlockedIncrement(&seq);
    auto temp=final.parent_path()/
        (final.stem().wstring()+L".soggfy-converting-"+std::to_wstring(GetCurrentProcessId())+
         L"-"+std::to_wstring(id)+final.extension().wstring());

    std::wstring cover_path;
    if(settings.embed_cover_art && !cover.empty()) {
        std::wstring cover_ext=(Lower(cover_extension)==L".png")?L".png":L".jpg";
        cover_path=(final.parent_path()/
            (L".soggfy-cover-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(id)+cover_ext)).wstring();
        if(!WriteBytes(cover_path,cover))cover_path.clear();
    }

    std::wstring command=L"-y -hide_banner -loglevel warning -nostdin -i "+Quote(native_path);
    const auto final_ext=ExtensionOf(result.path);
    if(!cover_path.empty() && (final_ext==L"mp3"||final_ext==L"m4a"||final_ext==L"mp4")) {
        command+=L" -i "+Quote(cover_path)+L" -map 0:a:0 -map 1:v:0 -map_metadata 0 -c:v copy -disposition:v attached_pic";
    } else if(existing&&settings.embed_cover_art&&(final_ext==L"mp3"||final_ext==L"m4a"||final_ext==L"mp4")) {
        command+=L" -map 0:a:0 -map 0:v? -map_metadata 0 -c:v copy -disposition:v attached_pic";
    } else {
        command+=L" -map 0:a:0 -map_metadata 0";
    }
    auto args=PresetArgs(settings);
    if(!args.empty())command+=L" "+args;
    command+=L" "+Quote(temp.wstring());

    DWORD exit_code=~0u;
    bool launched=Run(ffmpeg,command,exit_code,control,deadline,result.error);
    if(launched&&exit_code!=0 && final_ext==L"m4a" &&
       Lower(settings.output_preset).find(L"fdk aac")!=std::wstring::npos) {
        // Most redistributable FFmpeg builds omit non-free libfdk_aac. Preserve
        // old Soggfy's UI preset but fall back to FFmpeg's native AAC encoder.
        DeleteFileW(WindowsPath(temp.wstring()).c_str());
        std::wstring fallback=L"-y -hide_banner -loglevel warning -nostdin -i "+Quote(native_path);
        if(!cover_path.empty()) {
            fallback+=L" -i "+Quote(cover_path)+L" -map 0:a:0 -map 1:v:0 -map_metadata 0 -c:v copy -disposition:v attached_pic";
        } else if(existing&&settings.embed_cover_art)
            fallback+=L" -map 0:a:0 -map 0:v? -map_metadata 0 -c:v copy -disposition:v attached_pic";
        else fallback+=L" -map 0:a:0 -map_metadata 0";
        const auto preset=Lower(settings.output_preset);
        if(preset.find(L"224k vbr")!=std::wstring::npos)fallback+=L" -c:a aac -q:a 2";
        else if(preset.find(L"160k")!=std::wstring::npos)fallback+=L" -c:a aac -b:a 160k";
        else fallback+=L" -c:a aac -b:a 256k";
        fallback+=L" "+Quote(temp.wstring());
        launched=Run(ffmpeg,fallback,exit_code,control,deadline,result.error);
    }
    if(!cover_path.empty())DeleteFileW(WindowsPath(cover_path).c_str());
    if(!launched||exit_code!=0) {
        DeleteFileW(WindowsPath(temp.wstring()).c_str());
        result.state=PostProcessResult::State::Failed;
        if(result.error.empty())result.error=launched?"FFmpeg exited with code "+std::to_string(exit_code):"Could not start FFmpeg";
        return result;
    }

    if(Canceled(control)||!ValidateAudio(ffmpeg,temp.wstring(),control,deadline,result.error)||
       Canceled(control)||GetTickCount64()>=deadline) {
        DeleteFileW(WindowsPath(temp.wstring()).c_str());
        result.state=PostProcessResult::State::Failed;
        if(result.error.empty())result.error=GetTickCount64()>=deadline?
            "Conversion timed out; native original retained":"Conversion canceled; native original retained";
        return result;
    }
    if(!MoveFileExW(WindowsPath(temp.wstring()).c_str(),WindowsPath(result.path).c_str(),
                    MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(WindowsPath(temp.wstring()).c_str());
        result.state=PostProcessResult::State::Failed;
        result.error="Converted file could not be published";
        return result;
    }

    if(!settings.keep_native_original && !Canceled(control) && _wcsicmp(native_path.c_str(),result.path.c_str())!=0)
        DeleteFileW(WindowsPath(native_path).c_str());

    result.state=PostProcessResult::State::Converted;
    return result;
}
PostProcessResult PostProcessPublishedFile(const std::wstring& native_path,const Settings& settings,
    const std::vector<unsigned char>& cover,const std::wstring& cover_extension,const PostProcessControl& control) {
    return ProcessFile(native_path,settings,cover,cover_extension,control,false);
}
PostProcessResult RetryPostProcessExistingFile(const std::wstring& native_path,const Settings& settings,const PostProcessControl& control) {
    if(!ConversionRequested(settings,native_path))return {PostProcessResult::State::Native,native_path,{}};
    struct SourceLock {
        HANDLE handle=INVALID_HANDLE_VALUE;
        ~SourceLock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    } source;
    source.handle=CreateFileW(WindowsPath(native_path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Quality quality;
    if(source.handle==INVALID_HANDLE_VALUE||!ReadQuality(native_path,quality)||
       (quality.codec!=Codec::Flac&&quality.codec!=Codec::Vorbis))
        return {PostProcessResult::State::Failed,native_path,"Existing native file is unreadable or unverified; original retained"};
    auto retained=settings;retained.keep_native_original=true;
    return ProcessFile(native_path,retained,{},L"",control,true);
}
}
