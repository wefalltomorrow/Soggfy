#pragma once
#include <cstdint>
namespace history {
// Private, versioned protocol between same-build validation processes. No paths,
// tags, variable-length messages or decoded PCM cross this boundary.
constexpr uint32_t ValidationMagic=0x31564753;
constexpr uint64_t ValidationInputLimit=256ull*1024*1024;
enum class ValidationFormat:uint32_t { Vorbis=1,Flac=2,Png=3,Jpeg=4 };
constexpr uint64_t ArtworkInputLimit=4ull*1024*1024;
inline bool ArtworkFormat(ValidationFormat format) {
    return format==ValidationFormat::Png||format==ValidationFormat::Jpeg;
}
struct ArtworkValidationReply {
    uint32_t magic=ValidationMagic,version=2,format=0;
    uint32_t width=0,height=0,depth=0,channels=0,progressive=0;
};
static_assert(sizeof(ArtworkValidationReply)==32,"fixed artwork wire layout");
inline bool ValidReply(const ArtworkValidationReply& r,ValidationFormat format) {
    return ArtworkFormat(format)&&r.magic==ValidationMagic&&r.version==2&&
        r.format==uint32_t(format)&&r.width&&r.width<=8192&&r.height&&r.height<=8192&&
        uint64_t(r.width)*r.height<=16ull*1024*1024&&r.channels>=1&&r.channels<=4&&
        (format==ValidationFormat::Jpeg?(r.depth==8&&r.channels!=2&&r.progressive<=1):
         (r.progressive==0&&(r.depth==1||r.depth==2||r.depth==4||r.depth==8||r.depth==16)&&
          (r.depth>=8||r.channels==1)));
}
struct ValidationReply {
    uint32_t magic=ValidationMagic,version=1;
    uint64_t samples=0,units=0;
    uint32_t format=0,rate=0,channels=0,bits=0;
};
static_assert(sizeof(ValidationReply)==40,"fixed validation wire layout");
inline bool ValidReply(const ValidationReply& r,ValidationFormat format) {
    return (format==ValidationFormat::Vorbis||format==ValidationFormat::Flac)&&
        r.magic==ValidationMagic&&r.version==1&&r.format==uint32_t(format)&&
        r.samples&&r.units&&r.rate>=8000&&r.rate<=655350&&r.channels>=1&&
        r.channels<=8&&(format==ValidationFormat::Flac?(r.bits>=4&&r.bits<=32):r.bits==0);
}
}
