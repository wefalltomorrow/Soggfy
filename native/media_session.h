#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "media_position_core.h"
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
std::string Utf8(const std::wstring& s);
std::string Json(const std::string& s);
class MediaReader {
    friend class MediaSnapshotReader;
    void* manager_ = nullptr;
    bool initialized_ = false;
    Media cached_;
    MediaPositionClock position_clock_;
public:
    ~MediaReader();
    // Blocking COM/WinRT boundary. Use MediaSnapshotReader from capture.
    bool Read(Media& out,bool include_artwork=true,double playback_rate=1.0);
    bool Read(Media& out,double& observed_at,bool include_artwork,double playback_rate);
    void ResetPosition() { position_clock_={}; }
};
enum class MediaRead { Pending,Ready,Failed };
struct MediaReadOptions {
    unsigned generation=0,capture_epoch=0;
    double playback_rate=1.0;
    bool artwork=false;
    bool operator==(const MediaReadOptions& other) const {
        return generation==other.generation && capture_epoch==other.capture_epoch &&
            playback_rate==other.playback_rate && artwork==other.artwork;
    }
};
class MediaSnapshotReader {
public:
    // Test/provider context must outlive outstanding work, even after Stop.
    using Fetch=bool(*)(Media&,double&,const MediaReadOptions&,void*);
    explicit MediaSnapshotReader(Fetch fetch=nullptr,void* context=nullptr) noexcept;
    ~MediaSnapshotReader();
    MediaSnapshotReader(const MediaSnapshotReader&)=delete;
    MediaSnapshotReader& operator=(const MediaSnapshotReader&)=delete;
    // One consumer owns Start/TryRead/Stop. Stop never waits on Windows.
    // A stopping observer must finish before Start can create another.
    bool Start(const MediaReadOptions&) noexcept;
    void Stop() noexcept;
    bool IsStopped() const noexcept;
    // RAM-only try-lock. Configuration revisions reject in-flight old reads,
    // including A->B->A changes. A sequence is consumed at most once.
    // Failed leaves Media untouched; missing/Pending data still needs the
    // consumer's three-second coverage timeout. Keep sequence across restarts.
    MediaRead TryRead(Media&,double& observed_at,uint64_t& sequence,
                      const MediaReadOptions&) noexcept;
private:
    struct State;
    std::shared_ptr<State> state_;
    Fetch fetch_;
    void* context_;
    static DWORD WINAPI Run(void*);
};
}
