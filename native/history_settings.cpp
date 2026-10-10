#define WIN32_LEAN_AND_MEAN
#include "history_settings.h"
#include "playback_speed_limit.h"
#include <shobjidl.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <cmath>
#include "async_log.h"
namespace history {
static SRWLOCK lock=SRWLOCK_INIT;
static Settings settings;
static std::wstring ini,music_path;
static bool IsMusic(std::wstring root) {
    while(!root.empty() && (root.back()==L'\\' || root.back()==L'/')) root.pop_back();
    return !music_path.empty() && (_wcsicmp(root.c_str(),music_path.c_str())==0 ||
        _wcsicmp(root.c_str(),(music_path+L"\\Spotify").c_str())==0);
}
static volatile LONG downloads_enabled=0,ogg_enabled=1,flac_enabled=1,capture_epoch=0,log_enabled=1,debug_enabled=0;
static HANDLE settings_signal=nullptr,persist_done=nullptr;
static volatile LONG persisted_generation=-1,persist_failed=0;
static constexpr char default_ini[]=
    "[Soggfy]\r\n"
    "; Classic UI recreates the original Soggfy top-bar integration.\r\n"
    "Classic UI=1\r\n"
    "; Optional fallback to Floggfy's native To Disk menu when Classic UI=0.\r\n"
    "Native Menu=0\r\n"
    "Playback Speed=1\r\n"
    "Skip Downloaded Tracks=0\r\n"
    "Skip Ignored Tracks=0\r\n"
    "Embed Cover Art=1\r\n"
    "Save Cover Art=1\r\n"
    "Embed Lyrics=1\r\n"
    "Save Lyrics=1\r\n"
    "Save Canvas=0\r\n"
    "Block Telemetry=1\r\n"
    "Lift Add To Queue=0\r\n"
    "Keep Native Original=1\r\n"
    "Output Preset=Native\r\n"
    "Output Ext=\r\n"
    "Output Args=\r\n"
    "FFmpeg Path=\r\n"
    "Podcast Template=Podcasts/{artist_name}/{album_name}/{release_date} - {track_name}.{ext}\r\n"
    "Canvas Template={artist_name}/{album_name}{multi_disc_path}/Canvas/{track_num}. {track_name}.mp4\r\n"
    "Invalid Char Replacement=unicode\r\n"
    "\r\n"
    "[To Disk]\r\n"
    "Downloads=0\r\n"
    "; Empty uses the Windows Music folder plus \\Spotify.\r\n"
    "Save Location=\r\n"
    "; Fresh installs use the original Soggfy-style track template. Clear this for the smart layout.\r\n"
    "Path Template={artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}.{ext}\r\n"
    "; Path-only cleanup; leaves names such as AC/DC alone.\r\n"
    "Normalize Artist Separators=1\r\n"
    "Ogg=1\r\n"
    "; Captures native FLAC when Spotify supplies lossless audio.\r\n"
    "Flac=1\r\n"
    "; Reads existing renderer and local client caches; makes no requests.\r\n"
    "Metadata=1\r\n"
    "; Activity log in Save Location, capped at 5 MiB.\r\n"
    "Log=1\r\n"
    "DebugLog=0\r\n"
    "\r\n"
    "[History]\r\n"
    "; Maximum compressed audio held in RAM for one track.\r\n"
    "MaxBufferedMiB=500\r\n";
static bool CreateDefaultIni(const std::wstring& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,
        CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return GetLastError()==ERROR_FILE_EXISTS || GetLastError()==ERROR_ALREADY_EXISTS;
    DWORD written=0;const DWORD size=DWORD(sizeof(default_ini)-1);
    bool ok=WriteFile(file,default_ini,size,&written,nullptr) && written==size;
    CloseHandle(file);if(!ok)DeleteFileW(path.c_str());return ok;
}
static DWORD WINAPI PersistSettings(LPVOID) {
    for(;;) {
        WaitForSingleObject(settings_signal,INFINITE);
        auto snapshot=GetSettings();
        bool ok=true;
        auto flag=[&](const wchar_t* section,const wchar_t* key,bool value) {
            if(!WritePrivateProfileStringW(section,key,value?L"1":L"0",ini.c_str()))ok=false;
        };
        auto text=[&](const wchar_t* section,const wchar_t* key,const std::wstring& value) {
            if(!WritePrivateProfileStringW(section,key,value.c_str(),ini.c_str()))ok=false;
        };
        flag(L"Soggfy",L"Classic UI",snapshot.classic_ui);
        flag(L"Soggfy",L"Native Menu",snapshot.menu);
        flag(L"Soggfy",L"Skip Downloaded Tracks",snapshot.skip_downloaded_tracks);
        flag(L"Soggfy",L"Skip Ignored Tracks",snapshot.skip_ignored_tracks);
        flag(L"Soggfy",L"Embed Cover Art",snapshot.embed_cover_art);
        flag(L"Soggfy",L"Save Cover Art",snapshot.save_cover_art);
        flag(L"Soggfy",L"Embed Lyrics",snapshot.embed_lyrics);
        flag(L"Soggfy",L"Save Lyrics",snapshot.save_lyrics);
        flag(L"Soggfy",L"Save Canvas",snapshot.save_canvas);
        flag(L"Soggfy",L"Block Telemetry",snapshot.block_telemetry);
        flag(L"Soggfy",L"Lift Add To Queue",snapshot.lift_add_to_queue);
        flag(L"Soggfy",L"Keep Native Original",snapshot.keep_native_original);

        wchar_t speed[32];swprintf(speed,32,L"%.3g",snapshot.playback_speed);
        text(L"Soggfy",L"Playback Speed",speed);
        text(L"Soggfy",L"Output Preset",snapshot.output_preset);
        text(L"Soggfy",L"Output Ext",snapshot.output_ext);
        text(L"Soggfy",L"Output Args",snapshot.output_args);
        text(L"Soggfy",L"FFmpeg Path",snapshot.ffmpeg_path);
        text(L"Soggfy",L"Podcast Template",snapshot.podcast_template);
        text(L"Soggfy",L"Canvas Template",snapshot.canvas_template);
        text(L"Soggfy",L"Invalid Char Replacement",snapshot.invalid_char_repl);

        flag(L"To Disk",L"Downloads",snapshot.downloads);
        flag(L"To Disk",L"Ogg",snapshot.ogg);
        flag(L"To Disk",L"FLAC",snapshot.flac);
        flag(L"To Disk",L"Metadata",snapshot.metadata);
        flag(L"To Disk",L"Log",snapshot.log);
        flag(L"To Disk",L"DebugLog",snapshot.debug_log);
        flag(L"To Disk",L"Normalize Artist Separators",snapshot.normalize_artist_separators);
        text(L"To Disk",L"Save Location",snapshot.save_location);
        text(L"To Disk",L"Path Template",snapshot.path_template);

        InterlockedExchange(&persist_failed,!ok);
        InterlockedExchange(&persisted_generation,LONG(snapshot.generation));
        SetEvent(persist_done);
        if(!ok)LogActivity("failed","settings persistence");
    }
}
bool FlushSettings(unsigned timeout_ms) {
    unsigned expected=GetSettings().generation;ULONGLONG end=GetTickCount64()+timeout_ms;
    while(unsigned(InterlockedCompareExchange(&persisted_generation,0,0))!=expected) {
        if(GetTickCount64()>=end)return false;
        WaitForSingleObject(persist_done,10);
    }return !persist_failed;
}
std::wstring WindowsPath(const std::wstring& p) {
    if(p.rfind(L"\\\\?\\",0)==0) return p;
    if(p.rfind(L"\\\\",0)==0) return L"\\\\?\\UNC\\"+p.substr(2);
    if(p.size()>2 && p[1]==L':') return L"\\\\?\\"+p;
    return p;
}
bool EnsureDirectory(const std::wstring& path) {
    if(path.empty()) return false;
    auto p=WindowsPath(path); DWORD a=GetFileAttributesW(p.c_str());
    if(a!=INVALID_FILE_ATTRIBUTES) return (a&FILE_ATTRIBUTE_DIRECTORY)!=0;
    size_t slash=path.find_last_of(L"\\/");
    if(slash!=std::wstring::npos && slash>2 && !EnsureDirectory(path.substr(0,slash))) return false;
    return CreateDirectoryW(p.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS;
}
void HistoryLog(const char* message) {
    QueueDiagnostic(message);
    if(std::strstr(message,"failed") || std::strstr(message,"discarded") ||
       std::strstr(message,"rejected") || std::strstr(message,"overflow") ||
       std::strstr(message,"unsupported") || std::strstr(message,"stopped after"))
        LogActivity("failed",message);
}
void InitSettings(HMODULE proxy) {
    wchar_t path[2048]; DWORD n=GetModuleFileNameW(proxy,path,2048);
    if(!n || n>=2048) return;
    wchar_t* slash=wcsrchr(path,L'\\'); if(!slash) return; *slash=0;
    ini=std::wstring(path)+L"\\SpotifyHistory.ini";
    DWORD attributes=GetFileAttributesW(ini.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES &&
       (GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND) &&
       !CreateDefaultIni(ini))
        QueueDiagnostic("failed to create default SpotifyHistory.ini; using built-in settings");
    settings.downloads=GetPrivateProfileIntW(L"To Disk",L"Downloads",GetPrivateProfileIntW(L"History",L"Enabled",0,ini.c_str()),ini.c_str())!=0;
    settings.classic_ui=GetPrivateProfileIntW(L"Soggfy",L"Classic UI",1,ini.c_str())!=0;
    settings.menu=GetPrivateProfileIntW(L"Soggfy",L"Native Menu",0,ini.c_str())!=0;
    settings.ogg=GetPrivateProfileIntW(L"To Disk",L"Ogg",1,ini.c_str())!=0;
    settings.flac=GetPrivateProfileIntW(L"To Disk",L"FLAC",1,ini.c_str())!=0;
    settings.metadata=GetPrivateProfileIntW(L"To Disk",L"Metadata",1,ini.c_str())!=0;
    settings.normalize_artist_separators=GetPrivateProfileIntW(L"To Disk",L"Normalize Artist Separators",1,ini.c_str())!=0;
    settings.log=GetPrivateProfileIntW(L"To Disk",L"Log",1,ini.c_str())!=0;
    settings.debug_log=GetPrivateProfileIntW(L"To Disk",L"DebugLog",0,ini.c_str())!=0;

    settings.skip_downloaded_tracks=GetPrivateProfileIntW(L"Soggfy",L"Skip Downloaded Tracks",0,ini.c_str())!=0;
    settings.skip_ignored_tracks=GetPrivateProfileIntW(L"Soggfy",L"Skip Ignored Tracks",0,ini.c_str())!=0;
    settings.embed_cover_art=GetPrivateProfileIntW(L"Soggfy",L"Embed Cover Art",1,ini.c_str())!=0;
    settings.save_cover_art=GetPrivateProfileIntW(L"Soggfy",L"Save Cover Art",1,ini.c_str())!=0;
    settings.embed_lyrics=GetPrivateProfileIntW(L"Soggfy",L"Embed Lyrics",1,ini.c_str())!=0;
    settings.save_lyrics=GetPrivateProfileIntW(L"Soggfy",L"Save Lyrics",1,ini.c_str())!=0;
    settings.save_canvas=GetPrivateProfileIntW(L"Soggfy",L"Save Canvas",0,ini.c_str())!=0;
    settings.block_telemetry=GetPrivateProfileIntW(L"Soggfy",L"Block Telemetry",1,ini.c_str())!=0;
    settings.lift_add_to_queue=GetPrivateProfileIntW(L"Soggfy",L"Lift Add To Queue",0,ini.c_str())!=0;
    settings.keep_native_original=GetPrivateProfileIntW(L"Soggfy",L"Keep Native Original",1,ini.c_str())!=0;

    settings.max_buffered_mib=std::max(8u,std::min(512u,GetPrivateProfileIntW(L"History",L"MaxBufferedMiB",500,ini.c_str())));

    wchar_t raw[2048],expanded[4096],raw_template[4096],value[4096];
    GetPrivateProfileStringW(L"To Disk",L"Save Location",L"",raw,2048,ini.c_str());
    GetPrivateProfileStringW(L"To Disk",L"Path Template",L"{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}.{ext}",raw_template,4096,ini.c_str());
    settings.path_template=raw_template;

    GetPrivateProfileStringW(L"Soggfy",L"Playback Speed",L"1",value,4096,ini.c_str());
    settings.playback_speed=std::max(1.0,std::min(kMaxPlaybackSpeed,wcstod(value,nullptr)));
    GetPrivateProfileStringW(L"Soggfy",L"Output Preset",L"Native",value,4096,ini.c_str());settings.output_preset=value;
    GetPrivateProfileStringW(L"Soggfy",L"Output Ext",L"",value,4096,ini.c_str());settings.output_ext=value;
    GetPrivateProfileStringW(L"Soggfy",L"Output Args",L"",value,4096,ini.c_str());settings.output_args=value;
    GetPrivateProfileStringW(L"Soggfy",L"FFmpeg Path",L"",value,4096,ini.c_str());settings.ffmpeg_path=value;
    GetPrivateProfileStringW(L"Soggfy",L"Podcast Template",L"Podcasts/{artist_name}/{album_name}/{release_date} - {track_name}.{ext}",value,4096,ini.c_str());settings.podcast_template=value;
    GetPrivateProfileStringW(L"Soggfy",L"Canvas Template",L"{artist_name}/{album_name}{multi_disc_path}/Canvas/{track_num}. {track_name}.mp4",value,4096,ini.c_str());settings.canvas_template=value;
    GetPrivateProfileStringW(L"Soggfy",L"Invalid Char Replacement",L"unicode",value,4096,ini.c_str());settings.invalid_char_repl=value;
    PWSTR music=nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Music,0,nullptr,&music))) {
        music_path=music; CoTaskMemFree(music);
    }
    if(!raw[0] && !music_path.empty()) settings.root=music_path+L"\\Spotify";
    DWORD length=ExpandEnvironmentStringsW(raw,expanded,4096);
    if(raw[0] && length && length<=4096) {settings.root=expanded;settings.save_location=raw;}
    settings.music_folder=IsMusic(settings.root);
    downloads_enabled=settings.downloads; ogg_enabled=settings.ogg; flac_enabled=settings.flac;
    log_enabled=settings.log;debug_enabled=settings.debug_log;
    settings_signal=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    persist_done=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE thread=settings_signal&&persist_done ? CreateThread(nullptr,0,PersistSettings,nullptr,0,nullptr) : nullptr;
    if(thread) CloseHandle(thread);
    else {
        if(settings_signal) CloseHandle(settings_signal);
        if(persist_done) CloseHandle(persist_done);
        settings_signal=nullptr;persist_done=nullptr;
    }
    persisted_generation=LONG(settings.generation);StartLogger();
}
Settings GetSettings() {
    AcquireSRWLockShared(&lock); auto copy=settings; ReleaseSRWLockShared(&lock); return copy;
}
bool LoggingEnabled() { return log_enabled!=0; }
bool DebugLoggingEnabled() { return debug_enabled!=0; }
bool OggEnabled() { return downloads_enabled && ogg_enabled; }
bool FlacEnabled() { return downloads_enabled && flac_enabled; }
unsigned CaptureEpoch() { return unsigned(InterlockedCompareExchange(&capture_epoch,0,0)); }
static bool SetBool(bool value,bool& field,volatile LONG* fast,bool affects_capture) {
    AcquireSRWLockExclusive(&lock);
    bool ok=settings_signal!=nullptr;
    if(ok) {
        if(field!=value && affects_capture) {
            ++settings.capture_epoch;
            InterlockedExchange(&capture_epoch,LONG(settings.capture_epoch));
        }
        field=value;
        if(fast) InterlockedExchange(fast,value);
        ++settings.generation;
    }
    ReleaseSRWLockExclusive(&lock);
    if(ok)SetEvent(settings_signal);
    return ok;
}
bool SetDownloads(bool v) { return SetBool(v,settings.downloads,&downloads_enabled,true); }
bool SetOgg(bool v) { return SetBool(v,settings.ogg,&ogg_enabled,true); }
bool SetFlac(bool v) { return SetBool(v,settings.flac,&flac_enabled,true); }
bool SetMetadata(bool v) { return SetBool(v,settings.metadata,nullptr,false); }
bool SetLogging(bool v) { return SetBool(v,settings.log,&log_enabled,false); }
bool SetDebugLogging(bool v) { return SetBool(v,settings.debug_log,&debug_enabled,false); }
bool SetNormalizeArtistSeparators(bool v) { return SetBool(v,settings.normalize_artist_separators,nullptr,false); }
bool SetSkipDownloadedTracks(bool v) { return SetBool(v,settings.skip_downloaded_tracks,nullptr,false); }
bool SetSkipIgnoredTracks(bool v) { return SetBool(v,settings.skip_ignored_tracks,nullptr,false); }
bool SetEmbedCoverArt(bool v) { return SetBool(v,settings.embed_cover_art,nullptr,false); }
bool SetSaveCoverArt(bool v) { return SetBool(v,settings.save_cover_art,nullptr,false); }
bool SetEmbedLyrics(bool v) { return SetBool(v,settings.embed_lyrics,nullptr,false); }
bool SetSaveLyrics(bool v) { return SetBool(v,settings.save_lyrics,nullptr,false); }
bool SetSaveCanvas(bool v) { return SetBool(v,settings.save_canvas,nullptr,false); }
bool SetBlockTelemetry(bool v) { return SetBool(v,settings.block_telemetry,nullptr,false); }
bool SetLiftAddToQueue(bool v) { return SetBool(v,settings.lift_add_to_queue,nullptr,false); }
bool SetKeepNativeOriginal(bool v) { return SetBool(v,settings.keep_native_original,nullptr,false); }

