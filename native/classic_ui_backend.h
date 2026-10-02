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
void SetClassicCurrentIgnored(bool ignored);
bool ClassicCurrentIgnored();
}
