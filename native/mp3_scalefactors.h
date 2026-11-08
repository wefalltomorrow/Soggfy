#pragma once
#include <cstdint>

namespace history::mp3_layout_detail {
// Number of transmitted scalefactor bits, before Huffman data. Inputs are
// bounded side-information fields. This does not decode the scalefactors.
inline unsigned ScalefactorBits(bool mpeg1,unsigned compress,unsigned block,
                               bool mixed,unsigned reuse,bool intensity) {
    if(mpeg1) {
        static constexpr uint8_t widths[16][2]={
            {0,0},{0,1},{0,2},{0,3},{3,0},{1,1},{1,2},{1,3},
            {2,1},{2,2},{2,3},{3,1},{3,2},{3,3},{4,2},{4,3}};
        const unsigned a=widths[compress][0],b=widths[compress][1];
        if(block==2)return (mixed?17:18)*a+18*b;
        return ((reuse&8)?0:6*a)+((reuse&4)?0:5*a)+
               ((reuse&2)?0:5*b)+((reuse&1)?0:5*b);
    }
    // MPEG-2/2.5 partition sizes for long, short and mixed blocks. The six
    // groups correspond to three ordinary and three intensity encodings.
    static constexpr uint8_t partitions[6][3][4]={
        {{6,5,5,5},{9,9,9,9},{6,9,9,9}},
        {{6,5,7,3},{9,9,12,6},{6,9,12,6}},
        {{11,10,0,0},{18,18,0,0},{15,18,0,0}},
        {{7,7,7,0},{12,12,12,0},{6,15,12,0}},
        {{6,6,6,3},{12,9,9,6},{6,12,9,6}},
        {{8,8,5,0},{15,12,9,0},{6,18,9,0}}};
    unsigned group=0,width[4]={};
    if(intensity) {
        compress>>=1; // low bit selects intensity scale, not bit width
        if(compress<180) {
            group=3;width[0]=compress/36;width[1]=compress/6%6;width[2]=compress%6;
        } else if(compress<244) {
            group=4;compress-=180;
            width[0]=compress/16;width[1]=compress/4%4;width[2]=compress%4;
        } else {
            group=5;compress-=244;width[0]=compress/3;width[1]=compress%3;
        }
    } else if(compress<400) {
        width[0]=compress/80;width[1]=compress/16%5;
        width[2]=compress/4%4;width[3]=compress%4;
    } else if(compress<500) {
        group=1;compress-=400;
        width[0]=compress/20;width[1]=compress/4%5;width[2]=compress%4;
    } else {
        group=2;compress-=500;width[0]=compress/3;width[1]=compress%3;
    }
    const unsigned shape=block==2?(mixed?2:1):0;
    unsigned bits=0;
    for(unsigned i=0;i<4;++i)bits+=width[i]*partitions[group][shape][i];
    return bits;
}
}
