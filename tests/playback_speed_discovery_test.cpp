#include "../native/playback_speed_discovery.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using namespace history;

template<class T>
static void Put(std::vector<std::uint8_t>& image,std::size_t offset,T value){
    assert(offset+sizeof(value)<=image.size());
    std::memcpy(image.data()+offset,&value,sizeof(value));
}
static std::vector<std::uint8_t> Image(bool duplicate=false,bool old_anchor=false){
    std::vector<std::uint8_t> image(0x20000);
    Put<std::uint16_t>(image,0,0x5a4d);Put<std::uint32_t>(image,0x3c,0x80);
    Put<std::uint32_t>(image,0x80,0x00004550);Put<std::uint16_t>(image,0x84,0x8664);
    Put<std::uint16_t>(image,0x86,2);Put<std::uint16_t>(image,0x94,240);
    Put<std::uint16_t>(image,0x98,0x20b);Put<std::uint32_t>(image,0x98+56,std::uint32_t(image.size()));
    Put<std::uint32_t>(image,0x98+108,16);Put<std::uint32_t>(image,0x98+112+3*8,0x1000);
    const auto sections=std::size_t(0x98+240);
    Put<std::uint32_t>(image,sections+8,0x1000);Put<std::uint32_t>(image,sections+12,0x1000);
    Put<std::uint32_t>(image,sections+16,0x1000);Put<std::uint32_t>(image,sections+36,0x40000040);
    Put<std::uint32_t>(image,sections+40+8,0x1d000);Put<std::uint32_t>(image,sections+40+12,0x2000);
    Put<std::uint32_t>(image,sections+40+16,0x1d000);Put<std::uint32_t>(image,sections+40+36,0x60000020);

    struct F{std::uint32_t b,e;};std::vector<F> funcs={{0x4000,0x5400}};
    if(duplicate)funcs.push_back({0x7000,0x8400});
    Put<std::uint32_t>(image,0x98+112+3*8+4,std::uint32_t(funcs.size()*12));
    for(size_t i=0;i<funcs.size();++i){
        Put<std::uint32_t>(image,0x1000+i*12,funcs[i].b);
        Put<std::uint32_t>(image,0x1000+i*12+4,funcs[i].e);
        Put<std::uint32_t>(image,0x1000+i*12+8,0x1800);
    }
    const auto call=[&](std::uint32_t at,std::uint32_t target){
        image[at]=0xE8;auto rel=std::int32_t(target-(at+5));Put<std::int32_t>(image,at+1,rel);
        const std::uint8_t tail[]={0x90,0x48,0x8B,0x45,0x20,0x4C,0x89,0x7D};
        std::memcpy(image.data()+at+5,tail,sizeof(tail));
    };
    call(0x3000,0x4000);
    if(duplicate)call(0x3100,0x7000);
    if(old_anchor){
        const std::uint8_t anchor[]={0x01,0x49,0x8B,0x0C,0x24,0x48,0x8B,0x01,0xFF,0x50,0x38,0x48,0x8D};
        std::memcpy(image.data()+0x4800,anchor,sizeof(anchor));
    }
    return image;
}
int main(){
    std::uint32_t target=0;
    auto image=Image(false,true);
    assert(DiscoverPlaybackSpeedTarget(image.data(),image.size(),target)==PlaybackDiscoveryResult::Found);
    assert(target==0x4000);

    image=Image(false,false);
    assert(DiscoverPlaybackSpeedTarget(image.data(),image.size(),target)==PlaybackDiscoveryResult::Found);
    assert(target==0x4000);

    image=Image(true,false);
    assert(DiscoverPlaybackSpeedTarget(image.data(),image.size(),target)==PlaybackDiscoveryResult::AmbiguousTarget);

    image=Image(false,false);image[0x3000]=0x90;
    assert(DiscoverPlaybackSpeedTarget(image.data(),image.size(),target)==PlaybackDiscoveryResult::MissingTarget);
}
