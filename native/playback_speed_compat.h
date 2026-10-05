#pragma once
#include <cstdint>

namespace history {

enum class PlaybackSpeedBackend : unsigned char {
    Unsupported,
    ConstructorHook,
    TrackCreate133,
    SessionPlayer133,
    ContextSetter133
};

PlaybackSpeedBackend PlaybackSpeedBackendForVersion(std::uint16_t major,std::uint16_t minor,
                                                    std::uint16_t patch,std::uint16_t build) noexcept;
bool PlaybackSpeedVersionSupported(std::uint16_t major,std::uint16_t minor,
                                   std::uint16_t patch,std::uint16_t build) noexcept;

}
