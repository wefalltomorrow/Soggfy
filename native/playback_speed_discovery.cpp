#include "playback_speed_discovery.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace history {
namespace {
constexpr std::uint32_t kExecutable=0x20000000;
constexpr std::size_t kSectionSize=40;
constexpr std::size_t kRuntimeFunctionSize=12;

template<class T>
bool Read(const std::uint8_t* image,std::size_t size,std::size_t offset,T& value) noexcept {
    if(offset>size||sizeof(T)>size-offset)return false;
    std::memcpy(&value,image+offset,sizeof(T));
    return true;
}
bool Range(std::size_t size,std::size_t offset,std::size_t length) noexcept {
    return offset<=size&&length<=size-offset;
}

struct Image {
    const std::uint8_t* base=nullptr;
    std::size_t available=0,sections=0;
    std::uint16_t section_count=0;
    std::uint32_t image_size=0,exceptions=0,exception_size=0;
};

bool Parse(const std::uint8_t* base,std::size_t available,Image& image) noexcept {
    if(!base||available<0x200)return false;
    std::uint16_t dos=0,machine=0,optional_size=0,magic=0;
    std::uint32_t nt=0,signature=0,directory_count=0;
    if(!Read(base,available,0,dos)||dos!=0x5a4d||
       !Read(base,available,0x3c,nt)||nt>0x100000||
       !Read(base,available,nt,signature)||signature!=0x00004550||
       !Read(base,available,nt+4,machine)||machine!=0x8664||
       !Read(base,available,nt+6,image.section_count)||!image.section_count||
       !Read(base,available,nt+20,optional_size)||optional_size<240||
       !Read(base,available,nt+24,magic)||magic!=0x20b||
       !Read(base,available,nt+24+56,image.image_size)||!image.image_size||
       image.image_size>available||
       !Read(base,available,nt+24+108,directory_count)||directory_count<=3||
       !Read(base,available,nt+24+112+3*8,image.exceptions)||
       !Read(base,available,nt+24+112+3*8+4,image.exception_size))
        return false;
    image.sections=std::size_t(nt)+24+optional_size;
    image.base=base;image.available=available;
    return Range(available,image.sections,std::size_t(image.section_count)*kSectionSize)&&
           image.exception_size>=kRuntimeFunctionSize&&
           image.exception_size%kRuntimeFunctionSize==0&&
           Range(image.image_size,image.exceptions,image.exception_size);
}

bool Executable(const Image& image,std::uint32_t rva,std::size_t length=1) noexcept {
    const std::uint64_t finish=std::uint64_t(rva)+length;
    for(std::uint16_t index=0;index<image.section_count;++index) {
        const auto section=image.sections+std::size_t(index)*kSectionSize;
        std::uint32_t virtual_size=0,virtual_address=0,raw_size=0,flags=0;
        if(!Read(image.base,image.available,section+8,virtual_size)||
           !Read(image.base,image.available,section+12,virtual_address)||
           !Read(image.base,image.available,section+16,raw_size)||
           !Read(image.base,image.available,section+36,flags))return false;
        const std::uint64_t end=std::uint64_t(virtual_address)+std::max(virtual_size,raw_size);
        if((flags&kExecutable)&&rva>=virtual_address&&finish<=end&&finish<=image.image_size)return true;
    }
    return false;
}

bool RuntimeFunction(const Image& image,std::uint32_t rva,std::uint32_t& begin,std::uint32_t& end) noexcept {
    const std::size_t count=image.exception_size/kRuntimeFunctionSize;
    std::size_t low=0,high=count;
    while(low<high) {
        const auto middle=low+(high-low)/2;
        std::uint32_t candidate=0;
        if(!Read(image.base,image.available,image.exceptions+middle*kRuntimeFunctionSize,candidate))return false;
        if(candidate<=rva)low=middle+1;else high=middle;
    }
    if(!low)return false;
    const auto entry=image.exceptions+(low-1)*kRuntimeFunctionSize;
    if(!Read(image.base,image.available,entry,begin)||
       !Read(image.base,image.available,entry+4,end)||
       begin>=end||rva<begin||rva>=end||!Executable(image,begin,end-begin))return false;
    return true;
}

template<std::size_t N>
void AddMaskedCallTargets(const Image& image,const std::array<int,N>& pattern,
                          std::vector<std::uint32_t>& targets) noexcept {
    for(std::uint16_t index=0;index<image.section_count;++index) {
        const auto section=image.sections+std::size_t(index)*kSectionSize;
        std::uint32_t virtual_size=0,virtual_address=0,raw_size=0,flags=0;
        if(!Read(image.base,image.available,section+8,virtual_size)||
           !Read(image.base,image.available,section+12,virtual_address)||
           !Read(image.base,image.available,section+16,raw_size)||
           !Read(image.base,image.available,section+36,flags))return;
        if(!(flags&kExecutable)||virtual_address>=image.image_size)continue;
        const auto extent=std::min<std::size_t>(std::max(virtual_size,raw_size),image.image_size-virtual_address);
        if(extent<N)continue;
        for(std::size_t at=0;at+N<=extent;++at) {
            bool match=true;
            for(std::size_t p=0;p<N;++p)if(pattern[p]>=0&&image.base[virtual_address+at+p]!=std::uint8_t(pattern[p])){match=false;break;}
            if(!match)continue;
            const std::uint32_t call_rva=virtual_address+std::uint32_t(at);
            std::int32_t rel=0;
            if(!Read(image.base,image.available,call_rva+1,rel))continue;
            const std::int64_t destination=std::int64_t(call_rva)+5+rel;
            if(destination<0||destination>=image.image_size)continue;
            const auto target=std::uint32_t(destination);
            std::uint32_t begin=0,end=0;
            if(!RuntimeFunction(image,target,begin,end)||begin!=target)continue;
            const auto size=end-begin;
            if(size<0x80||size>0x5000)continue;
            if(std::find(targets.begin(),targets.end(),target)==targets.end())targets.push_back(target);
        }
    }
}

template<std::size_t N>
void AddInternalTargets(const Image& image,const std::array<std::uint8_t,N>& pattern,
                        std::vector<std::uint32_t>& targets) noexcept {
    for(std::uint16_t index=0;index<image.section_count;++index) {
        const auto section=image.sections+std::size_t(index)*kSectionSize;
        std::uint32_t virtual_size=0,virtual_address=0,raw_size=0,flags=0;
        if(!Read(image.base,image.available,section+8,virtual_size)||
           !Read(image.base,image.available,section+12,virtual_address)||
           !Read(image.base,image.available,section+16,raw_size)||
           !Read(image.base,image.available,section+36,flags))return;
        if(!(flags&kExecutable)||virtual_address>=image.image_size)continue;
        const auto extent=std::min<std::size_t>(std::max(virtual_size,raw_size),image.image_size-virtual_address);
        if(extent<N)continue;
        const auto* start=image.base+virtual_address;
        for(std::size_t at=0;at+N<=extent;++at) {
            if(std::memcmp(start+at,pattern.data(),N))continue;
            std::uint32_t begin=0,end=0;
            if(!RuntimeFunction(image,virtual_address+std::uint32_t(at),begin,end))continue;
            const auto size=end-begin;
            // This player-construction routine is large; reject small helpers
            // that happen to contain the same virtual-call sequence.
            if(size<0x800||size>0x6000)continue;
            if(std::find(targets.begin(),targets.end(),begin)==targets.end())targets.push_back(begin);
        }
    }
}
}

