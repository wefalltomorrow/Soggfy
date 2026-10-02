#pragma once
#include <windows.h>
#include <string>
namespace history {
struct Settings {
    bool downloads=false,ogg=true,flac=true,menu=false,classic_ui=true,music_folder=false;
    bool metadata=true,log=true,debug_log=false,normalize_artist_separators=true;

    // Classic Soggfy behaviour/settings.
    double playback_speed=1.0;
    bool skip_downloaded_tracks=false,skip_ignored_tracks=false;
    bool embed_cover_art=true,save_cover_art=true;
    bool embed_lyrics=true,save_lyrics=true,save_canvas=false;
    bool block_telemetry=true,lift_add_to_queue=false;
    bool keep_native_original=true;

    unsigned max_buffered_mib=500,generation=0,capture_epoch=0;
    std::wstring root,save_location,path_template;
    std::wstring podcast_template,canvas_template,invalid_char_repl=L"unicode";
    std::wstring output_preset=L"Native",output_ext,output_args,ffmpeg_path;
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

bool SetSkipDownloadedTracks(bool enabled);
bool SetSkipIgnoredTracks(bool enabled);
bool SetEmbedCoverArt(bool enabled);
bool SetSaveCoverArt(bool enabled);
bool SetEmbedLyrics(bool enabled);
bool SetSaveLyrics(bool enabled);
bool SetSaveCanvas(bool enabled);
bool SetBlockTelemetry(bool enabled);
bool SetLiftAddToQueue(bool enabled);
bool SetKeepNativeOriginal(bool enabled);

bool SetPlaybackSpeed(double value);
bool SetPathTemplate(const std::wstring& value);
bool SetPodcastTemplate(const std::wstring& value);
bool SetCanvasTemplate(const std::wstring& value);
bool SetInvalidCharReplacement(const std::wstring& value);
bool SetOutputPreset(const std::wstring& value);
bool SetOutputExtension(const std::wstring& value);
bool SetOutputArguments(const std::wstring& value);
bool SetFFmpegPath(const std::wstring& value);
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
