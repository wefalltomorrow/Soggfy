#pragma once
#include "media_snapshot.h"
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>

namespace history {
struct MetadataInstance {
    std::string_view key;
    uint64_t generation=0;
    bool exact=false;
};

// Only corroborated client URI/playback-ID observations may refresh a snapshot.
// A matching title or a provisional legacy repeat is insufficient. Unknown
// optional fields do not erase fields already observed for this exact listen.
template<class Observation>
inline bool RefreshListenMetadata(Media& saved,Observation&& incoming,
    MetadataInstance before,MetadataInstance after,bool completed=false) {
    if(!before.exact || !after.exact || before.key.empty() ||
       before.key!=after.key || !before.generation || before.generation!=after.generation ||
       incoming.title.empty() || !std::isfinite(saved.duration) ||
       !std::isfinite(incoming.duration) || saved.duration<=0 || incoming.duration<=0 ||
       std::fabs(saved.duration-incoming.duration)>1) return false;
    Media next=std::forward<Observation>(incoming);
    if(next.artist.empty())next.artist=saved.artist;
    if(next.album.empty())next.album=saved.album;
    if(next.album_artist.empty())next.album_artist=saved.album_artist;
    if(next.genre.empty())next.genre=saved.genre;
    if(!next.track)next.track=saved.track;
    if(next.app.empty())next.app=saved.app;
    if(next.cover.empty() || next.cover_extension.empty()) {
        next.cover=saved.cover;next.cover_extension=saved.cover_extension;
    }
    // Completion time/coverage belong to the finished listen, not later polls.
    if(completed) {
        next.duration=saved.duration;next.position=saved.position;
        next.raw_position=saved.raw_position;next.timeline_age=saved.timeline_age;
        next.playing=saved.playing;
    }
    saved=std::move(next);return true;
}
}