PlaybackDiscoveryResult DiscoverPlaybackSpeedTarget(const std::uint8_t* mapped_image,
                                                    std::size_t available,
                                                    std::uint32_t& target_rva) noexcept {
    target_rva=0;
    Image image{};
    if(!Parse(mapped_image,available,image))return PlaybackDiscoveryResult::InvalidImage;

    // Modern Spotify.dll (2026): direct call to the track-player constructor.
    // Wildcards are -1. The fourth parameter of the resolved function is the
    // native playback-speed double used by Spotify itself.
    constexpr std::array<int,13> modern_call={
        0xE8,-1,-1,-1,-1,0x90,0x48,0x8B,0x45,-1,0x4C,0x89,0x7D
    };
    std::vector<std::uint32_t> modern;
    AddMaskedCallTargets(image,modern_call,modern);

    // Independent older x64 anchors inside the same constructor. They are only
    // accepted when the enclosing PE runtime function is large and unique.
    constexpr std::array<std::uint8_t,12> legacy_a={
        0x01,0x49,0x8B,0x0F,0x48,0x8B,0x01,0xFF,0x50,0x38,0x48,0x8D
    };
    constexpr std::array<std::uint8_t,13> legacy_b={
        0x01,0x49,0x8B,0x0C,0x24,0x48,0x8B,0x01,0xFF,0x50,0x38,0x48,0x8D
    };
    std::vector<std::uint32_t> legacy;
    AddInternalTargets(image,legacy_a,legacy);
    AddInternalTargets(image,legacy_b,legacy);

    if(modern.size()>1||legacy.size()>1)return PlaybackDiscoveryResult::AmbiguousTarget;
    if(!modern.empty()&&!legacy.empty()&&modern.front()!=legacy.front())
        return PlaybackDiscoveryResult::AmbiguousTarget;

    const auto target=!modern.empty()?modern.front():(!legacy.empty()?legacy.front():0);
    if(!target)return PlaybackDiscoveryResult::MissingTarget;
    if(!Executable(image,target,16))return PlaybackDiscoveryResult::InvalidTarget;
    target_rva=target;
    return PlaybackDiscoveryResult::Found;
}
}
