#pragma once
#include "mp3_huffman_tables.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace history::mp3_huffman_detail {
class BoundedBits {
public:
    BoundedBits(const uint8_t* bytes,size_t size,size_t begin,size_t end)
        : bytes_(bytes),size_(size),position_(begin),end_(end) {}
    size_t position()const noexcept{return position_;}
    size_t remaining()const noexcept{return end_-position_;}
    bool Peek(unsigned count,uint32_t& value)const noexcept {
        return PeekAt(0,count,value);
    }
    bool PeekAt(size_t offset,unsigned count,uint32_t& value)const noexcept {
        value=0;
        if(count>24||position_>end_||offset>end_-position_)return false;
        const size_t bit=position_+offset;
        if(bit>size_*8)return false;
        for(unsigned i=0;i<count;++i) {
            value<<=1;
            if(i<end_-bit&&bit+i<size_*8)
                value|=(bytes_[(bit+i)/8]>>(7-(bit+i)%8))&1;
        }
        return true;
    }
    bool Read(unsigned count,uint32_t& value)noexcept {
        if(count>remaining()||!Peek(count,value))return false;
        position_+=count;return true;
    }
    bool Skip(unsigned count)noexcept {
        if(count>remaining())return false;
        position_+=count;return true;
    }
private:
    const uint8_t* bytes_;size_t size_,position_,end_;
};

inline bool DecodePair(BoundedBits& bits,unsigned selector)noexcept {
    if(selector==0)return true;
    if(selector>=32||selector==4||selector==14)return false;
    const size_t table_at=static_cast<size_t>(tabindex[selector]);
    const size_t table_count=sizeof(tabs)/sizeof(tabs[0]);
    if(table_at>=table_count)return false;
    size_t table_end=table_count;
    for(unsigned next=selector+1;next<32;++next) {
        const size_t candidate=static_cast<size_t>(tabindex[next]);
        if(candidate>table_at) {table_end=candidate;break;}
    }
    if(table_end>table_count||table_end<=table_at)return false;
    const int16_t* codebook=tabs+table_at;
    const size_t table_size=table_end-table_at;
    uint32_t peek=0;
    if(!bits.Peek(5,peek))return false;
    int leaf=codebook[peek];
    for(unsigned depth=0;leaf<0&&depth<16;++depth) {
        const unsigned width=static_cast<unsigned>(leaf)&7u;
        int offset=leaf/8;
        if(leaf%8<0)--offset; // arithmetic floor division, independent of compiler
        if(!bits.Skip(width)||!bits.Peek(width,peek))return false;
        const int index=static_cast<int>(peek)-offset;
        if(index<0||static_cast<size_t>(index)>=table_size)return false;
        leaf=codebook[index];
    }
    if(leaf<0)return false;
    const unsigned code_bits=static_cast<unsigned>(leaf)>>8;
    uint32_t ignored=0;
    if(!bits.Read(code_bits,ignored))return false;
    for(unsigned i=0;i<2;++i) {
        const unsigned magnitude=(static_cast<unsigned>(leaf)>>(i*4))&15u;
        if(magnitude==15&&g_linbits[selector])
            if(!bits.Read(g_linbits[selector],ignored))return false;
        if(magnitude&&!bits.Skip(1))return false;
    }
    return true;
}

inline bool TryDecodeCount1(BoundedBits& bits,unsigned selector)noexcept {
    const uint8_t* codebook=selector?tab33:tab32;
    const size_t count=selector?sizeof(tab33):sizeof(tab32);
    uint32_t peek=0;
    if(!bits.Peek(4,peek))return false;
    unsigned leaf=codebook[peek];
    if(!(leaf&8u)) {
        const unsigned extra=leaf&3u;
        if(!bits.PeekAt(4,extra,peek))return false;
        const size_t index=(leaf>>3)+peek;
        if(index>=count)return false;
        leaf=codebook[index];
    }
    const unsigned code_bits=leaf&7u;
    BoundedBits candidate=bits;
    uint32_t ignored=0;
    if(!candidate.Read(code_bits,ignored))return false;
    for(unsigned i=0;i<4;++i)
        if((leaf&(128u>>i))&&!candidate.Skip(1))return false;
    bits=candidate;
    return true;
}

inline unsigned SampleBandIndex(unsigned version,unsigned rate_index)noexcept {
    if(version==3)return rate_index+5;
    if(version==2)return rate_index+2;
    return rate_index?rate_index-1:0;
}

inline bool ValidateGranule(const uint8_t* data,size_t size,size_t start,
                            unsigned part_bits,unsigned scalefactor_bits,
                            unsigned big_values,const unsigned selectors[3],
                            bool switched,unsigned block,bool mixed,
                            unsigned region0,unsigned region1,unsigned band_index,
                            unsigned count1_table,const char*& error)noexcept {
    auto fail=[&](const char* message){error=message;return false;};
    if(start>size*8||part_bits>size*8-start)
        return fail("MP3 main data exceeds available reservoir");
    if(scalefactor_bits>part_bits)return fail("MP3 scalefactors exceed granule bit budget");
    const uint8_t* bands=nullptr;size_t band_count=0;
    if(switched&&block==2) {
        if(mixed){bands=g_scf_mixed[band_index];band_count=39;}
        else {bands=g_scf_short[band_index];band_count=39;}
    } else {bands=g_scf_long[band_index];band_count=22;}
    BoundedBits bits(data,size,start+scalefactor_bits,start+part_bits);
    // Region counts are inclusive in side information. Switched blocks infer
    // 7 for long/start/stop and mixed short blocks, or 8 for ordinary short.
    const unsigned region0_end=switched?
        ((block==2&&!mixed)?9u:8u):region0+1;
    const unsigned region1_end=switched?static_cast<unsigned>(band_count):
        region0_end+region1+1;
    unsigned remaining=big_values,band=0;
    while(remaining) {
        if(band>=band_count)return fail("MP3 big values exceed scalefactor bands");
        const unsigned region=band<region0_end?0u:band<region1_end?1u:2u;
        const unsigned pairs=bands[band]/2;
        if(!pairs)return fail("MP3 big values exceed scalefactor bands");
        const unsigned decode=std::min(remaining,pairs);
        for(unsigned i=0;i<decode;++i)
            if(!DecodePair(bits,selectors[region]))
                return fail("truncated or invalid MP3 Huffman pair");
        remaining-=decode;
        ++band;
    }
    unsigned coefficients=big_values*2;
    while(coefficients+4<=576) {
        BoundedBits candidate=bits;
        if(!TryDecodeCount1(candidate,count1_table))break; // final incomplete symbol is stuffing
        bits=candidate;coefficients+=4;
    }
    return true;
}
} // namespace history::mp3_huffman_detail
