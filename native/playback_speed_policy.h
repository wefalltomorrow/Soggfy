#pragma once
#include <algorithm>
#include <cmath>
#include "playback_speed_limit.h"

namespace history {

// Match Rafiuth/Soggfy StateManager::GetPlaySpeed(): the saved slider value
// is a preference, not the speed while the downloader is disabled.
inline double ClassicEffectiveSpeed(bool downloads,double configured,bool supported=true) noexcept {
    if(!downloads||!supported||!std::isfinite(configured)||configured<1.0)return 1.0;
    // Clamp older persisted preferences and all internal callers.
    return std::min(configured,kMaxPlaybackSpeed);
}

} // namespace history
