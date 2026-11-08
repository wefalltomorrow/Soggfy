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
    assert(!info.xing.present&&!info.xing.flags&&!info.xing.audio_frames&&!info.xing.stream_bytes);
}
static void Put32(Bytes& bytes,size_t at,uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes.at(at+i)=uint8_t(value>>(24-8*i));
}
static void Xing() {
    Mp3Layout info;const char* error=nullptr;
    for(unsigned version:{3u,2u,0u})for(unsigned channels:{1u,2u})
    for(bool crc:{false,true})for(const char* magic:{"Xing","Info"}) {
        const bool high=version==3;
        const unsigned side=high?(channels==1?17:32):(channels==1?9:17);
        const unsigned length=high?417:208,third=high?0x90:version==2?0x80:0x40;
        auto audio=Frame((0xe3|(version<<3))-(crc?1:0),third,length,channels==1?0xc0:0);
        for(unsigned flags=0;flags<16;++flags) {
            auto tag=audio;std::memcpy(tag.data()+4+side,magic,4);
            Put32(tag,8+side,flags);size_t cursor=12+side;
            if(flags&1){Put32(tag,cursor,2);cursor+=4;}
            if(flags&2){Put32(tag,cursor,3*length);cursor+=4;}
            if(flags&4)cursor+=100;
            if(flags&8){Put32(tag,cursor,50);cursor+=4;}
            Crc(tag);
            auto complete=tag;Append(complete,audio);Append(complete,audio);
            assert(InspectMp3Layout(complete,info,error));
            assert(info.frames==3&&info.xing.present&&info.xing.flags==flags);
            assert(info.xing.audio_frames==((flags&1)?2u:0u));
            assert(info.xing.stream_bytes==((flags&2)?3*length:0u));
            CompressedBuffer blocks;assert(blocks.Append(complete.data(),complete.size(),65536));
            assert(InspectMp3Layout(blocks,info,error)&&info.xing.present);
            Reject(tag);
            if(flags&3) {
                auto short_file=complete;short_file.resize(2*length);Reject(short_file);
                auto long_file=complete;Append(long_file,audio);Reject(long_file);
            }
            // Optional fields must fit in the FIRST frame, even when the
            // caller supplies a longer file with bytes beyond that frame.
            for(size_t bound=8+side;bound<cursor;++bound) {
                Mp3XingInfo x;error=nullptr;
                assert(!mp3_layout_detail::ReadXing(complete,0,bound,side,x,error));
                assert(error&&!x.present);
            }
            auto bad=complete;Put32(bad,8+side,16);Crc(bad);Reject(bad);
            if(flags&1){bad=complete;Put32(bad,12+side,0);Crc(bad);Reject(bad);}
            if(flags&2){bad=complete;Put32(bad,12+side+((flags&1)?4:0),0);Crc(bad);Reject(bad);}
            // The first audio frame cannot borrow bytes from the seek frame.
            bad=complete;SetBits(bad,length+(crc?6:4),0,high?9:8,1);Crc(bad,length);Reject(bad);
        }
    }
    std::cout<<"PASS: Xing/Info counts, all flag combinations, bounded optional fields, protected frames, fixed blocks and seek-frame reservoir isolation\n";
}
static void Scalefactors() {
    // Independent boundary expectations for the six LSF encoding ranges and
    // MPEG-1 group reuse. Columns are long, short and mixed scalefactor bits.
    struct Case {bool high,intensity;unsigned compress,reuse,long_bits,short_bits,mixed_bits;};
    const Case cases[]={
        {true,false,15,0,74,126,122},{true,false,15,8,50,126,122},
        {true,false,15,4,54,126,122},{true,false,15,2,59,126,122},
        {true,false,15,1,59,126,122},{true,false,15,15,0,126,122},
        {false,false,399,0,74,126,114},{false,false,400,0,0,0,0},
        {false,false,499,0,65,108,96},{false,false,500,0,0,0,0},
        {false,false,511,0,53,90,81},{false,true,359,0,98,168,159},
        {false,true,360,0,0,0,0},{false,true,487,0,54,90,81},
        {false,true,488,0,0,0,0},{false,true,511,0,40,69,54}};
    Mp3Layout info;const char*error=nullptr;
    for(const auto& c:cases)for(unsigned block:{0u,1u,2u,3u})for(bool mixed:{false,true}) {
        unsigned expected=block==2?(mixed?c.mixed_bits:c.short_bits):c.long_bits;
        assert(mp3_layout_detail::ScalefactorBits(c.high,c.compress,block,mixed,c.reuse,c.intensity)==expected);
        for(unsigned version:{3u,2u,0u}) {
            if((version==3)!=c.high)continue;
            for(unsigned channels:{1u,2u})for(bool crc:{false,true}) {
                if(c.intensity&&channels!=2)continue;
                for(unsigned record=0;record<channels*(c.high?2:1);++record) {
                    if(c.intensity&&record%channels!=1)continue;
                    const unsigned second=0xe3|(version<<3),prefix=crc?6:4;
                    auto frame=Frame(second-(crc?1:0),c.high?0x90:version==2?0x80:0x40,
                                     c.high?417:208,channels==1?0xc0:c.intensity?0x50:0);
                    const unsigned start=c.high?(channels==1?18:20):(channels==1?9:10);
                    const unsigned bit=start+record*(c.high?59:63),switched=bit+(c.high?33:38);
                    if(c.high)SetBits(frame,prefix,(channels==1?14:12)+4*(record%channels),4,c.reuse);
                    SetBits(frame,prefix,bit+29,c.high?4:9,c.compress);
                    if(block) {
                        SetBits(frame,prefix,switched,1,1);
                        SetBits(frame,prefix,switched+1,2,block);
                        SetBits(frame,prefix,switched+3,1,mixed);
                    }
                    // First granule cannot reuse; its long block still needs 74.
                    unsigned needed=c.high&&record<channels&&block!=2?74:expected;
                    SetBits(frame,prefix,bit,12,needed);Crc(frame);
                    assert(InspectMp3Layout(frame,info,error));
                    if(needed) {
                        SetBits(frame,prefix,bit,12,needed-1);Crc(frame);
                        assert(!InspectMp3Layout(frame,info,error));
                        assert(std::strcmp(error,"MP3 scalefactors exceed granule bit budget")==0);
                        assert(!info.frames&&!info.audio_size);
                    }
                }
            }
        }
    }
    // Mode-extension bits apply to intensity stereo only in joint stereo,
    // and only to the right channel. 359 needs 54 ordinary long-block bits.
    for(unsigned mode:{0u,0x10u,0x40u,0x50u})for(unsigned channel:{0u,1u}) {
        auto frame=Frame(0xf3,0x80,208,mode);unsigned bit=10+63*channel;
        SetBits(frame,4,bit+29,9,359);
        unsigned need=mode==0x50&&channel==1?98:54;
        SetBits(frame,4,bit,12,need);assert(InspectMp3Layout(frame,info,error));
        SetBits(frame,4,bit,12,need-1);Reject(frame);
    }
    std::cout<<"PASS: scalefactor budgets, MPEG-1 reuse, all LSF ranges, intensity channel/mode, switched/mixed blocks and CRC layouts\n";
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
    assert(argc==1);Scalefactors();Xing();Mp3Layout info;const char* error="stale";
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
