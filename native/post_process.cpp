#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include "post_process.h"
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
bool Run(const std::wstring& exe,std::wstring command,DWORD& exit_code) {
    std::wstring mutable_command=Quote(exe)+L" "+command;
    STARTUPINFOW si{};si.cb=sizeof(si);
    PROCESS_INFORMATION pi{};
    bool ok=CreateProcessW(exe.c_str(),mutable_command.data(),nullptr,nullptr,FALSE,
                           CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0;
    if(!ok)return false;
    WaitForSingleObject(pi.hProcess,INFINITE);
    ok=GetExitCodeProcess(pi.hProcess,&exit_code)!=0;
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return ok;
}
}

bool ConversionRequested(const Settings& settings,const std::wstring& native_path) {
    if(settings.output_ext.empty())return false;
    return Lower(settings.output_ext)!=ExtensionOf(native_path);
}

PostProcessResult PostProcessPublishedFile(const std::wstring& native_path,const Settings& settings,
                                           const std::vector<unsigned char>& cover,
                                           const std::wstring& cover_extension) {
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

    DWORD attr=GetFileAttributesW(WindowsPath(result.path).c_str());
    if(attr!=INVALID_FILE_ATTRIBUTES) {
        result.state=PostProcessResult::State::Skipped;
        if(!settings.keep_native_original && _wcsicmp(native_path.c_str(),result.path.c_str())!=0)
            DeleteFileW(WindowsPath(native_path).c_str());
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
    const auto final_ext=Lower(settings.output_ext);
    if(!cover_path.empty() && (final_ext==L"mp3"||final_ext==L"m4a"||final_ext==L"mp4")) {
        command+=L" -i "+Quote(cover_path)+L" -map 0:a:0 -map 1:v:0 -map_metadata 0 -c:v copy -disposition:v attached_pic";
    } else {
        command+=L" -map 0:a:0 -map_metadata 0";
    }
    auto args=PresetArgs(settings);
    if(!args.empty())command+=L" "+args;
    command+=L" "+Quote(temp.wstring());

    DWORD exit_code=~0u;
    bool launched=Run(ffmpeg,command,exit_code);
    if(!cover_path.empty())DeleteFileW(WindowsPath(cover_path).c_str());
    if((!launched||exit_code!=0) && final_ext==L"m4a" &&
       Lower(settings.output_preset).find(L"fdk aac")!=std::wstring::npos) {
        // Most redistributable FFmpeg builds omit non-free libfdk_aac. Preserve
        // old Soggfy's UI preset but fall back to FFmpeg's native AAC encoder.
        DeleteFileW(WindowsPath(temp.wstring()).c_str());
        std::wstring fallback=L"-y -hide_banner -loglevel warning -nostdin -i "+Quote(native_path);
        if(!cover_path.empty()) {
            fallback+=L" -i "+Quote(cover_path)+L" -map 0:a:0 -map 1:v:0 -map_metadata 0 -c:v copy -disposition:v attached_pic";
        } else fallback+=L" -map 0:a:0 -map_metadata 0";
        const auto preset=Lower(settings.output_preset);
        if(preset.find(L"224k vbr")!=std::wstring::npos)fallback+=L" -c:a aac -q:a 2";
        else if(preset.find(L"160k")!=std::wstring::npos)fallback+=L" -c:a aac -b:a 160k";
        else fallback+=L" -c:a aac -b:a 256k";
        fallback+=L" "+Quote(temp.wstring());
        launched=Run(ffmpeg,fallback,exit_code);
    }
    if(!launched||exit_code!=0) {
        DeleteFileW(WindowsPath(temp.wstring()).c_str());
        result.state=PostProcessResult::State::Failed;
        result.error=launched?"FFmpeg exited with code "+std::to_string(exit_code):"Could not start FFmpeg";
        return result;
    }

    if(!MoveFileExW(WindowsPath(temp.wstring()).c_str(),WindowsPath(result.path).c_str(),
                    MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(WindowsPath(temp.wstring()).c_str());
        result.state=PostProcessResult::State::Failed;
        result.error="Converted file could not be published";
        return result;
    }

    if(!settings.keep_native_original && _wcsicmp(native_path.c_str(),result.path.c_str())!=0)
        DeleteFileW(WindowsPath(native_path).c_str());

    result.state=PostProcessResult::State::Converted;
    return result;
}
}
