#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "playback_speed_limit.h"

namespace history {

inline std::uint64_t ClassicPcmSamplesToKeep(std::uint64_t produced,double speed) noexcept {
    if(!produced)return 0;
    if(!std::isfinite(speed)||speed<=1.0)return produced;
    if(speed>kMaxPlaybackSpeed)speed=kMaxPlaybackSpeed;
    return static_cast<std::uint64_t>(
        std::max<double>(1.0,std::floor(static_cast<double>(produced)/speed)));
}

}
