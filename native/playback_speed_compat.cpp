#include "playback_speed_compat.h"

namespace history {

bool PlaybackSpeedVersionSupported(std::uint16_t major,std::uint16_t minor,
                                   std::uint16_t patch,std::uint16_t build) noexcept {
    // RC46 validates the live snd-decoder PCM dispatcher only on Spotify
    // 1.3.1.234.g59d6bf59 x64. Fail closed on every other build until its
    // decoder ABI/layout has been independently verified.
    return major==1 && minor==3 && patch==1 && build==234;
}

}
