#include "../native/mp3_layout.h"
#include "../native/compressed_buffer.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace history;
using Bytes=std::vector<uint8_t>;
static void Crc(Bytes& bytes,size_t at=0) {
    if(bytes.at(at+1)&1)return;
    const bool high=((bytes.at(at+1)>>3)&3)==3,mono=(bytes.at(at+3)>>6)==3;
    const unsigned side=high?(mono?17:32):(mono?9:17);
    // Independent polynomial long division, including the initial remainder.
    const size_t bits=(side+2)*8;std::vector<unsigned> dividend(bits+16);
    for(size_t i=0;i<bits;++i)
        dividend[i]=(bytes.at(at+(i/8<2?i/8+2:i/8+4))>>(7-i%8))&1;
    for(size_t i=0;i<16;++i)dividend[i]^=1;
    for(size_t i=0;i<bits;++i)if(dividend[i])
        for(size_t j=0;j<=16;++j)dividend[i+j]^=(0x18005u>>(16-j))&1;
    unsigned crc=0;for(size_t i=bits;i<bits+16;++i)crc=(crc<<1)|dividend[i];
    bytes.at(at+4)=uint8_t(crc>>8);bytes.at(at+5)=uint8_t(crc);
}
static Bytes Frame(unsigned second=0xfb,unsigned third=0x90,size_t size=417,unsigned mode=0) {
    Bytes bytes(size);bytes[0]=0xff;bytes[1]=uint8_t(second);bytes[2]=uint8_t(third);bytes[3]=uint8_t(mode);Crc(bytes);return bytes;
}
static void Append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
static void SetBits(Bytes& b,size_t byte,size_t bit,unsigned count,unsigned value) {
    assert(count<=16&&value<(1u<<count));
    for(unsigned i=0;i<count;++i) {
        const size_t position=bit+i;const auto mask=uint8_t(1u<<(7-position%8));
        b.at(byte+position/8)=uint8_t((b.at(byte+position/8)&~mask)|
            (((value>>(count-1-i))&1)?mask:0));
    }
}
static void Reject(const Bytes& bytes) {
    Mp3Layout info;info.frames=999;info.rate=123;const char* error=nullptr;
    assert(!InspectMp3Layout(bytes,info,error)&&error&&*error);
    assert(!info.frames&&!info.rate&&!info.audio_size&&!info.nominal_samples);
}
int main(int argc,char** argv) {
    if(argc==2) {
        std::ifstream input(argv[1],std::ios::binary);assert(input);
        Bytes bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        Mp3Layout info;const char* error=nullptr;
        if(!InspectMp3Layout(bytes,info,error)){std::cerr<<error<<"\n";return 1;}
        std::cout<<info.rate<<' '<<info.channels<<' '<<info.frames<<' '<<info.nominal_samples<<' '<<info.audio_offset<<' '<<info.audio_size<<'\n';
        return 0;
    }
    assert(argc==1);Mp3Layout info;const char* error="stale";
    auto bytes=Frame();Append(bytes,Frame(0xfa,0xa2,523));Append(bytes,Frame());
    assert(InspectMp3Layout(bytes,info,error)&&!error);
    assert(info.rate==44100&&info.channels==2&&info.frames==3&&info.nominal_samples==3456&&info.audio_offset==0&&info.audio_size==1357);
    CompressedBuffer blocks;assert(blocks.Append(bytes.data(),bytes.size(),65536));
    assert(InspectMp3Layout(blocks,info,error)&&info.frames==3);
    Bytes long_stream;
    for(unsigned i=0;i<200;++i)Append(long_stream,Frame());
    CompressedBuffer crossing;
    assert(crossing.Append(long_stream.data(),long_stream.size(),131072));
    assert(InspectMp3Layout(crossing,info,error)&&info.frames==200&&info.audio_size==83400);
    for(size_t count=0;count<bytes.size();++count) {
        Bytes prefix(bytes.begin(),bytes.begin()+count);
        // Whole-frame truncation is structurally indistinguishable from a
        // shorter recording. Never promote this result to audio completeness.
        if(count==417||count==940)assert(InspectMp3Layout(prefix,info,error));
        else Reject(prefix);
    }
    for(const auto& header:std::vector<Bytes>{Frame(0xeb),Frame(0xfd),Frame(0xfb,0),Frame(0xfb,0xf0),Frame(0xfb,0x9c),Frame(0xfb,0x90,417,2)})Reject(header);
    auto mixed=bytes;Append(mixed,Frame(0xf3,0x80,208,0xc0));Reject(mixed);
    auto junk=bytes;junk.insert(junk.begin()+417,0);Reject(junk);
    junk=bytes;junk.push_back(0);Reject(junk);
    for(const auto& low:std::vector<Bytes>{Frame(0xf3,0x80,208,0xc0),Frame(0xe3,0x40,208)}) {
        assert(InspectMp3Layout(low,info,error)&&info.frames==1&&info.nominal_samples==576);
        assert(info.rate==(low[1]==0xf3?22050u:11025u)&&info.channels==(low[1]==0xf3?1u:2u));
    }
    // Independently chosen byte/bit boundaries cover every side-info layout,
    // with and without a CRC word. These are extent fixtures, not valid audio.
    for(unsigned second:{0xfbu,0xf3u,0xe3u})for(unsigned channels:{1u,2u})for(bool crc:{false,true}) {
        const bool high=second==0xfb;const size_t frame_size=high?417:208;
        const unsigned third=high?0x90:second==0xf3?0x80:0x40;
        const unsigned prefix=crc?6:4,back_bits=high?9:8;
        const unsigned side_size=high?(channels==1?17:32):(channels==1?9:17);
        const unsigned first_length=high?(channels==1?18:20):(channels==1?9:10);
        const unsigned stride=high?59:63,records=channels*(high?2:1);
        const unsigned payload=unsigned(frame_size)-prefix-side_size;
        auto first=Frame(second-(crc?1:0),third,frame_size,channels==1?0xc0:0);
        auto bad=first;SetBits(bad,prefix,0,back_bits,1);Crc(bad);Reject(bad);
        for(unsigned record=0;record<records;++record) {
            const size_t record_bit=first_length+record*stride;
            bad=first;SetBits(bad,prefix,record_bit,12,payload*8+1);Crc(bad);Reject(bad);
            bad=first;SetBits(bad,prefix,record_bit+12,9,289);Crc(bad);Reject(bad);
            bad=first;SetBits(bad,prefix,record_bit+12,9,288);Crc(bad);
            assert(InspectMp3Layout(bad,info,error)); // layout boundary, not valid audio
            const size_t switched=record_bit+(high?33:38);
            bad=first;SetBits(bad,prefix,switched,1,1);Crc(bad);Reject(bad);
            for(unsigned type:{1u,2u,3u}) {
                SetBits(bad,prefix,switched+1,2,type);Crc(bad);
                assert(InspectMp3Layout(bad,info,error));
            }
        }
        if(records>1) {
            bad=first;SetBits(bad,prefix,first_length,12,payload*4+1);
            SetBits(bad,prefix,first_length+stride,12,payload*4+1);Crc(bad);Reject(bad);
        }
        // Leave one reservoir byte after a partially consumed byte. The next
        // frame may start at the following byte, but not one byte earlier.
        SetBits(first,prefix,first_length,12,(payload-2)*8+1);
        Crc(first);
        auto second_frame=Frame(second-(crc?1:0),third,frame_size,channels==1?0xc0:0);
        SetBits(second_frame,prefix,0,back_bits,1);
        SetBits(second_frame,prefix,first_length,12,(payload+1)*8);
        Crc(second_frame);
        auto good=first;Append(good,second_frame);
        assert(InspectMp3Layout(good,info,error)&&info.frames==2);
        CompressedBuffer reservoir_blocks;assert(reservoir_blocks.Append(good.data(),good.size(),65536));
        assert(InspectMp3Layout(reservoir_blocks,info,error));
        bad=good;SetBits(bad,frame_size+prefix,0,back_bits,2);Crc(bad,frame_size);Reject(bad);
        bad=good;SetBits(bad,frame_size+prefix,first_length,12,(payload+1)*8+1);Crc(bad,frame_size);Reject(bad);
        Reject(second_frame); // missing predecessor despite complete framing
        if(crc) {
            // Every protected bit and stored checksum bit must be covered.
            for(size_t byte=2;byte<prefix+side_size;++byte)for(unsigned bit=0;bit<8;++bit) {
                bad=first;bad[byte]^=uint8_t(1u<<bit);Reject(bad);
            }
            bad=first;bad[prefix+side_size]^=128;
            assert(InspectMp3Layout(bad,info,error)); // CRC excludes main data
        }
    }
    for(unsigned version:{3u,4u}) {
        Bytes tag={'I','D','3',uint8_t(version),0,0,0,0,0,3,0,0,0};
        auto tagged=tag;Append(tagged,bytes);
        Bytes tail(128);std::memcpy(tail.data(),"TAG",3);Append(tagged,tail);
        assert(InspectMp3Layout(tagged,info,error)&&info.audio_offset==13&&info.audio_size==1357);
        for(unsigned index:{4u,5u,6u}) {auto bad=tagged;bad[index]=index==6?128:1;Reject(bad);}
        if(version==4) {
            tag[5]=16;auto footer=Bytes(tag.begin(),tag.begin()+10);footer[0]='3';footer[1]='D';footer[2]='I';
            auto good=tag;Append(good,footer);Append(good,bytes);
            assert(InspectMp3Layout(good,info,error)&&info.audio_offset==23);
            for(size_t i=13;i<23;++i){auto bad=good;bad[i]^=1;Reject(bad);}
        }
    }
    assert(!InspectMp3Layout(nullptr,0,info,error)&&error&&!info.frames);
    const uint8_t tiny=0;
    assert(!InspectMp3Layout(&tiny,256ull*1024*1024+1,info,error));
    uint32_t state=0x78ba9123;
    for(unsigned trial=0;trial<10000;++trial) {
        auto mutated=bytes;
        state=state*1664525u+1013904223u;const size_t at=state%mutated.size();
        state=state*1664525u+1013904223u;mutated[at]^=uint8_t(state>>24);
        if(InspectMp3Layout(mutated,info,error))assert(!error&&info.frames&&info.audio_offset<=mutated.size()&&info.audio_size<=mutated.size()-info.audio_offset);
        else assert(error&&!info.frames&&!info.audio_size);
    }
    std::cout<<"PASS: bounded MP3 layout, protected CRC, side constraints, reservoir extents, ID3 extents/footer, fixed blocks, truncation and mutations; not an audio validator\n";
    return 0;
}
