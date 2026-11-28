#pragma once
#include "metadata_refresh_inspection.h"
#include <ogg/ogg.h>
#include <algorithm>

namespace history {
namespace metadata_verification_detail {
// Both containers have already passed structural inspection. Compare packets
// incrementally so verification does not copy the complete compressed payload.
class Packets {
    const std::vector<uint8_t>& bytes_;
    size_t at_=0;
    ogg_stream_state stream_{};
    bool initialized_=false;
public:
    explicit Packets(const std::vector<uint8_t>& bytes):bytes_(bytes) {
        uint32_t serial=0;for(unsigned i=0;i<4;++i)serial|=uint32_t(bytes_[14+i])<<(8*i);
        initialized_=ogg_stream_init(&stream_,int(serial))==0;
    }
    ~Packets(){if(initialized_)ogg_stream_clear(&stream_);}
    Packets(const Packets&)=delete;
    Packets& operator=(const Packets&)=delete;
    int Next(ogg_packet& packet) {
        if(!initialized_)return -1;
        for(;;) {
            const int next=ogg_stream_packetout(&stream_,&packet);
            if(next)return next;
            if(at_==bytes_.size())return 0;
            const auto* b=bytes_.data()+at_;
            const size_t header=27+b[26];size_t body=0;
            for(size_t i=27;i<header;++i)body+=b[i];
            ogg_page page{const_cast<unsigned char*>(b),long(header),
                const_cast<unsigned char*>(b+header),long(body)};
            if(ogg_stream_pagein(&stream_,&page))return -1;
            at_+=header+body;
        }
    }
};
inline bool OggPayload(const std::vector<uint8_t>& old,const std::vector<uint8_t>& now) {
    if(!std::equal(old.begin()+14,old.begin()+18,now.begin()+14))return false;
    Packets a(old),b(now);ogg_packet left{},right{};size_t index=0;
    for(;;++index) {
        const int x=a.Next(left),y=b.Next(right);
        if(x!=y||x<0)return false;
        if(!x)return index>=4;
        if(index==1)continue; // The only packet allowed to change is comments.
        if(left.bytes!=right.bytes||bool(left.b_o_s)!=bool(right.b_o_s)||
           bool(left.e_o_s)!=bool(right.e_o_s)||
           (index>=3&&left.granulepos!=right.granulepos)||
           (left.bytes&&std::memcmp(left.packet,right.packet,size_t(left.bytes))))return false;
    }
}
struct Block {unsigned kind;size_t at,size;};
inline std::vector<Block> FlacBlocks(const std::vector<uint8_t>& bytes) {
    std::vector<Block> result;size_t at=4;bool last=false;
    while(!last) {
        last=(bytes[at]&128)!=0;const unsigned kind=bytes[at]&127;
        const size_t size=(size_t(bytes[at+1])<<16)|(size_t(bytes[at+2])<<8)|bytes[at+3];
        at+=4;
        if(kind!=1&&kind!=4)result.push_back({kind,at,size}); // padding and comments only
        at+=size;
    }
    result.push_back({128,at,bytes.size()-at});return result;
}
inline bool FlacPayload(const std::vector<uint8_t>& old,const std::vector<uint8_t>& now) {
    const auto a=FlacBlocks(old),b=FlacBlocks(now);
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i) {
        if(a[i].kind!=b[i].kind||a[i].size!=b[i].size||
           !std::equal(old.begin()+a[i].at,old.begin()+a[i].at+a[i].size,now.begin()+b[i].at))return false;
    }
    return true;
}
}
// Independent verification of the permitted delta and byte-exact compressed
// audio/codec headers/artwork. Full source decoding and stable-file ownership
// remain required; structural equivalence is not an audio-validity claim.
inline bool VerifyMetadataRefreshOutput(const std::vector<uint8_t>& source,
    const std::string& uri,const Tags& incoming,const std::vector<uint8_t>& output) {
    MetadataInspection old,now;
    if(!InspectMetadataRefreshSource(source,uri,old)||!InspectMetadataRefreshSource(output,uri,now)||
       old.format!=now.format||old.samples!=now.samples||old.rate!=now.rate||
       old.channels!=now.channels||old.bits!=now.bits)return false;
    Tags delta;
    if(PlanMetadataRefresh(old.comments,uri,incoming,delta)!=MetadataPlanState::Changed||
       !delta.cover.empty())return false;
    std::set<std::string> replaced;
    for(const auto& field:delta.fields)replaced.insert(metadata_plan_detail::Key(field.first));
    std::vector<std::string> expected;
    for(const auto& field:old.comments.fields)
        if(!replaced.count(metadata_plan_detail::Key(std::string_view(field).substr(0,field.find('=')))))
            expected.push_back(field);
    for(const auto& field:delta.fields)expected.push_back(field.first+'='+field.second);
    auto vendor=old.comments.vendor;
    if(vendor.empty()&&old.format==NativeRecordingFormat::Ogg)vendor="SpotifyRepair native history";
    if(now.comments.vendor!=vendor||now.comments.fields!=expected)return false;
    return old.format==NativeRecordingFormat::Ogg?
        metadata_verification_detail::OggPayload(source,output):
        metadata_verification_detail::FlacPayload(source,output);
}
}
