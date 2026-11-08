#pragma once
#include <cstddef>
#include <cstdint>

namespace history {
// Structural information only. Frame boundaries and nominal sample counts do
// not establish valid audio or a complete recording. A file truncated exactly
// at a frame boundary can pass. Decode, reservoir, gapless timing, provenance
// and listening/lifecycle checks remain separate requirements for publication.
struct Mp3Layout {
    unsigned rate=0,channels=0,frames=0;
    uint64_t nominal_samples=0;
    size_t audio_offset=0,audio_size=0;
};
namespace mp3_layout_detail {
struct View {
    const uint8_t* data;size_t count;
    size_t size()const noexcept{return count;}
    uint8_t operator[](size_t at)const noexcept{return data[at];}
};
template<class Bytes>
bool Match(const Bytes& b,size_t at,const char* text,size_t length) {
    if(at>b.size()||length>b.size()-at)return false;
    for(size_t i=0;i<length;++i)if(b[at+i]!=uint8_t(text[i]))return false;
    return true;
}
}
// Works directly on fixed-block CompressedBuffer and contiguous byte vectors.
// No allocations, resynchronization, tolerance for junk, or partial results.
// Callers must keep the input immutable throughout inspection.
template<class Bytes>
bool InspectMp3Layout(const Bytes& bytes,Mp3Layout& output,const char*& error) {
    output={};error=nullptr;Mp3Layout found;
    auto fail=[&](const char* message){error=message;return false;};
    const auto size=bytes.size();
    if(size<4||size>256ull*1024*1024)return fail("MP3 input size rejected");
    size_t at=0,end=size;
    using mp3_layout_detail::Match;
    if(Match(bytes,0,"ID3",3)) {
        if(size<10)return fail("truncated ID3 header");
        const unsigned version=bytes[3],flags=bytes[5];
        if((version!=3&&version!=4)||bytes[4]||
           (flags&~(version==3?0xe0u:0xf0u)))return fail("unsupported ID3 header");
        size_t payload=0;
        for(size_t i=6;i<10;++i) {
            if(bytes[i]&128)return fail("invalid ID3 synchsafe size");
            payload=(payload<<7)|bytes[i];
        }
        if(payload>16u*1024*1024||payload>size-10)return fail("ID3 extent rejected");
        at=10+payload;
        if(version==4&&(flags&16)) {
            if(!Match(bytes,at,"3DI",3)||size-at<10)return fail("invalid ID3 footer");
            for(size_t i=3;i<10;++i)if(bytes[at+i]!=bytes[i])return fail("ID3 footer disagrees with header");
            at+=10;
        }
    }
    if(end-at>=128&&Match(bytes,end-128,"TAG",3))end-=128;
    found.audio_offset=at;
    while(at<end) {
        if(end-at<4)return fail("truncated MP3 header");
        const uint32_t h=(uint32_t(bytes[at])<<24)|(uint32_t(bytes[at+1])<<16)|
            (uint32_t(bytes[at+2])<<8)|bytes[at+3];
        const unsigned version=(h>>19)&3,layer=(h>>17)&3,index=(h>>12)&15,rate_index=(h>>10)&3;
        if((h&0xffe00000u)!=0xffe00000u||version==1||layer!=1||!index||index==15||
           rate_index==3||(h&3)==2)return fail("unsupported MP3 frame header");
        static constexpr unsigned rates[]={44100,48000,32000};
        static constexpr unsigned high[]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
        static constexpr unsigned low[]={0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};
        const unsigned rate=rates[rate_index]/(version==3?1:version==2?2:4);
        const unsigned channels=((h>>6)&3)==3?1:2;
        const size_t length=(version==3?144000u:72000u)*(version==3?high:low)[index]/rate+((h>>9)&1);
        const unsigned side_info=version==3?(channels==1?17:32):(channels==1?9:17);
        if(length<4+side_info+((h&(1u<<16))?0:2)||length>end-at)
            return fail("truncated MP3 frame");
        if(!found.frames){found.rate=rate;found.channels=channels;}
        else if(found.rate!=rate||found.channels!=channels)return fail("MP3 format changed");
        if(found.frames==1000000)return fail("MP3 frame limit exceeded");
        ++found.frames;found.nominal_samples+=version==3?1152:576;
        if(found.nominal_samples>uint64_t(rate)*86400)return fail("MP3 duration limit exceeded");
        at+=length;
    }
    if(!found.frames)return fail("MP3 has no frames");
    found.audio_size=end-found.audio_offset;output=found;return true;
}
inline bool InspectMp3Layout(const uint8_t* bytes,size_t size,Mp3Layout& output,const char*& error) {
    if(!bytes){output={};error="null MP3 input";return false;}
    return InspectMp3Layout(mp3_layout_detail::View{bytes,size},output,error);
}
}
