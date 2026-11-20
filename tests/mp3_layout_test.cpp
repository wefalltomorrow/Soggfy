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
static void Reject(const Bytes& bytes,const char* expected=nullptr) {
    Mp3Layout info;info.frames=999;info.rate=123;const char* error=nullptr;
    assert(!InspectMp3Layout(bytes,info,error)&&error&&*error);
    if(expected)assert(std::strcmp(error,expected)==0);
    assert(!info.frames&&!info.rate&&!info.audio_size&&!info.nominal_samples);
    assert(!info.xing.present&&!info.xing.flags&&!info.xing.audio_frames&&!info.xing.stream_bytes);
    assert(!info.xing.gapless_present&&!info.xing.encoder_delay&&!info.xing.encoder_padding&&!info.xing.gapless_samples);
    assert(!info.vbri.present&&!info.vbri.version&&!info.vbri.audio_frames&&!info.vbri.stream_bytes);
}
static void Put16(Bytes& bytes,size_t at,uint16_t value) {
    bytes.at(at)=uint8_t(value>>8);bytes.at(at+1)=uint8_t(value);
}
static void Put32(Bytes& bytes,size_t at,uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes.at(at+i)=uint8_t(value>>(24-8*i));
}
static void Vbri() {
    Mp3Layout info;const char* error=nullptr;
    for(unsigned version:{3u,2u,0u})for(unsigned channels:{1u,2u})for(bool crc:{false,true}) {
        const bool high=version==3;
        const unsigned second=(high?0xfbu:version==2?0xf3u:0xe3u)-(crc?1:0);
        const unsigned third=high?0x90:version==2?0x80:0x40,length=high?417:208;
        auto seek=Frame(second,third,length,channels==1?0xc0:0);
        const size_t tag=36;std::memcpy(seek.data()+tag,"VBRI",4);
        Put16(seek,tag+4,1);Put16(seek,tag+6,576);Put16(seek,tag+8,75);
        Put32(seek,tag+10,3*length);Put32(seek,tag+14,2);
        Put16(seek,tag+18,4);Put16(seek,tag+20,1);
        Put16(seek,tag+22,2);Put16(seek,tag+24,1);
        for(unsigned i=0;i<8;++i)seek[tag+26+i]=uint8_t(i+1);
        Crc(seek);
        auto complete=seek;Append(complete,Frame(second,third,length,channels==1?0xc0:0));
        Append(complete,Frame(second,third,length,channels==1?0xc0:0));
        assert(InspectMp3Layout(complete,info,error));
        assert(info.frames==3&&info.vbri.present&&info.vbri.version==1);
        assert(info.vbri.delay==576&&info.vbri.quality==75);
        assert(info.vbri.audio_frames==2&&info.vbri.stream_bytes==3*length);
        assert(info.vbri.toc_entries==4&&info.vbri.toc_scale==1&&
               info.vbri.toc_entry_bytes==2&&info.vbri.frames_per_entry==1);
        auto bad=complete;Put32(bad,tag+14,1);Crc(bad);Reject(bad,"MP3 VBRI frame count mismatch");
        bad=complete;Put32(bad,tag+10,3*length-1);Crc(bad);Reject(bad,"MP3 VBRI byte count mismatch");
        bad=complete;Put16(bad,tag+4,2);Crc(bad);Reject(bad,"unsupported MP3 VBRI version");
        bad=complete;Put16(bad,tag+22,5);Crc(bad);Reject(bad,"invalid MP3 VBRI table dimensions");
        bad=complete;SetBits(bad,length+(crc?6:4),0,high?9:8,1);Crc(bad,length);
        Reject(bad,"MP3 reservoir starts before recording");
        // The fixed header and declared TOC must fit inside the tag frame.
        Mp3VbriInfo parsed;error=nullptr;
        assert(!mp3_layout_detail::ReadVbri(complete,0,69,parsed,error));
        assert(error&&std::strcmp(error,"truncated MP3 VBRI table")==0&&!parsed.present);
    }
    auto short_tag=Frame();std::memcpy(short_tag.data()+36,"VBRI",4);
    Mp3VbriInfo parsed;error=nullptr;
    for(size_t bound=40;bound<62;++bound) {
        assert(!mp3_layout_detail::ReadVbri(short_tag,0,bound,parsed,error));
        assert(error&&std::strcmp(error,"truncated MP3 VBRI header")==0&&!parsed.present);
    }
    std::cout<<"PASS: VBRI fixed offset, declared extents/counts, table bounds, versions, CRC layouts and fixed frames\n";
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
            if(flags==15)for(const char* encoder:{"LAME","Lavf","Lavc"}) {
                auto gapless=tag;std::memcpy(gapless.data()+cursor,encoder,4);
                gapless[cursor+21]=0x02;gapless[cursor+22]=0x10;gapless[cursor+23]=0x03;
                auto tagged=gapless;Append(tagged,audio);Append(tagged,audio);
                assert(InspectMp3Layout(tagged,info,error));
                assert(info.xing.gapless_present&&info.xing.encoder_delay==33&&
                       info.xing.encoder_padding==3);
                const uint64_t audio_samples=(high?2304:1152);
                assert(info.xing.gapless_samples==audio_samples-36);
                Mp3XingInfo parsed;error=nullptr;
                assert(mp3_layout_detail::ReadXing(gapless,0,cursor+23,side,parsed,error));
                assert(parsed.present&&!parsed.gapless_present);
                error=nullptr;
                assert(mp3_layout_detail::ReadXing(gapless,0,cursor+24,side,parsed,error));
                assert(parsed.gapless_present&&parsed.encoder_delay==33&&parsed.encoder_padding==3);
                auto impossible=tagged;
                impossible[cursor+21]=0xff;impossible[cursor+22]=0xff;impossible[cursor+23]=0xff;
                Crc(impossible);Reject(impossible,"MP3 gapless trim exceeds audio extent");
            }
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
    std::cout<<"PASS: Xing/Info counts, all flag combinations, bounded optional fields, gapless delay/padding, protected frames and seek-frame reservoir isolation\n";
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
static void HuffmanSelectors() {
    unsigned cases=0;
    Mp3Layout info;const char* error=nullptr;
    for(unsigned version:{3u,2u,0u})for(unsigned channels:{1u,2u})
    for(bool crc:{false,true}) {
        const bool high=version==3;
        const unsigned prefix=crc?6:4,start=high?(channels==1?18:20):(channels==1?9:10);
        for(unsigned record=0;record<channels*(high?2:1);++record)
        for(unsigned block:{0u,1u,2u,3u})for(unsigned table=0;table<(block?2u:3u);++table)
        for(unsigned selected=0;selected<32;++selected) {
            auto frame=Frame((0xe3|(version<<3))-(crc?1:0),
                             high?0x90:version==2?0x80:0x40,high?417:208,channels==1?0xc0:0);
            const unsigned switched=start+record*(high?59:63)+(high?33:38);
            if(block) {
                SetBits(frame,prefix,switched,1,1);
                SetBits(frame,prefix,switched+1,2,block);
                // These bits are subblock gains, not a third table selector.
                SetBits(frame,prefix,switched+14,5,14);
            }
            SetBits(frame,prefix,switched+(block?4:1)+5*table,5,selected);Crc(frame);
            CompressedBuffer blocks;assert(blocks.Append(frame.data(),frame.size(),65536));
            if(selected==4||selected==14) {
                Reject(frame);
                assert(!InspectMp3Layout(blocks,info,error));
                assert(std::strcmp(error,"unsupported MP3 Huffman table selector")==0);
            } else {
                assert(InspectMp3Layout(frame,info,error));
                assert(InspectMp3Layout(blocks,info,error));
            }
            ++cases;
        }
    }
    assert(cases==6912);
    std::cout<<"PASS: 6912 Huffman selector declarations across versions, records, blocks, CRC and fixed buffers; reserved selectors rejected even in empty regions\n";
}
static void HuffmanBitLimits() {
    using history::mp3_huffman_detail::BoundedBits;
    using history::mp3_huffman_detail::DecodePair;
    using history::mp3_huffman_detail::TryDecodeCount1;
    uint8_t zeros[4]{};

    // The all-zero table-16 path decodes magnitudes (14, 15), then consumes
    // one linbit and both signs. Table 31 uses the 13-bit escape form.
    BoundedBits table16(zeros,sizeof(zeros),0,10);
    assert(DecodePair(table16,16)&&table16.position()==10);
    BoundedBits table16_short(zeros,sizeof(zeros),0,9);
    assert(!DecodePair(table16_short,16));
    BoundedBits table31(zeros,sizeof(zeros),0,21);
    assert(DecodePair(table31,31)&&table31.position()==21);
    BoundedBits table31_short(zeros,sizeof(zeros),0,20);
    assert(!DecodePair(table31_short,31));

    // Count1 table 0's zero-prefix symbol needs three signs; table 1's
    // zero-prefix symbol has four nonzero coefficients and needs four signs.
    BoundedBits count1_table0(zeros,sizeof(zeros),0,9);
    assert(TryDecodeCount1(count1_table0,0)&&count1_table0.position()==9);
    BoundedBits count1_table0_short(zeros,sizeof(zeros),0,8);
    assert(!TryDecodeCount1(count1_table0_short,0));
    BoundedBits count1_table1(zeros,sizeof(zeros),0,8);
    assert(TryDecodeCount1(count1_table1,1)&&count1_table1.position()==8);
    BoundedBits count1_table1_short(zeros,sizeof(zeros),0,7);
    assert(!TryDecodeCount1(count1_table1_short,1));
    std::cout<<"PASS: bounded linbits escapes and count1 code/sign limits\n";
}
static void EntropyPayload() {
    Mp3Layout info;const char* error=nullptr;
    // Table 1's all-zero prefix selects a 3-bit (1,1) pair and two sign bits.
    auto pair=Frame();
    constexpr unsigned record=20,switched=record+33,payload=36;
    SetBits(pair,4,record,12,5);
    SetBits(pair,4,record+12,9,1);
    SetBits(pair,4,switched+1,5,1);
    assert(InspectMp3Layout(pair,info,error));
    SetBits(pair,4,record,12,4);
    Reject(pair,"truncated or invalid MP3 Huffman pair");

    // The first long band has two pairs at 44.1 kHz; the third pair must use
    // region 1's table. Table 0 consumes no bits, then table 1 consumes five.
    auto long_region=Frame();
    SetBits(long_region,4,record,12,5);
    SetBits(long_region,4,record+12,9,3);
    SetBits(long_region,4,switched+6,5,1);
    SetBits(long_region,payload,0,5,0);
    assert(InspectMp3Layout(long_region,info,error));

    auto switched_region=[&](unsigned block,bool mixed,unsigned big_values) {
        auto frame=Frame();
        SetBits(frame,4,record,12,5);
        SetBits(frame,4,record+12,9,big_values);
        SetBits(frame,4,switched,1,1);
        SetBits(frame,4,switched+1,2,block);
        SetBits(frame,4,switched+3,1,mixed);
        SetBits(frame,4,switched+9,5,1);
        SetBits(frame,payload,0,5,0);
        if(!InspectMp3Layout(frame,info,error)) {
            std::cerr<<"switched block "<<block<<": "<<(error?error:"no error")<<'\n';
            assert(false);
        }
    };
    // At 44.1 kHz, start/stop blocks cross after eight long bands (18 pairs),
    // ordinary short blocks after nine short bands, and mixed short blocks
    // after eight mixed bands. The next pair must use region 1 in every case.
    switched_region(1,false,19);
    switched_region(2,false,19);
    switched_region(2,true,19);
    switched_region(3,false,19);
    std::cout<<"PASS: exact Huffman bit limits and long/start/short region boundaries\n";
}
int main(int argc,char** argv) {
    if(argc==2||(argc==3&&std::strcmp(argv[1],"--gapless")==0)) {
        const bool show_gapless=argc==3;
        std::ifstream input(argv[show_gapless?2:1],std::ios::binary);assert(input);
        Bytes bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        Mp3Layout info;const char* error=nullptr;
        if(!InspectMp3Layout(bytes,info,error)){std::cerr<<error<<"\n";return 1;}
        if(show_gapless) {
            std::cout<<unsigned(info.xing.gapless_present)<<' '<<info.xing.encoder_delay<<' '
                     <<info.xing.encoder_padding<<' '<<info.xing.gapless_samples<<'\n';
            return 0;
        }
        std::cout<<info.rate<<' '<<info.channels<<' '<<info.frames<<' '<<info.nominal_samples<<' '<<info.audio_offset<<' '<<info.audio_size<<'\n';
        return 0;
    }
    assert(argc==1);Scalefactors();Xing();Vbri();HuffmanSelectors();HuffmanBitLimits();EntropyPayload();Mp3Layout info;const char* error="stale";
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
        for(unsigned index:{4u,5u,6u}) {auto bad=tagged;bad.at(index)=index==6?128:1;Reject(bad);}
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
    std::cout<<"PASS: bounded MP3 layout and Huffman syntax, protected CRC, side constraints, reservoir extents, ID3 extents/footer, fixed blocks, truncation and mutations; not an audio decoder\n";
    return 0;
}
