#pragma once
#include <cstddef>
#include <cstdint>

namespace history {
// Untrusted declarations, checked against observed frame/byte extents by the
// layout inspector. They do not prove audio validity or listening coverage.
struct Mp3XingInfo {
    bool present=false;
    unsigned flags=0;
    uint32_t audio_frames=0,stream_bytes=0;
    bool gapless_present=false;
    uint16_t encoder_delay=0,encoder_padding=0;
    uint64_t gapless_samples=0; // Declared trim applied to observed audio frames; not a completeness claim.
};
namespace mp3_layout_detail {
// Input is one complete, bounded frame. Xing/Info uses the fixed offset after
// the four-byte header and nominal side-info size, even with MPEG protection.
// Encoder extensions (including delay/padding and their CRC) remain separate.
template<class Bytes>
bool ReadXing(const Bytes& bytes,size_t frame,size_t length,unsigned side,
              Mp3XingInfo& output,const char*& error) {
    output={};
    size_t at=4+side;
    auto match=[&](const char* word) {
        if(at>length||length-at<4)return false;
        for(unsigned i=0;i<4;++i)if(bytes[frame+at+i]!=uint8_t(word[i]))return false;
        return true;
    };
    if(!match("Xing")&&!match("Info"))return true;
    Mp3XingInfo found;found.present=true;at+=4;
    auto read=[&](uint32_t& value) {
        if(at>length||length-at<4)return false;
        value=0;
        for(unsigned i=0;i<4;++i)value=(value<<8)|bytes[frame+at++];
        return true;
    };
    uint32_t flags=0;
    if(!read(flags)){error="truncated MP3 Xing flags";return false;}
    if(flags&~15u){error="unsupported MP3 Xing flags";return false;}
    found.flags=flags;
    if(flags&1) {
        if(!read(found.audio_frames)){error="truncated MP3 Xing frame count";return false;}
        if(!found.audio_frames){error="zero MP3 Xing frame count";return false;}
    }
    if(flags&2) {
        if(!read(found.stream_bytes)){error="truncated MP3 Xing byte count";return false;}
        if(!found.stream_bytes){error="zero MP3 Xing byte count";return false;}
    }
    if(flags&4) {
        if(at>length||length-at<100){error="truncated MP3 Xing seek table";return false;}
        at+=100;
    }
    uint32_t quality=0;
    if((flags&8)&&!read(quality)){error="truncated MP3 Xing quality";return false;}
    // LAME-compatible encoder extensions place the 12-bit delay/padding pair
    // 21 bytes after their nine-byte version string. Treat it as an untrusted
    // declaration and require both a recognized prefix and bounded bytes.
    auto encoder=[&](const char* prefix) {
        for(unsigned i=0;i<4;++i)if(at+i>=length||bytes[frame+at+i]!=uint8_t(prefix[i]))return false;
        return true;
    };
    if(at<=length&&length-at>=24&&
       (encoder("LAME")||encoder("Lavf")||encoder("Lavc"))) {
        const unsigned first=bytes[frame+at+21];
        const unsigned second=bytes[frame+at+22];
        const unsigned third=bytes[frame+at+23];
        found.gapless_present=true;
        found.encoder_delay=uint16_t((first<<4)|(second>>4));
        found.encoder_padding=uint16_t(((second&15)<<8)|third);
    }
    output=found;return true;
}
}
}
