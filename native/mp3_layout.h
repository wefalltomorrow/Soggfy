#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "mp3_entropy.h"
#include "mp3_scalefactors.h"
#include "mp3_xing.h"
#include "mp3_vbri.h"

namespace history {
// Frame boundaries and nominal sample counts do not establish valid audio or a
// complete recording. Without a retained Xing or VBRI declaration, truncation
// exactly at a frame boundary can pass. CRC, side-information constraints,
// reservoir extents, scalefactor bit budgets and bounded Huffman payload syntax
// are checked; spectral reconstruction is not.
// Decode, gapless timing, provenance
// and listening/lifecycle checks remain separate requirements for publication.
struct Mp3Layout {
    unsigned rate=0,channels=0,frames=0;
    uint64_t nominal_samples=0;
    size_t audio_offset=0,audio_size=0;
    Mp3XingInfo xing;
    Mp3VbriInfo vbri;
};
namespace mp3_layout_detail {
struct EntropyGranule {
    unsigned part_bits=0,scalefactor_bits=0,big_values=0;
    unsigned selectors[3]={};
    unsigned block=0,region0=0,region1=0,count1_table=0;
    bool switched=false,mixed=false;
};
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
// Called only within a side-information extent already checked by the frame
// parser. Layer III has fixed side-information record widths in both modes.
template<class Bytes>
unsigned Bits(const Bytes& b,size_t byte,size_t bit,unsigned count) {
    unsigned value=0;
    for(unsigned i=0;i<count;++i) {
        const size_t position=bit+i;
        value=(value<<1)|((b[byte+position/8]>>(7-position%8))&1);
    }
    return value;
}
// MPEG Layer III protection covers the last 16 header bits and all side
// information, excluding the stored CRC word and compressed main data.
template<class Bytes>
uint16_t ProtectedCrc(const Bytes& b,size_t frame,unsigned side_size) {
    uint16_t crc=0xffff;
    for(unsigned i=0;i<side_size+2;++i) {
        const unsigned value=b[frame+(i<2?i+2:i+4)];
        crc^=uint16_t(value<<8);
        for(unsigned bit=0;bit<8;++bit)
            crc=uint16_t((unsigned(crc)<<1)^((crc&0x8000)?0x8005:0));
    }
    return crc;
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
    uint64_t main_bytes=0,used_end=0;
    unsigned samples_per_frame=0;
    // main_data_begin reaches back at most 511 bytes. Retain that history and
    // one bounded Layer III frame payload for entropy checks.
    constexpr size_t kReservoirBytes=511,kMaxFrameBytes=1441;
    uint8_t reservoir[kReservoirBytes]={};size_t reservoir_size=0;
    uint8_t main_data_window[kReservoirBytes+kMaxFrameBytes];
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
        const unsigned prefix=4+((h&(1u<<16))?0:2);
        if(length<prefix+side_info||length>end-at)
            return fail("truncated MP3 frame");
        if(prefix==6&&mp3_layout_detail::ProtectedCrc(bytes,at,side_info)!=
            ((unsigned(bytes[at+4])<<8)|bytes[at+5]))return fail("MP3 protected CRC mismatch");
        if(!found.frames&&!mp3_layout_detail::ReadXing(bytes,at,length,side_info,found.xing,error))return false;
        if(!found.frames&&!mp3_layout_detail::ReadVbri(bytes,at,length,found.vbri,error))return false;
        // A recognized seek frame is metadata. Its bytes must not provide
        // reservoir history to the first audio frame. Nominal samples still
        // count physical frames; they are never a gapless duration claim.
        const bool seek_frame=!found.frames&&(found.xing.present||found.vbri.present);
        if(!seek_frame) {
            // Treat main data as one byte sequence with frame headers, CRC words
            // and side information removed. Each frame points backwards into
            // that sequence; its declared bit range must be present and disjoint
            // from previously consumed bits. Gaps/stuffing are permitted.
            const size_t side=at+prefix;
            const unsigned back=mp3_layout_detail::Bits(bytes,side,0,version==3?9:8);
            size_t bit=version==3?(channels==1?18:20):(channels==1?9:10);
            const unsigned granule_count=channels*(version==3?2:1);
            mp3_layout_detail::EntropyGranule granules[4]{};
            unsigned data_bits=0;
            for(unsigned i=0;i<granule_count;++i) {
                auto& granule=granules[i];
                granule.part_bits=mp3_layout_detail::Bits(bytes,side,bit,12);
                data_bits+=granule.part_bits;
                granule.big_values=mp3_layout_detail::Bits(bytes,side,bit+12,9);
                if(granule.big_values>288)
                    return fail("MP3 coefficient pair count exceeds granule");
                const size_t switched=bit+(version==3?33:38);
                if(mp3_layout_detail::Bits(bytes,side,switched,1)&&
                   !mp3_layout_detail::Bits(bytes,side,switched+1,2))
                    return fail("MP3 switched block type is zero");
                granule.switched=mp3_layout_detail::Bits(bytes,side,switched,1)!=0;
                granule.block=granule.switched?
                    mp3_layout_detail::Bits(bytes,side,switched+1,2):0;
                granule.mixed=granule.switched&&
                    mp3_layout_detail::Bits(bytes,side,switched+3,1)!=0;
                // Reserved pair-codebook selectors have no decoding table.
                // Reject them even for an empty region; do not silently turn
                // an unsupported declaration into table-zero coefficients.
                const size_t tables=switched+(granule.switched?4:1);
                for(unsigned table=0;table<(granule.switched?2u:3u);++table) {
                    const unsigned selected=mp3_layout_detail::Bits(bytes,side,tables+5*table,5);
                    if(selected==4||selected==14)
                        return fail("unsupported MP3 Huffman table selector");
                    granule.selectors[table]=selected;
                }
                const unsigned compress=mp3_layout_detail::Bits(bytes,side,bit+29,version==3?4:9);
                const unsigned channel=i%channels;
                const unsigned reuse=version==3&&i>=channels?
                    mp3_layout_detail::Bits(bytes,side,(channels==1?14:12)+4*channel,4):0;
                const bool intensity=((h>>6)&3)==1&&(h&16)&&channel==1;
                granule.scalefactor_bits=mp3_layout_detail::ScalefactorBits(
                    version==3,compress,granule.block,granule.mixed,reuse,intensity);
                if(granule.scalefactor_bits>granule.part_bits)
                    return fail("MP3 scalefactors exceed granule bit budget");
                if(granule.switched) {
                    granule.region0=(granule.block==2&&!granule.mixed)?8:7;
                    granule.region1=255;
                } else {
                    granule.region0=mp3_layout_detail::Bits(bytes,side,switched+16,4);
                    granule.region1=mp3_layout_detail::Bits(bytes,side,switched+20,3);
                }
                granule.count1_table=mp3_layout_detail::Bits(bytes,side,switched+25,1);
                bit+=version==3?59:63;
            }
            if(back>main_bytes)return fail("MP3 reservoir starts before recording");
            if(back>reservoir_size)return fail("MP3 reservoir starts before recording");
            const uint64_t start=(main_bytes-back)*8;
            const size_t payload_at=side+side_info;
            const size_t payload_size=length-prefix-side_info;
            if(payload_size>kMaxFrameBytes)return fail("MP3 frame payload exceeds bounded window");
            std::memcpy(main_data_window,reservoir,reservoir_size);
            for(size_t i=0;i<payload_size;++i)main_data_window[reservoir_size+i]=bytes[payload_at+i];
            const size_t window_size=reservoir_size+payload_size;
            main_bytes+=payload_size;
            if(start<used_end)return fail("MP3 reservoir overlaps prior frame data");
            if(data_bits>main_bytes*8-start)return fail("MP3 main data exceeds available reservoir");
            used_end=start+data_bits;
            size_t granule_bit=(reservoir_size-back)*8;
            for(unsigned i=0;i<granule_count;++i) {
                const auto& granule=granules[i];
                if(!mp3_huffman_detail::ValidateGranule(main_data_window,window_size,
                    granule_bit,granule.part_bits,granule.scalefactor_bits,
                    granule.big_values,granule.selectors,granule.switched,
                    granule.block,granule.mixed,granule.region0,granule.region1,
                    mp3_huffman_detail::SampleBandIndex(version,rate_index),
                    granule.count1_table,error))return false;
                granule_bit+=granule.part_bits;
            }
            reservoir_size=window_size<kReservoirBytes?window_size:kReservoirBytes;
            std::memcpy(reservoir,main_data_window+window_size-reservoir_size,reservoir_size);
        }
        if(!found.frames){
            found.rate=rate;found.channels=channels;
            samples_per_frame=version==3?1152:576;
        }
        else if(found.rate!=rate||found.channels!=channels)return fail("MP3 format changed");
        if(found.frames==1000000)return fail("MP3 frame limit exceeded");
        ++found.frames;found.nominal_samples+=version==3?1152:576;
        if(found.nominal_samples>uint64_t(rate)*86400)return fail("MP3 duration limit exceeded");
        at+=length;
    }
    if(!found.frames)return fail("MP3 has no frames");
    found.audio_size=end-found.audio_offset;
    if(found.xing.present) {
        if(found.frames<2)return fail("MP3 Xing header has no audio frames");
        if((found.xing.flags&1)&&uint64_t(found.xing.audio_frames)+1!=found.frames)
            return fail("MP3 Xing frame count mismatch");
        if((found.xing.flags&2)&&found.xing.stream_bytes!=found.audio_size)
            return fail("MP3 Xing byte count mismatch");
        if(found.xing.gapless_present) {
            const uint64_t audio_samples=found.nominal_samples-samples_per_frame;
            const uint64_t trim=uint64_t(found.xing.encoder_delay)+found.xing.encoder_padding;
            if(trim>audio_samples)return fail("MP3 gapless trim exceeds audio extent");
            found.xing.gapless_samples=audio_samples-trim;
        }
    }
    if(found.vbri.present) {
        if(uint64_t(found.vbri.audio_frames)+1!=found.frames)
            return fail("MP3 VBRI frame count mismatch");
        if(found.vbri.stream_bytes!=found.audio_size)
            return fail("MP3 VBRI byte count mismatch");
    }
    output=found;return true;
}
inline bool InspectMp3Layout(const uint8_t* bytes,size_t size,Mp3Layout& output,const char*& error) {
    if(!bytes){output={};error="null MP3 input";return false;}
    return InspectMp3Layout(mp3_layout_detail::View{bytes,size},output,error);
}
}
