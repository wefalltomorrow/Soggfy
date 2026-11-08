#pragma once
#include "metadata_refresh_plan.h"
#include "flac_history_core.h"
#include "ogg_history_core.h"
#include "existing_quality_core.h"
#include <cstring>

namespace history {
enum class NativeRecordingFormat { Unknown, Ogg, Flac };
struct MetadataInspection {
    NativeRecordingFormat format=NativeRecordingFormat::Unknown;
    VorbisComments comments;
    uint64_t samples=0;
    unsigned rate=0,channels=0,bits=0;
};
// Complete-container structural inspection only. This deliberately does not
// claim audio validity: the caller must decode the entire stable source and
// check decoded coverage before authorizing a metadata transaction.
inline bool InspectMetadataRefreshSource(const std::vector<uint8_t>& source,
    const std::string& expected_uri,MetadataInspection& output) {
    output={};MetadataInspection found;
    if(source.size()<4)return false;
    Quality quality;if(!ParseExistingQualityHeader(source.data(),source.size(),quality))return false;
    if(!std::memcmp(source.data(),"fLaC",4)) {
        FlacInfo info;size_t audio=0;
        if(ParseFlacMetadata(source,info,audio)!=FlacParse::Valid||
           audio>VorbisCommentByteLimit||source.size()-audio<2||!info.total_samples)return false;
        bool comments=false;size_t at=4;
        while(at<audio) {
            const unsigned kind=source[at]&127;
            const size_t length=(size_t(source[at+1])<<16)|(size_t(source[at+2])<<8)|source[at+3];
            at+=4;
            if(kind==4) {
                if(comments||!ParseVorbisComments(source.data()+at,length,false,found.comments))return false;
                comments=true;
            }
            at+=length;
        }
        if(!comments)return false;
        found.format=NativeRecordingFormat::Flac;found.samples=info.total_samples;
        found.rate=info.rate;found.channels=info.channels;found.bits=info.bits;
    } else if(!std::memcmp(source.data(),"OggS",4)) {
        Stream stream;std::vector<uint8_t> packet;size_t at=0,packets=0;
        bool pending=false;
        while(at<source.size()) {
            if(stream.complete||source.size()-at<27)return false;
            const auto* b=source.data()+at;const size_t header=27+b[26];
            if(header>source.size()-at)return false;
            size_t body=0;for(size_t i=27;i<header;++i)body+=b[i];
            if(body>source.size()-at-header)return false;
            Page page;if(!ParsePage(b,header+body,page))return false;
            if(!at) {if(!page.vorbis_start||page.sequence>1)return false;}
            else if(page.serial!=stream.serial||page.sequence!=stream.next||(page.flags&2))return false;
            if(bool(page.flags&1)!=pending)return false;
            const auto state=stream.Push(b,header+body);
            if(state==Result::Invalid||state==Result::Ignore||state==Result::Replay)return false;
            size_t pos=header;
            for(size_t i=27;i<header;++i) {
                const size_t length=b[i];
                // Retain only the three headers. Audio is scanned without
                // allocating a second copy of the recording.
                if(packets<3) {
                    if(length>VorbisCommentByteLimit-packet.size())return false;
                    packet.insert(packet.end(),b+pos,b+pos+length);
                }
                pos+=length;pending=length==255;
                if(pending)continue;
                if(packets==0&&(packet.size()!=30||packet[0]!=1||std::memcmp(packet.data()+1,"vorbis",6)))return false;
                if(packets==1&&(packet.size()<7||packet[0]!=3||std::memcmp(packet.data()+1,"vorbis",6)||
                   !ParseVorbisComments(packet.data()+7,packet.size()-7,true,found.comments)))return false;
                if(packets==2&&(packet.size()<7||packet[0]!=5||std::memcmp(packet.data()+1,"vorbis",6)))return false;
                ++packets;packet.clear();
            }
            if((page.flags&4)&&pending)return false;
            at+=header+body;
        }
        if(!stream.complete||pending||packets<4||stream.samples<=0)return false;
        found.format=NativeRecordingFormat::Ogg;found.samples=uint64_t(stream.samples);
        found.rate=stream.rate;found.channels=stream.channels;
    } else return false;
    Tags identity;identity.fields.emplace_back("SPOTIFY_URI",expected_uri);Tags unused;
    if(PlanMetadataRefresh(found.comments,expected_uri,identity,unused)==MetadataPlanState::Rejected)return false;
    // Require exact agreement with this container, including every duplicate.
    metadata_plan_detail::Fields fields;
    for(const auto& field:found.comments.fields) {
        const auto eq=field.find('=');
        if(eq==std::string::npos)fields[metadata_plan_detail::Key(field)].push_back("<opaque field>");
        else fields[metadata_plan_detail::Key(field.substr(0,eq))].push_back(field.substr(eq+1));
    }
    using metadata_plan_detail::Exact;
    if(!Exact(fields,"HISTORY_CAPTURE",found.format==NativeRecordingFormat::Ogg?
        "native compressed Ogg pages":"native compressed FLAC input")||
       !Exact(fields,"HISTORY_SAMPLES",std::to_string(found.samples))||
       !Exact(fields,"HISTORY_SAMPLE_RATE",std::to_string(found.rate))||
       !Exact(fields,"HISTORY_BITS_PER_SAMPLE",std::to_string(found.bits)))return false;
    output=std::move(found);return true;
}
}
