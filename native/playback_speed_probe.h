#pragma once
#include <cstdint>

namespace history {

// Atomically sampled counters from Spotify's decoder boundary.
// A snapshot is deliberately approximate: independent relaxed atomics are
// sufficient for diagnosis, and the decoder hot path must never take a lock.
struct DecodeProbeSnapshot {
    std::uint64_t enters=0, exits=0, last_enter_ms=0, last_exit_ms=0;
    std::uint64_t decoded_samples=0, reported_samples=0, compressed_units=0;
    std::uint64_t no_pcm_calls=0, no_input_change_calls=0, invalid_output_calls=0;
    std::uint64_t last_capacity=0, last_produced=0, last_kept=0;
    std::uint64_t last_encoded_before=0, last_encoded_after=0, last_flags=0;
};

// These classifications describe observations, NOT root cause:
// "no_new_calls" cannot distinguish a fetch stall from scheduler starvation.
inline const char* DescribeDecodeProbe(const DecodeProbeSnapshot& old,
                                        const DecodeProbeSnapshot& now,
                                        std::uint64_t wall_ms) noexcept {
    if(now.enters==0 && now.exits==0)return "no_calls_observed";
    if(now.enters>now.exits && now.last_enter_ms &&
       wall_ms>=now.last_enter_ms+5000 &&
       now.last_enter_ms>=now.last_exit_ms)
        return "call_unreturned_5s";
    if(now.enters==old.enters)return "no_new_calls";
    if(now.exits==old.exits)return "entered_no_return";
    if(now.decoded_samples==old.decoded_samples)return "calls_no_pcm";
    if(now.compressed_units==old.compressed_units)return "pcm_no_reported_input_delta";
    return "decode_progress";
}
}
