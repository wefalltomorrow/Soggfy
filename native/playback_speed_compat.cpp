#include "playback_speed_compat.h"

namespace history {

bool PlaybackSpeedVersionSupported(std::uint16_t major,std::uint16_t minor,
                                   std::uint16_t patch,std::uint16_t build) noexcept {
    // The native track-player constructor hook was runtime-validated on
    // Spotify 1.3.1.234. Spotify 1.3.3.264 changed the constructor ABI/layout:
    // installing the old hook can redirect execution to a raw Spotify RVA
    // during startup. Keep capture usable at 1x and fail closed until each
    // new player ABI is explicitly validated.
    return major==1 && minor==3 && patch==1 && build==234;
}

}
