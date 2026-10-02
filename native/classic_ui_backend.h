#pragma once
#include "media_session.h"
#include <string>
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
std::vector<ClassicTrackResult> QueryClassicTrackStatuses(const std::vector<ClassicTrackQuery>& queries);
bool RevealClassicTrack(const std::wstring& path);

struct ClassicM3UEntry {
    long duration_seconds=0;
    std::string artist,title;
    std::wstring path;
};
bool SaveClassicM3U(const std::wstring& suggested_filename,const std::string& playlist_name,
                    const std::vector<ClassicM3UEntry>& entries);

void SetClassicCurrentIgnored(bool ignored);
bool ClassicCurrentIgnored();
}
