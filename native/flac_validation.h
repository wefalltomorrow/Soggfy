#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace history {
struct FlacDecodedCoverage {
    uint64_t samples=0,frames=0;
    unsigned rate=0,channels=0,bits=0;
};
// Test-only reference decoding. Production requires an isolated, bounded helper
// and a stable source handle before this can authorize any file transaction.
bool DecodeFlacRecording(const std::vector<uint8_t>& source,
    FlacDecodedCoverage& output,std::string& error,
    const std::function<bool()>& canceled={});
}
