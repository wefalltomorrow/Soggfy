#pragma once
#include "media_session.h"
#include <string>
#include <cstdint>
#include <vector>

namespace history {
struct ClassicTrackQuery {
    std::string uri;
    std::wstring title,artist,album,all_artists;
};
struct ClassicTrackResult {
    std::string uri,status,message;
    std::wstring path;
};

void SetClassicTrackStatus(const Media& media,const char* status,const std::string& message={},
                           const std::wstring& path={});
struct ClassicStatusMetrics {
    uint64_t scans=0,compilations=0,regex_checks=0,cache_hits=0;
    uint64_t scan_ms=0,compile_ms=0,match_ms=0,elapsed_ms=0;
};
unsigned ClassicStatusRevision();
std::vector<ClassicTrackResult> QueryClassicTrackStatuses(const std::vector<ClassicTrackQuery>& queries,ClassicStatusMetrics* metrics=nullptr);
bool RevealClassicTrack(const std::wstring& path);

struct ClassicM3UEntry {
    long duration_seconds=0;
    std::string artist,title;
    std::wstring path;
};
bool SaveClassicM3U(const std::wstring& suggested_filename,const std::string& playlist_name,
                    const std::vector<ClassicM3UEntry>& entries);
bool QueueClassicCanvasDownload(const std::wstring& url,const std::wstring& title,
                                const std::wstring& artist,const std::wstring& album,
                                unsigned track);

void SetClassicCurrentIgnored(bool ignored);
bool ClassicCurrentIgnored();
}
