#include "../native/metadata_refresh_plan.h"
#include "../native/metadata_refresh_inspection.h"
#include "../native/metadata_refresh_verification.h"
#include "../native/flac_history_core.h"
#include <ogg/ogg.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
using namespace history;
using Bytes=std::vector<uint8_t>;
static const std::string uri="spotify:track:0123456789abcdefghijkl";
static VorbisComments Original(const char* capture) {
    return {"custom vendor",{"SPOTIFY_URI="+uri,"HISTORY_COMPLETE_LISTEN=1",
        "HISTORY_TRANSCODED=0",std::string("HISTORY_CAPTURE=")+capture,
        "HISTORY_SAMPLES=44100","HISTORY_SAMPLE_RATE=44100",
        std::string("HISTORY_BITS_PER_SAMPLE=")+(std::string(capture)=="native compressed FLAC input"?"16":"0"),
        "title=original",u8"ARTIST=日本語","artist=second",
        "CUSTOM=value=with=equals","OPAQUE","METADATA_BLOCK_PICTURE=opaque",
        "DATE=2024","GENRE="}};
}
static Tags Incoming() {
    return {{{"SPOTIFY_URI",uri},{"TITLE","replace?"},{"ARTIST","replace?"},
        {"DATE","2024-02-29"},{"YEAR","2024"},{"GENRE","new genre"}}, {1,2,3},"image/png"};
}
static void Check(const VorbisComments& old,const VorbisComments& now) {
    assert(old.vendor==now.vendor);
    std::vector<std::string> retained;
    for(const auto& field:old.fields)if(field!="DATE=2024"&&field!="GENRE=")retained.push_back(field);
    auto expected=retained;
    expected.insert(expected.end(),{"GENRE=new genre","DATE=2024-02-29","YEAR=2024"});
    assert(now.fields==expected);
    Tags delta;assert(PlanMetadataRefresh(now,uri,Incoming(),delta)==MetadataPlanState::Unchanged);
}
struct Packet {Bytes bytes;ogg_int64_t granule;bool eos;};
static std::vector<Packet> ReadOgg(const Bytes& bytes) {
    ogg_sync_state sync;assert(!ogg_sync_init(&sync));
    std::memcpy(ogg_sync_buffer(&sync,long(bytes.size())),bytes.data(),bytes.size());
    assert(!ogg_sync_wrote(&sync,long(bytes.size())));
    ogg_stream_state stream;assert(!ogg_stream_init(&stream,42));
    ogg_page page;ogg_packet packet;std::vector<Packet> packets;int result;
    while((result=ogg_sync_pageout(&sync,&page))==1) {
        assert(!ogg_stream_pagein(&stream,&page));
        int next;while((next=ogg_stream_packetout(&stream,&packet))==1)
            packets.push_back({Bytes(packet.packet,packet.packet+packet.bytes),packet.granulepos,packet.e_o_s!=0});
        assert(next==0);
    }
    assert(result==0);ogg_stream_clear(&stream);ogg_sync_clear(&sync);return packets;
}
static Bytes Ogg(const VorbisComments& comments) {
    Bytes comment;assert(BuildVorbisComments(comments.vendor,comments.fields,true,comment));
    comment.insert(comment.begin(),{3,'v','o','r','b','i','s'});
    Bytes id(30);id[0]=1;std::memcpy(id.data()+1,"vorbis",6);id[11]=2;id[12]=0x44;id[13]=0xac;
    id[28]=0xb8;id[29]=1;
    std::vector<Bytes> packets={id,comment,{5,'v','o','r','b','i','s'},{0,1,2,3,4,5}};
    ogg_stream_state stream;assert(!ogg_stream_init(&stream,42));Bytes bytes;ogg_page page;
    for(size_t i=0;i<packets.size();++i) {
        auto& data=packets[i];ogg_packet packet={data.data(),long(data.size()),i==0,i==3,i==3?44100:0,ogg_int64_t(i)};
        assert(!ogg_stream_packetin(&stream,&packet));
        while(ogg_stream_flush(&stream,&page)) {
            bytes.insert(bytes.end(),page.header,page.header+page.header_len);
            bytes.insert(bytes.end(),page.body,page.body+page.body_len);
        }
    }
    ogg_stream_clear(&stream);return bytes;
}
static void Block(Bytes& bytes,unsigned kind,const Bytes& body) {
    bytes.push_back(uint8_t(kind));
    for(int shift:{16,8,0})bytes.push_back(uint8_t(body.size()>>shift));
    bytes.insert(bytes.end(),body.begin(),body.end());
}
static std::vector<std::pair<unsigned,Bytes>> Blocks(const Bytes& bytes) {
    std::vector<std::pair<unsigned,Bytes>> blocks;size_t at=4;bool last=false;
    while(!last) {
        assert(bytes.size()-at>=4);last=(bytes[at]&128)!=0;unsigned type=bytes[at]&127;
        size_t size=(size_t(bytes[at+1])<<16)|(size_t(bytes[at+2])<<8)|bytes[at+3];at+=4;
        assert(size<=bytes.size()-at);blocks.push_back({type,Bytes(bytes.begin()+at,bytes.begin()+at+size)});at+=size;
    }
    blocks.push_back({128,Bytes(bytes.begin()+at,bytes.end())});return blocks;
}
static void Reject(const Bytes& bytes) {
    MetadataInspection stale;stale.samples=123;stale.comments.vendor="stale";stale.pictures.emplace_back();
    assert(!InspectMetadataRefreshSource(bytes,uri,stale));
    assert(stale.format==NativeRecordingFormat::Unknown&&stale.samples==0&&stale.comments.vendor.empty()&&stale.pictures.empty());
}
static void Crc(Bytes& bytes,size_t at) {
    const size_t header=27+bytes[at+26];size_t body=0;
    for(size_t i=27;i<header;++i)body+=bytes[at+i];
    ogg_page page={bytes.data()+at,long(header),bytes.data()+at+header,long(body)};
    ogg_page_checksum_set(&page);
}
static void Real(const char* input,const char* baseline,const char* refreshed) {
    std::ifstream file(input,std::ios::binary);assert(file);
    Bytes source{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    assert(source.size()>4);const bool flac=source[0]=='f';
    const auto old=Original(flac?"native compressed FLAC input":"native compressed Ogg pages");
    uint64_t samples=0;Quality quality;
    assert(ParseExistingQualityHeader(source.data(),source.size(),quality));
    if(flac) {FlacInfo info;size_t audio;assert(ParseFlacMetadata(source,info,audio)==FlacParse::Valid);samples=info.total_samples;}
    else {
        Stream stream;size_t at=0;
        while(at<source.size()) {
            assert(source.size()-at>=27);size_t size=27+source[at+26];
            assert(size<=source.size()-at);
            for(size_t i=27;i<27u+source[at+26];++i)size+=source[at+i];
            assert(size<=source.size()-at);
            auto result=stream.Push(source.data()+at,size);
            assert(result!=Result::Invalid&&result!=Result::Ignore&&result!=Result::Replay);at+=size;
        }
        assert(stream.complete);samples=uint64_t(stream.samples);
    }
    Tags initial;for(const auto& field:old.fields) {
        auto split=field.find('=');
        if(split!=std::string::npos)initial.fields.emplace_back(field.substr(0,split),field.substr(split+1));
    }
    for(auto& field:initial.fields) {
        if(field.first=="HISTORY_SAMPLES")field.second=std::to_string(samples);
        if(field.first=="HISTORY_SAMPLE_RATE")field.second=std::to_string(quality.rate);
        if(field.first=="HISTORY_BITS_PER_SAMPLE")field.second=std::to_string(quality.bits);
    }
    Bytes tagged,output;std::string error;
    auto tagger=flac?TagFlac:TagOgg;assert(tagger(source,initial,tagged,error));
    MetadataInspection inspection;assert(InspectMetadataRefreshSource(tagged,uri,inspection));
    Tags delta;assert(PlanMetadataRefresh(inspection.comments,uri,Incoming(),delta)==MetadataPlanState::Changed);
    assert(tagger(tagged,delta,output,error));
    assert(VerifyMetadataRefreshOutput(tagged,uri,Incoming(),output));
    assert(!VerifyMetadataRefreshOutput(tagged,uri,Incoming(),tagged));
    MetadataInspection updated;assert(InspectMetadataRefreshSource(output,uri,updated));
    assert(updated.samples==inspection.samples&&updated.rate==inspection.rate&&updated.bits==inspection.bits);
    Check(inspection.comments,updated.comments);
    std::ofstream before(baseline,std::ios::binary);before.write(reinterpret_cast<const char*>(tagged.data()),std::streamsize(tagged.size()));assert(before);
    std::ofstream after(refreshed,std::ios::binary);after.write(reinterpret_cast<const char*>(output.data()),std::streamsize(output.size()));assert(after);
}
int main(int argc,char** argv) {
    if(argc==4){Real(argv[1],argv[2],argv[3]);return 0;}
    assert(argc==1);
    // Synthetic container fixtures test preservation only, not audio decoding.
    auto old=Original("native compressed Ogg pages");auto source=Ogg(old);Tags delta;
    MetadataInspection inspection;
    assert(InspectMetadataRefreshSource(source,uri,inspection));
    assert(inspection.format==NativeRecordingFormat::Ogg&&inspection.samples==44100&&inspection.rate==44100);
    assert(inspection.comments.fields==old.fields);
    assert(inspection.pictures.size()==1&&!inspection.pictures[0].envelope_valid);
    auto artwork=old;
    const std::string wire="AAAAAwAAAAlpbWFnZS9wbmcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAF4";
    artwork.fields.push_back("metadata_block_picture="+wire);
    artwork.fields.push_back("METADATA_BLOCK_PICTURE="+wire);
    MetadataInspection art;assert(InspectMetadataRefreshSource(Ogg(artwork),uri,art));
    assert(art.pictures.size()==3&&!art.pictures[0].envelope_valid);
    for(size_t n:{1u,2u})assert(art.pictures[n].envelope_valid&&art.pictures[n].mime=="image/png"&&art.pictures[n].data_bytes==1);

    assert(!InspectMetadataRefreshSource(source,"spotify:track:abcdefghijkl0123456789",inspection));
    for(size_t n=0;n<source.size();++n)Reject(Bytes(source.begin(),source.begin()+n));
    auto bad=source;bad.push_back(0);Reject(bad);
    bad=source;bad.back()^=1;Reject(bad);
    bad=source;bad[58+18]=3;Crc(bad,58);Reject(bad); // sequence gap
    bad=source;bad[58+18]=0;Crc(bad,58);Reject(bad); // replay
    bad=source;bad[58+5]|=1;Crc(bad,58);Reject(bad); // false continuation
    bad=source;bad[58+14]^=1;Crc(bad,58);Reject(bad); // different serial
    for(const auto& field:{"HISTORY_SAMPLES=44099","HISTORY_SAMPLE_RATE=48000",
        "HISTORY_BITS_PER_SAMPLE=16","HISTORY_CAPTURE=native compressed FLAC input",
        "SPOTIFY_URI=spotify:track:abcdefghijkl0123456789"}) {
        auto conflict=old;conflict.fields.push_back(field);Reject(Ogg(conflict));
    }
    assert(PlanMetadataRefresh(old,uri,Incoming(),delta)==MetadataPlanState::Changed);
    Bytes output;std::string error;assert(TagOgg(source,delta,output,error));
    assert(VerifyMetadataRefreshOutput(source,uri,Incoming(),output));
    bad=output;bad.back()^=1;
    size_t last=0;
    for(size_t at=0;at<bad.size();) {
        last=at;size_t size=27+bad[at+26];
        for(size_t i=27;i<27u+bad[at+26];++i)size+=bad[at+i];
        at+=size;
    }
    Crc(bad,last); // CRC-valid audio mutation must still fail equivalence.
    assert(!VerifyMetadataRefreshOutput(source,uri,Incoming(),bad));
    Tags unauthorized=delta;unauthorized.fields.emplace_back("CUSTOM","replaced");
    assert(TagOgg(source,unauthorized,bad,error));
    assert(!VerifyMetadataRefreshOutput(source,uri,Incoming(),bad));
    unauthorized=delta;unauthorized.cover={1,2,3};unauthorized.mime="image/png";
    assert(TagOgg(source,unauthorized,bad,error));
    assert(!VerifyMetadataRefreshOutput(source,uri,Incoming(),bad)); // envelope validity grants no replacement

    assert(InspectMetadataRefreshSource(output,uri,inspection));
    auto before=ReadOgg(source),after=ReadOgg(output);assert(before.size()==4&&after.size()==4);
    for(size_t i:{0u,2u,3u})assert(before[i].bytes==after[i].bytes&&before[i].eos==after[i].eos);
    assert(before.back().granule==after.back().granule);VorbisComments now;
    assert(ParseVorbisComments(after[1].bytes.data()+7,after[1].bytes.size()-7,true,now));Check(old,now);
    old=Original("native compressed FLAC input");Bytes comments;
    assert(BuildVorbisComments(old.vendor,old.fields,false,comments));
    Bytes info(34);info[0]=info[2]=0x10;
    uint64_t packed=(uint64_t(44100)<<44)|(uint64_t(1)<<41)|(uint64_t(15)<<36)|44100;
    for(unsigned i=0;i<8;++i)info[10+i]=uint8_t(packed>>(56-i*8));
    source={'f','L','a','C'};Block(source,0,info);Block(source,2,{1,2,3,4,5});
    Block(source,6,{8,7,6,5,4,3,2,1});Block(source,128|4,comments);
    source.insert(source.end(),{255,248,1,2,3,4,5,6});
    assert(InspectMetadataRefreshSource(source,uri,inspection));
    assert(inspection.format==NativeRecordingFormat::Flac&&inspection.samples==44100&&inspection.bits==16);
    assert(inspection.pictures.size()==2&&!inspection.pictures[0].envelope_valid&&!inspection.pictures[1].envelope_valid);
    Bytes artBlock;assert(DecodePictureBase64(wire,artBlock));
    Bytes artFlac={'f','L','a','C'};Block(artFlac,0,info);Block(artFlac,6,artBlock);Block(artFlac,128|4,comments);
    artFlac.insert(artFlac.end(),{255,248});
    assert(InspectMetadataRefreshSource(artFlac,uri,art));
    assert(art.pictures.size()==2&&art.pictures[0].envelope_valid&&!art.pictures[1].envelope_valid);
    assert(art.pictures[0].mime=="image/png"&&art.pictures[0].data_bytes==1);

    assert(inspection.comments.fields==old.fields);
    const size_t metadata_end=source.size()-8;
    for(size_t n=0;n<metadata_end+2;++n)Reject(Bytes(source.begin(),source.begin()+n));
    bad=source;bad[25]^=1;Reject(bad); // STREAMINFO sample mismatch
    bad=source;bad[24]=bad[25]=0;Reject(bad); // unknown sample count
    bad={'f','L','a','C'};Block(bad,0,info);Block(bad,4,comments);Block(bad,128|4,comments);
    bad.insert(bad.end(),{255,248});Reject(bad); // ambiguous comments
    assert(PlanMetadataRefresh(old,uri,Incoming(),delta)==MetadataPlanState::Changed);
    assert(TagFlac(source,delta,output,error));
    assert(VerifyMetadataRefreshOutput(source,uri,Incoming(),output));
    bad=output;bad.back()^=1;assert(!VerifyMetadataRefreshOutput(source,uri,Incoming(),bad));
    unauthorized=delta;unauthorized.fields.emplace_back("CUSTOM","replaced");
    assert(TagFlac(source,unauthorized,bad,error));
    assert(!VerifyMetadataRefreshOutput(source,uri,Incoming(),bad));
    unauthorized=delta;unauthorized.cover={1,2,3};unauthorized.mime="image/png";
    assert(TagFlac(source,unauthorized,bad,error));
    assert(!VerifyMetadataRefreshOutput(source,uri,Incoming(),bad)); // envelope validity grants no replacement

    bad=output;bad[46]^=1; // opaque application metadata
    assert(!VerifyMetadataRefreshOutput(source,uri,Incoming(),bad));
    assert(InspectMetadataRefreshSource(output,uri,inspection));
    auto original=Blocks(source),tagged=Blocks(output);assert(original.size()==tagged.size());
    for(size_t i=0;i<original.size();++i) {
        assert(original[i].first==tagged[i].first);
        if(original[i].first==4) {
            assert(ParseVorbisComments(tagged[i].second.data(),tagged[i].second.size(),false,now));Check(old,now);
        } else assert(original[i].second==tagged[i].second);
    }
    std::puts("PASS: planned Ogg/FLAC deltas preserve audio, artwork, vendor, repeated/custom fields and provenance");
}
