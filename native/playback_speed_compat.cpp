#include "playback_speed_compat.h"

namespace history {

PlaybackSpeedBackend PlaybackSpeedBackendForVersion(std::uint16_t major,std::uint16_t minor,
                                                    std::uint16_t patch,std::uint16_t build) noexcept {
    if(major==1 && minor==3 && patch==1 && build==234)
        return PlaybackSpeedBackend::ConstructorHook;
    if(major==1 && minor==3 && patch==3 && build==264)
        return PlaybackSpeedBackend::SessionPlayer133;
    return PlaybackSpeedBackend::Unsupported;
}

bool PlaybackSpeedVersionSupported(std::uint16_t major,std::uint16_t minor,
                                   std::uint16_t patch,std::uint16_t build) noexcept {
    return PlaybackSpeedBackendForVersion(major,minor,patch,build)!=PlaybackSpeedBackend::Unsupported;
}

}
