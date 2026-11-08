#pragma once
#include "ogg_history_core.h"
#include "library_layout.h"
#include <cstring>

namespace history {
// Bounded identification only, not proof that the complete file is playable.
// An unknown/malformed header must never authorize replacing an existing file.
inline bool ParseExistingQualityHeader(const unsigned char* b,size_t n,Quality& out) {
    if(!b||n<4)return false;
    if(!std::memcmp(b,"OggS",4)) {
        // Vorbis identification occupies a complete, standalone BOS page.
        if(n<58||b[5]!=2||b[26]!=1||b[27]!=30)return false;
        for(unsigned i=6;i<14;++i)if(b[i])return false;
        // Legacy native captures can start at sequence one after Spotify's
        // private page is removed. Sequence coverage belongs to capture validation.
        Page page;
        if(!ParsePage(b,58,page)||!page.vorbis_start)return false;
        const unsigned small=b[56]&15,large=b[56]>>4;
        if(small<6||large>13||small>large||b[57]!=1)return false;
        out={Codec::Vorbis,page.rate,page.channels,0,page.bitrate};
        return true;
    }
    if(n<42||std::memcmp(b,"fLaC",4)||(b[4]&127)!=0||b[5]||b[6]||b[7]!=34)return false;
    const unsigned min_block=(unsigned(b[8])<<8)|b[9],max_block=(unsigned(b[10])<<8)|b[11];
    const unsigned min_frame=(unsigned(b[12])<<16)|(unsigned(b[13])<<8)|b[14];
    const unsigned max_frame=(unsigned(b[15])<<16)|(unsigned(b[16])<<8)|b[17];
    if(min_block<16||max_block<min_block||(min_frame&&max_frame&&min_frame>max_frame))return false;
    uint64_t packed=0;for(unsigned i=18;i<26;++i)packed=(packed<<8)|b[i];
    const unsigned rate=unsigned(packed>>44),bits=unsigned((packed>>36)&31)+1;
    if(!rate||rate>655350||bits<4)return false;
    out={Codec::Flac,rate,unsigned((packed>>41)&7)+1,bits,0};
    return true;
}
}
