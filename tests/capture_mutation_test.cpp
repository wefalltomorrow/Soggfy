// Deterministic bounded mutations, independent models and replayable seeds.
// This exercises Soggfy's current boundaries, not unported upstream features.
#include "../native/recording_identity.h"
#include "../native/flac_history_core.h"
#include "../native/client_playback.h"
#include "../native/decoder_context.h"
#include "../native/spotify_hook_discovery.h"
#include <ogg/ogg.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
using namespace history;
using Bytes=std::vector<uint8_t>;
static void le(Bytes& b,size_t at,uint64_t n,unsigned width) {
    assert(at+width<=b.size());
    for(unsigned i=0;i<width;++i)b[at+i]=uint8_t(n>>(8*i));
}
static void crc(Bytes& b) {
    le(b,22,0,4);uint32_t sum=0;
    for(auto c:b){sum^=uint32_t(c)<<24;for(unsigned i=0;i<8;++i)sum=(sum<<1)^((sum&0x80000000)?0x04c11db7:0);}
    le(b,22,sum,4);
}
static Bytes page(unsigned seq,unsigned flags,uint64_t samples,bool start=false) {
    Bytes b(58);std::memcpy(b.data(),"OggS",4);b[5]=uint8_t(flags);
    le(b,6,samples,8);le(b,14,123,4);le(b,18,seq,4);b[26]=1;b[27]=30;
    if(start){std::memcpy(b.data()+28,"\x01vorbis",7);b[39]=2;le(b,40,44100,4);b[56]=0xb8;b[57]=1;}
    crc(b);return b;
}
static Bytes flac() {
    Bytes b(42);std::memcpy(b.data(),"fLaC",4);b[4]=128;b[7]=34;b[8]=b[10]=16;
    uint64_t info=(uint64_t(44100)<<44)|(uint64_t(1)<<41)|(uint64_t(15)<<36)|44100;
    for(unsigned i=0;i<8;++i)b[18+i]=uint8_t(info>>(56-8*i));
    b.insert(b.end(),{0xff,0xf8});return b;
}
static Bytes ogg() {
    ogg_stream_state stream; ogg_stream_init(&stream,42);
    std::vector<uint8_t> input; ogg_page page;
    unsigned char id[30]={}; std::memcpy(id,"\x01vorbis",7); id[11]=2; id[12]=0x44; id[13]=0xac; id[28]=0xb8; id[29]=1;
    unsigned char comment[]={3,'v','o','r','b','i','s',0,0,0,0,0,0,0,0,1};
    unsigned char setup[]={5,'v','o','r','b','i','s'};
    unsigned char audio[]={0,1,2,3,4,5};
    unsigned char* packets[]={id,comment,setup,audio}; long lengths[]={30,16,7,6};
    for(int i=0;i<4;i++) {
        ogg_packet p={packets[i],lengths[i],i==0,i==3,i==3?44100:0,i};
        ogg_stream_packetin(&stream,&p);
        while(ogg_stream_flush(&stream,&page)) {
            input.insert(input.end(),page.header,page.header+page.header_len);
            input.insert(input.end(),page.body,page.body+page.body_len);
        }
    }
    ogg_stream_clear(&stream); return input;
}
static void inspect(const Bytes& b) {
    Page p;ParsePage(b.data(),b.size(),p);
    Quality q;ParseExistingQualityHeader(b.data(),b.size(),q);
    const auto identity=ParseRecordingIdentity(b.data(),b.size());
    if(identity.state==IdentityState::Tagged)
        assert(ValidSpotifyUri(identity.uri,"track")||ValidSpotifyUri(identity.uri,"episode"));
    FlacInfo a,c;size_t off=0,other=0;
    const auto result=ParseFlacMetadata(b,a,off);
    CompressedBuffer chunks;size_t at=0;
    while(at<b.size()){size_t n=std::min(size_t(17),b.size()-at);assert(chunks.Append(b.data()+at,n,65536));at+=n;}
    assert(result==ParseFlacMetadata(chunks,c,other));assert(off==other);
    if(result==FlacParse::Valid){assert(off<=b.size());assert(a.rate==c.rate&&a.total_samples==c.total_samples);}
    assert(chunks.Flatten()==b);
    Tags tags;tags.fields={{"TITLE","test"}};tags.cover={137,80,78,71,13,10,26,10};tags.mime="image/png";Bytes out;std::string error;
    if(TagFlac(b,tags,out,error)){FlacInfo info;size_t audio=0;assert(ParseFlacMetadata(out,info,audio)==FlacParse::Valid);}
    out.clear();TagOgg(b,tags,out,error);
    const std::string text(b.begin(),b.end());ClientPlayback client;
    if(ParseClientPlayback(text,client)){assert(client.instance.empty()||client.instance.size()==32);assert(std::isfinite(client.position));}
    RichMetadata rich;ParseRichMetadata(text,rich,error);
    spotify::HookTargets targets;
    spotify::DiscoverHookTargets(b.data(),b.size(),targets);
}
static void mutate(Bytes& b,std::mt19937& rng) {
    for(unsigned n=1+rng()%8;n;--n) {
        switch(rng()%5) {
        case 0:if(!b.empty())b[rng()%b.size()]^=uint8_t(1u<<(rng()%8));break;
        case 1:if(!b.empty())b.resize(rng()%(b.size()+1));break;
        case 2:if(b.size()<4096)b.insert(b.begin()+rng()%(b.size()+1),uint8_t(rng()));break;
        case 3:if(!b.empty())b[rng()%b.size()]=uint8_t(rng());break;
        default:if(b.size()<4096)b.resize(std::min(size_t(4096),b.size()+rng()%128),uint8_t(rng()));break;
        }
    }
    // Repair some Ogg CRCs to reach semantic validation beyond the checksum.
    if(b.size()>=28&&!std::memcmp(b.data(),"OggS",4)&&(rng()%2))crc(b);
}
static void models(std::mt19937& rng) {
    CompressedBuffer buffer;Bytes expected;
    for(unsigned step=0;step<24;++step) {
        Bytes part(rng()%8192,uint8_t(rng()));size_t before=buffer.size();
        bool ok=buffer.Append(part.data(),part.size(),(rng()%2)?65536:0);
        if(ok)expected.insert(expected.end(),part.begin(),part.end());
        else assert(buffer.size()==before);
        assert(buffer.size()==expected.size());
    }
    assert(!buffer.Append(nullptr,1,65536));assert(buffer.Flatten()==expected);assert(buffer.size()==0);
    CaptureOwner owner;uintptr_t model=0;
    for(unsigned i=0;i<16;++i){uintptr_t observed=rng()%5;bool accept=!observed||!model||model==observed;
        assert(owner.Accept(observed)==accept);if(accept&&observed)model=observed;assert(owner.owner==model);}
    const auto initial=decoder_context;
    {DecoderScope outer(7,9,11);{DecoderScope inner(8,10,12);assert(decoder_context.owner==8);}
     assert(decoder_context.owner==7&&decoder_context.epoch==9&&decoder_context.boundary==11);}
    assert(decoder_context.owner==initial.owner&&decoder_context.active==initial.active);
    const double rate=1+rng()%30;Listen listen;listen.Observe("A",0,30,true,0,rate);
    for(unsigned pos=1;pos<30;++pos)assert(listen.Observe("A",pos,30,true,pos/rate,rate).empty());
    assert(listen.Observe("B",0,30,true,30/rate,rate)=="A");
    Listen skipped;skipped.Observe("A",0,30,true,0,rate);
    assert(skipped.Observe("B",0,30,true,1/rate,rate).empty());
    FlacInfo info;assert(ParseFlacStreamInfo(flac().data(),42,info)==FlacParse::Valid);
    FlacCoverage coverage;uint64_t total=0;
    while(total<info.total_samples){unsigned n=unsigned(std::min(uint64_t(1+rng()%4096),info.total_samples-total));
        assert(coverage.Frame(info,n,info.rate,info.channels,info.bits,1,total));total+=n;}
    assert(coverage.Complete(info));assert(!coverage.Frame(info,1,info.rate,info.channels,info.bits,1,total));
    Stream stream;auto first=page(1,2,0,true),middle=page(2,0,44100),end=page(3,4,88200);
    assert(stream.Push(first.data(),first.size())==Result::Begin);
    assert(stream.Push(middle.data(),middle.size())==Result::Append);
    if(rng()%2)assert(stream.Push(middle.data(),middle.size())==Result::Replay);
    assert(stream.Push(end.data(),end.size())==Result::Complete);assert(stream.Duration()==2);
    Stream gap;gap.Push(first.data(),first.size());assert(gap.Push(end.data(),end.size())==Result::Invalid);
}
int main(int argc,char** argv) {
    const unsigned iterations=argc>1?unsigned(std::stoul(argv[1])):20000;
    const uint32_t seed=argc>2?uint32_t(std::stoul(argv[2])):4043931417u;
    if(!iterations||iterations>1000000)return 2;
    std::mt19937 rng(seed);std::vector<Bytes> corpus={Bytes{},page(1,2,0,true),page(2,0,44100),flac()};
    Tags tags;tags.fields={{"SPOTIFY_URI","spotify:track:"+std::string(22,'A')}};Bytes tagged;std::string error;
    tags.cover={137,80,78,71,13,10,26,10};tags.mime="image/png";
    assert(TagFlac(flac(),tags,tagged,error));corpus.push_back(tagged);
    const std::string heartbeat="v=1&uri=spotify%3Atrack%3A"+std::string(22,'A')+"&title=A&artist=B&album=C&duration=30&instance="+std::string(32,'1')+"&paused=0&buffering=0&local=1&position=0";
    ClientPlayback baseline;assert(ParseClientPlayback(heartbeat,baseline));
    corpus.emplace_back(heartbeat.begin(),heartbeat.end());
    corpus.push_back(ogg());assert(TagOgg(ogg(),tags,tagged,error));
    assert(ParseRecordingIdentity(tagged.data(),tagged.size()).uri==tags.fields[0].second);
    corpus.push_back(tagged);
    Bytes id3={'I','D','3',3,0,0,0,0,0,0,'T','X','X','X',0,0,0,0,0,0};
    Bytes payload={0};std::string field="SPOTIFY_URI";field+='\0';field+=tags.fields[0].second;
    payload.insert(payload.end(),field.begin(),field.end());
    for(unsigned i=0;i<4;++i){id3[9-i]=uint8_t(((10+payload.size())>>(7*i))&127);id3[17-i]=uint8_t(payload.size()>>(8*i));}
    id3.insert(id3.end(),payload.begin(),payload.end());
    assert(ParseRecordingIdentity(id3.data(),id3.size()).uri==tags.fields[0].second);corpus.push_back(id3);
    Bytes pe(1024);pe[0]='M';pe[1]='Z';le(pe,60,128,4);std::memcpy(pe.data()+128,"PE\0\0",4);corpus.push_back(pe);
    for(const auto& source:corpus)for(size_t n=0;n<=source.size();++n)inspect(Bytes(source.begin(),source.begin()+n));
    for(unsigned i=0;i<iterations;++i){
        if(i%1000==0){std::printf("seed=%u iteration=%u/%u\n",seed,i,iterations);std::fflush(stdout);}
        Bytes b=corpus[rng()%corpus.size()];mutate(b,rng);inspect(b);
        if(i%16==0)models(rng);
    }
    std::printf("PASS mutation campaign seed=%u iterations=%u\n",seed,iterations);
}
