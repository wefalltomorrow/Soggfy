#include "../native/existing_quality_core.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace history;
static void crc(std::vector<unsigned char>& v) {
    for(unsigned i=22;i<26;++i)v[i]=0;
    uint32_t value=0;
    for(auto b:v){value^=uint32_t(b)<<24;for(unsigned i=0;i<8;++i)value=(value<<1)^((value&0x80000000)?0x04c11db7:0);}
    for(unsigned i=0;i<4;++i)v[22+i]=uint8_t(value>>(i*8));
}
int main() {
    Quality q;
    std::vector<unsigned char> ogg(58);
    std::memcpy(ogg.data(),"OggS",4);ogg[5]=2;ogg[26]=1;ogg[27]=30;
    std::memcpy(ogg.data()+28,"\x01vorbis",7);ogg[39]=2;
    ogg[40]=0x44;ogg[41]=0xac;ogg[56]=0xb8;ogg[57]=1;crc(ogg);
    assert(ParseExistingQualityHeader(ogg.data(),ogg.size(),q)&&q.rate==44100&&q.channels==2);
    for(size_t n=0;n<ogg.size();++n)assert(!ParseExistingQualityHeader(ogg.data(),n,q));
    auto legacy=ogg;legacy[18]=1;crc(legacy);assert(ParseExistingQualityHeader(legacy.data(),legacy.size(),q));
    for(const auto& mutation:std::vector<std::pair<unsigned,unsigned>>{{5,3},{6,1},{26,2},{27,29},{35,1},{39,0},{56,0xb5},{56,0x8b},{56,0xe8},{57,0},{57,3}}) {
        auto bad=ogg;bad[mutation.first]=uint8_t(mutation.second);crc(bad);
        assert(!ParseExistingQualityHeader(bad.data(),bad.size(),q));
    }
    auto bad=ogg;bad[22]^=1;assert(!ParseExistingQualityHeader(bad.data(),bad.size(),q));
    std::vector<unsigned char> flac(42);std::memcpy(flac.data(),"fLaC",4);flac[7]=34;flac[8]=flac[10]=0x10;
    const uint64_t packed=(uint64_t(44100)<<44)|(uint64_t(1)<<41)|(uint64_t(15)<<36);
    for(unsigned i=0;i<8;++i)flac[18+i]=uint8_t(packed>>(56-i*8));
    assert(ParseExistingQualityHeader(flac.data(),flac.size(),q)&&q.bits==16&&q.rate==44100);
    for(size_t n=0;n<flac.size();++n)assert(!ParseExistingQualityHeader(flac.data(),n,q));
    for(const auto& mutation:std::vector<std::pair<unsigned,unsigned>>{{4,1},{7,33},{8,0},{10,1},{18,0xff},{21,0}}) {
        bad=flac;bad[mutation.first]=uint8_t(mutation.second);
        assert(!ParseExistingQualityHeader(bad.data(),bad.size(),q));
    }
    bad=flac;bad[14]=10;bad[17]=5;assert(!ParseExistingQualityHeader(bad.data(),bad.size(),q));
    // Unknown total-sample count, MD5 and frame sizes remain legal.
    flac[4]=0x80;assert(ParseExistingQualityHeader(flac.data(),flac.size(),q));
    assert(!ParseExistingQualityHeader(nullptr,42,q));
    std::puts("PASS: bounded Vorbis/FLAC identification, CRC, truncations and malformed headers");
}
