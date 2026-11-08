#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace history {
struct VorbisDecodedCoverage {
    uint64_t samples=0,packets=0;
    unsigned rate=0,channels=0;
};
// Decode an entire, single, zero-origin native Vorbis recording. No file I/O
// or publication. The owner must retain stable file identity independently.
bool DecodeVorbisRecording(const std::vector<uint8_t>& source,
    VorbisDecodedCoverage& output,std::string& error,
    const std::function<bool()>& canceled={});
}
