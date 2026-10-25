#pragma once
#include "media_session.h"
#include <string>
#include <cstdint>
#include <vector>
#include <functional>

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
struct ClassicStatusControl {
    std::function<bool()> canceled;
    unsigned timeout_ms=7000,index_lifetime_ms=10000;
    size_t max_files=100000,max_directories=10000;
    unsigned max_depth=12;
};
unsigned ClassicStatusRevision();
std::vector<ClassicTrackResult> QueryClassicTrackStatuses(const std::vector<ClassicTrackQuery>& queries,ClassicStatusMetrics* metrics=nullptr,
                                                        const ClassicStatusControl& control={});
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
