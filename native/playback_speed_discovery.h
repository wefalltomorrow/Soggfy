#pragma once
#include <cstddef>
#include <cstdint>

namespace history {
enum class PlaybackDiscoveryResult : unsigned char {
    Found,
    InvalidImage,
    MissingTarget,
    AmbiguousTarget,
    InvalidTarget,
};

PlaybackDiscoveryResult DiscoverPlaybackSpeedTarget(const std::uint8_t* mapped_image,
                                                    std::size_t available,
                                                    std::uint32_t& target_rva) noexcept;
}
