#pragma once
#include <cstdint>

namespace history {
bool PlaybackSpeedVersionSupported(std::uint16_t major,std::uint16_t minor,
                                   std::uint16_t patch,std::uint16_t build) noexcept;
}
