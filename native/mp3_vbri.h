#pragma once
#include <cstddef>
#include <cstdint>

namespace history {
// VBRI declarations are untrusted metadata. The layout inspector checks the
// fixed header, table extent, and declared stream counts; these values do not
// prove valid audio, complete playback, or trustworthy gapless timing.
struct Mp3VbriInfo {
    bool present=false;
    uint16_t version=0,delay=0,quality=0,toc_entries=0,toc_scale=0;
    uint16_t toc_entry_bytes=0,frames_per_entry=0;
    uint32_t audio_frames=0,stream_bytes=0;
};
namespace mp3_layout_detail {
// FFmpeg's MP3 demuxer checks VBRI at a fixed offset 4+32 from the frame
// start, independent of MPEG version, channel count, and CRC protection.
template<class Bytes>
bool ReadVbri(const Bytes& bytes,size_t frame,size_t length,
              Mp3VbriInfo& output,const char*& error) {
    output={};
    constexpr size_t marker=4+32;
    auto match=[&] {
        if(marker>length||length-marker<4)return false;
        return bytes[frame+marker]=='V'&&bytes[frame+marker+1]=='B'&&
               bytes[frame+marker+2]=='R'&&bytes[frame+marker+3]=='I';
    };
    if(!match())return true;
    Mp3VbriInfo found;found.present=true;
    size_t at=marker+4;
    auto read16=[&](uint16_t& value) {
        if(at>length||length-at<2)return false;
        value=uint16_t((uint16_t(bytes[frame+at])<<8)|bytes[frame+at+1]);
        at+=2;return true;
    };
    auto read32=[&](uint32_t& value) {
        if(at>length||length-at<4)return false;
        value=0;for(unsigned i=0;i<4;++i)value=(value<<8)|bytes[frame+at++];
        return true;
    };
    if(!read16(found.version)||!read16(found.delay)||!read16(found.quality)||
       !read32(found.stream_bytes)||!read32(found.audio_frames)||
       !read16(found.toc_entries)||!read16(found.toc_scale)||
       !read16(found.toc_entry_bytes)||!read16(found.frames_per_entry)) {
        error="truncated MP3 VBRI header";return false;
    }
    if(found.version!=1) {error="unsupported MP3 VBRI version";return false;}
    if(!found.stream_bytes||!found.audio_frames) {
        error="zero MP3 VBRI stream count";return false;
    }
    if(found.toc_entries) {
        if(found.toc_entry_bytes<1||found.toc_entry_bytes>4||
           !found.toc_scale||!found.frames_per_entry) {
            error="invalid MP3 VBRI table dimensions";return false;
        }
        const size_t table_bytes=size_t(found.toc_entries)*found.toc_entry_bytes;
        if(at>length||table_bytes>length-at) {
            error="truncated MP3 VBRI table";return false;
        }
    }
    output=found;return true;
}
}
}