static bool SetTextValue(const std::wstring& value,std::wstring& field,size_t limit=4095) {
    if(value.size()>limit)return false;
    AcquireSRWLockExclusive(&lock);
    bool ok=settings_signal!=nullptr;
    if(ok){field=value;++settings.generation;}
    ReleaseSRWLockExclusive(&lock);
    if(ok)SetEvent(settings_signal);
    return ok;
}
bool SetPlaybackSpeed(double value) {
    if(!std::isfinite(value) || value<1.0 || value>kMaxPlaybackSpeed)return false;
    AcquireSRWLockExclusive(&lock);
    bool ok=settings_signal!=nullptr;
    if(ok){settings.playback_speed=value;++settings.generation;}
    ReleaseSRWLockExclusive(&lock);
    if(ok)SetEvent(settings_signal);
    return ok;
}
bool SetPathTemplate(const std::wstring& value) { return SetTextValue(value,settings.path_template); }
bool SetPodcastTemplate(const std::wstring& value) { return SetTextValue(value,settings.podcast_template); }
bool SetCanvasTemplate(const std::wstring& value) { return SetTextValue(value,settings.canvas_template); }
bool SetOutputPreset(const std::wstring& value) { return SetTextValue(value,settings.output_preset,128); }
bool SetOutputExtension(const std::wstring& value) { return SetTextValue(value,settings.output_ext,32); }
bool SetOutputArguments(const std::wstring& value) { return SetTextValue(value,settings.output_args); }
bool SetFFmpegPath(const std::wstring& value) { return SetTextValue(value,settings.ffmpeg_path,2047); }
bool SetInvalidCharReplacement(const std::wstring& value) {
    if(value!=L"unicode" && value!=L"-" && value!=L"_" && !value.empty())return false;
    return SetTextValue(value,settings.invalid_char_repl,16);
}
bool SetSaveLocation(const std::wstring& root) {
    if(root.empty() || root.size()>2047 || !(root.size()>2 && (root[1]==L':' || root.rfind(L"\\\\",0)==0))) return false;
    DWORD attributes=GetFileAttributesW(WindowsPath(root).c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY)) return false;
    if(attributes==INVALID_FILE_ATTRIBUTES && GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND) return false;
    AcquireSRWLockExclusive(&lock);
    bool ok=settings_signal!=nullptr;
    if(ok) { settings.root=root; settings.save_location=root; settings.music_folder=IsMusic(root); ++settings.generation; }
    ReleaseSRWLockExclusive(&lock); if(ok)SetEvent(settings_signal); return ok;
}
static DWORD WINAPI Picker(LPVOID argument) {
    HWND owner=static_cast<HWND>(argument);
    HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(hr)) return 0;
    IFileOpenDialog* dialog=nullptr;
    hr=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_IFileOpenDialog,reinterpret_cast<void**>(&dialog));
    if(SUCCEEDED(hr)) {
        DWORD flags=0; dialog->GetOptions(&flags); dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"Soggfy — Save Location");
        auto current=GetSettings(); IShellItem* initial=nullptr;
        if(SUCCEEDED(SHCreateItemFromParsingName(current.root.c_str(),nullptr,IID_IShellItem,reinterpret_cast<void**>(&initial)))) {
            dialog->SetFolder(initial); initial->Release();
        }
        if(SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item=nullptr; PWSTR path=nullptr;
            if(SUCCEEDED(dialog->GetResult(&item))) {
                if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) {
                    if(!SetSaveLocation(path)) MessageBoxW(owner,L"The save location could not be saved.",L"Soggfy",MB_OK|MB_ICONERROR);
                    CoTaskMemFree(path);
                } item->Release();
            }
        } dialog->Release();
    }
    CoUninitialize(); return 0;
}
void PickSaveLocation(HWND owner) {
    HANDLE thread=CreateThread(nullptr,0,Picker,owner,0,nullptr); if(thread) CloseHandle(thread);
}
}
