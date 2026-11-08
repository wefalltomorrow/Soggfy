#pragma once
#include <string>
#include <vector>

namespace history {
struct Media {
    std::wstring title, artist, album, album_artist, app, genre;
    unsigned track = 0;
    double position = 0, duration = 0;
    double raw_position = 0, timeline_age = 0;
    bool playing = false;
    std::vector<unsigned char> cover;
    std::wstring cover_extension;
    std::string Key() const;
};
}
