#pragma once
#include "ogg_history_core.h"
#include "existing_quality_core.h"
#include "spotify_identity.h"
#include "vorbis_comments.h"
#include <cstring>
#include <vector>
#include <algorithm>
namespace history {
enum class IdentityState { Unknown, Untagged, Tagged };
struct RecordingIdentity { IdentityState state=IdentityState::Unknown; std::string uri; };
namespace identity_detail {
inline bool Field(RecordingIdentity& out,std::string key,const std::string& value) {
    for(char& c:key)if(c>='a'&&c<='z')c=char(c-'a'+'A');
    if(key!="SPOTIFY_URI")return true;
    if(!ValidSpotifyUri(value,"track")&&!ValidSpotifyUri(value,"episode"))return false;
    if(out.state==IdentityState::Tagged&&out.uri!=value)return false;
    out={IdentityState::Tagged,value};return true;
}
inline RecordingIdentity Comments(const unsigned char* b,size_t n,bool vorbis) {
    RecordingIdentity out{IdentityState::Untagged,{}};
    VorbisComments parsed;
    if(!ParseVorbisComments(b,n,vorbis,parsed))return {};
    for(const auto& field:parsed.fields) {
        const auto eq=field.find('=');
        if(eq!=std::string::npos&&!Field(out,field.substr(0,eq),field.substr(eq+1)))return {};
    }
    return out;
}
}
// Parse only structural metadata, never search audio payload for an identity.
// Input is bounded by the filesystem reader. Unknown includes truncated metadata.
inline RecordingIdentity ParseRecordingIdentity(const unsigned char* b,size_t n) {
    using namespace identity_detail;
    if(!b||n<4)return {};
    if(!std::memcmp(b,"fLaC",4)||!std::memcmp(b,"OggS",4)) {
        Quality quality;if(!ParseExistingQualityHeader(b,n,quality))return {};
    }
    if(!std::memcmp(b,"fLaC",4)) {
        size_t at=4;bool first=true,seen=false;RecordingIdentity result{IdentityState::Untagged,{}};
        while(at<n) {
            if(n-at<4)return {};
            unsigned type=b[at]&127;bool last=(b[at]&128)!=0;
            size_t length=(size_t(b[at+1])<<16)|(size_t(b[at+2])<<8)|b[at+3];at+=4;
            if(length>n-at||type==127||(first&&(type!=0||length!=34))||(!first&&type==0))return {};
            first=false;
            if(type==4) {if(seen)return {};seen=true;result=Comments(b+at,length,false);if(result.state==IdentityState::Unknown)return {};}
            at+=length;if(last)return result;
        }
        return {};
    }
    if(!std::memcmp(b,"OggS",4)) {
        size_t at=0;uint32_t serial=0,sequence=0;unsigned packets=0;std::vector<unsigned char> packet;
        while(n-at>=27) {
            size_t header=27+b[at+26];if(header>n-at)return {};
            size_t body=0;for(size_t i=27;i<header;++i)body+=b[at+i];
            if(body>n-at-header)return {};
            Page page;if(!ParsePage(b+at,header+body,page))return {};
            if(at==0){if(!page.vorbis_start||page.sequence>1)return {};serial=page.serial;}
            else if(page.serial!=serial||page.sequence!=sequence||(page.flags&2))return {};
            if(bool(page.flags&1)!=!packet.empty())return {};
            sequence=page.sequence+1;
            size_t pos=at+header;
            for(size_t i=27;i<header;++i) {
                const size_t len=b[at+i];packet.insert(packet.end(),b+pos,b+pos+len);pos+=len;
                if(len<255) {
                    ++packets;
                    if(packets==1&&(packet.size()!=30||std::memcmp(packet.data(),"\x01vorbis",7)))return {};
                    if(packets==2) {
                        if(packet.size()<7||std::memcmp(packet.data(),"\x03vorbis",7))return {};
                        return Comments(packet.data()+7,packet.size()-7,true);
                    }
                    packet.clear();
                }
            }
            at+=header+body;
        }
        return {};
    }
    // FFmpeg MP3 output uses ID3 TXXX user text frames (v2.3 often UTF-16).
    if(n>=10&&!std::memcmp(b,"ID3",3)) {
        if((b[3]!=3&&b[3]!=4)||b[4]||b[5])return {};
        size_t length=0;for(size_t i=6;i<10;++i){if(b[i]&128)return {};length=(length<<7)|b[i];}
        if(length>n-10)return {};
        const size_t end=10+length;size_t at=10;RecordingIdentity result{IdentityState::Untagged,{}};
        while(at<end) {
            if(!b[at]) {for(;at<end;++at)if(b[at])return {};break;}
            if(end-at<10)return {};
            for(size_t i=0;i<4;++i)if(!((b[at+i]>='A'&&b[at+i]<='Z')||(b[at+i]>='0'&&b[at+i]<='9')))return {};
            size_t size=0;for(size_t i=4;i<8;++i){if(b[3]==4&&(b[at+i]&128))return {};size=(size<<(b[3]==4?7:8))|b[at+i];}
            if(b[at+8]||b[at+9]||size>end-at-10)return {};
            bool text=!std::memcmp(b+at,"TXXX",4);at+=10;
            if(text) {
                if(size<2||b[at]>3)return {};
                std::string decoded;
                if(b[at]==0||b[at]==3)decoded.assign(reinterpret_cast<const char*>(b+at+1),size-1);
                else {
                    if((size-1)%2)return {};
                    bool little=false;size_t pos=1;
                    if(b[at]==1) {
                        if(size<3)return {};
                        if(b[at+1]==255&&b[at+2]==254)little=true;
                        else if(b[at+1]!=254||b[at+2]!=255)return {};
                        pos=3;
                    }
                    for(;pos+1<size;pos+=2) {
                        unsigned unit=little?unsigned(b[at+pos])|(unsigned(b[at+pos+1])<<8):
                            (unsigned(b[at+pos])<<8)|unsigned(b[at+pos+1]);
                        if(unit==0xfeff)continue;
                        if(unit==0xfffe){little=!little;continue;}
                        // Identity keys/values are ASCII; unrelated Unicode text is ignored.
                        decoded+=unit<128?char(unit):'?';
                    }
                }
                const auto split=decoded.find('\0');if(split==std::string::npos)return {};
                std::string key=decoded.substr(0,split),value=decoded.substr(split+1);
                while(!value.empty()&&!value.back())value.pop_back();
                if(!Field(result,key,value))return {};
            }
            at+=size;
        }
        return result;
    }
    return {};
}
}
