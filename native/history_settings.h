#pragma once
#include <windows.h>
#include <string>
namespace history {
struct Settings {
    bool downloads=false,ogg=true,flac=true,menu=false,classic_ui=true,music_folder=false;
    bool metadata=true,log=true,debug_log=false,normalize_artist_separators=true;
    unsigned max_buffered_mib=500,generation=0,capture_epoch=0;
    std::wstring root,save_location,path_template;
};
void InitSettings(HMODULE proxy);
Settings GetSettings();
bool SetDownloads(bool enabled);
bool SetOgg(bool enabled);
bool SetFlac(bool enabled);
bool SetMetadata(bool enabled);
bool SetLogging(bool enabled);
bool SetDebugLogging(bool enabled);
bool SetNormalizeArtistSeparators(bool enabled);
bool SetPathTemplate(const std::wstring& value);
bool SetSaveLocation(const std::wstring& root);
bool OggEnabled();
bool FlacEnabled();
unsigned CaptureEpoch();
bool EnsureDirectory(const std::wstring& path);
std::wstring WindowsPath(const std::wstring& path);
void PickSaveLocation(HWND owner);
bool LoggingEnabled();
bool DebugLoggingEnabled();
bool FlushSettings(unsigned timeout_ms);
void HistoryLog(const char* message);
}
