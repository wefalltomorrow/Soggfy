#pragma once
#include <windows.h>
#include <string>
namespace history {
struct Settings {
    bool downloads=false,ogg=true,flac=true,menu=true,music_folder=false;
    bool metadata=true,log=true,debug_log=false,normalize_artist_separators=true;
    unsigned max_buffered_mib=500,generation=0,capture_epoch=0;
    std::wstring root,save_location,path_template;
};
void InitSettings(HMODULE proxy);
Settings GetSettings();
bool SetDownloads(bool enabled);
bool SetOgg(bool enabled);
bool SetFlac(bool enabled);
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
