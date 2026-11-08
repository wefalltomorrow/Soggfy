#pragma once
#include <cstdint>
namespace history {
// Private, versioned protocol between same-build validation processes. No paths,
// tags, variable-length messages or decoded PCM cross this boundary.
constexpr uint32_t ValidationMagic=0x31564753;
constexpr uint64_t ValidationInputLimit=256ull*1024*1024;
enum class ValidationFormat:uint32_t { Vorbis=1,Flac=2 };
struct ValidationReply {
    uint32_t magic=ValidationMagic,version=1;
    uint64_t samples=0,units=0;
    uint32_t format=0,rate=0,channels=0,bits=0;
};
static_assert(sizeof(ValidationReply)==40,"fixed validation wire layout");
inline bool ValidReply(const ValidationReply& r,ValidationFormat format) {
    return r.magic==ValidationMagic&&r.version==1&&r.format==uint32_t(format)&&
        r.samples&&r.units&&r.rate>=8000&&r.rate<=655350&&r.channels>=1&&
        r.channels<=8&&(format==ValidationFormat::Flac?(r.bits>=4&&r.bits<=32):r.bits==0);
}
}
