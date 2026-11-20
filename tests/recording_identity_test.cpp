#include "../native/recording_identity.h"
#include <cassert>
#include <iostream>
using namespace history;
static void le(std::vector<unsigned char>& b,size_t n){for(unsigned i=0;i<4;++i)b.push_back(static_cast<unsigned char>(n>>(i*8)));}
static std::vector<unsigned char> flac(const std::vector<std::string>& fields) {
    std::vector<unsigned char> comments;le(comments,0);le(comments,fields.size());
    for(const auto& f:fields){le(comments,f.size());comments.insert(comments.end(),f.begin(),f.end());}
    std::vector<unsigned char> b{'f','L','a','C',0,0,0,34};b.resize(42);
    b[8]=b[10]=16;
    const uint64_t info=(uint64_t(44100)<<44)|(uint64_t(1)<<41)|(uint64_t(15)<<36)|44100;
    for(unsigned i=0;i<8;++i)b[18+i]=static_cast<unsigned char>(info>>((7-i)*8));
    b.insert(b.end(),{132,static_cast<unsigned char>(comments.size()>>16),static_cast<unsigned char>(comments.size()>>8),static_cast<unsigned char>(comments.size())});
    b.insert(b.end(),comments.begin(),comments.end());return b;
}
int main() {
    const std::string a="spotify:track:"+std::string(22,'A'),b="spotify:track:"+std::string(22,'B');
    auto bytes=flac({"TITLE=東京","SPOTIFY_URI="+a});
    assert(ParseRecordingIdentity(bytes.data(),bytes.size()).uri==a);
    for(size_t n=0;n<bytes.size();++n)assert(ParseRecordingIdentity(bytes.data(),n).state==IdentityState::Unknown);
    bytes=flac({"SPOTIFY_URI="+a,"SPOTIFY_URI="+b});
    assert(ParseRecordingIdentity(bytes.data(),bytes.size()).state==IdentityState::Unknown);
    bytes=flac({"spotify_uri="+a,"SPOTIFY_URI="+a});
    assert(ParseRecordingIdentity(bytes.data(),bytes.size()).uri==a);
    bytes=flac({"SPOTIFY_URI=spotify:track:short"});
    assert(ParseRecordingIdentity(bytes.data(),bytes.size()).state==IdentityState::Unknown);
    bytes=flac({"TITLE=SPOTIFY_URI="+a});
    assert(ParseRecordingIdentity(bytes.data(),bytes.size()).state==IdentityState::Untagged);
    bytes=flac({});bytes.insert(bytes.end(),a.begin(),a.end());
    assert(ParseRecordingIdentity(bytes.data(),bytes.size()).state==IdentityState::Untagged);
    bytes=flac({"SPOTIFY_URI="+a});bytes[46]=255;
    assert(ParseRecordingIdentity(bytes.data(),bytes.size()).state==IdentityState::Unknown);
    assert(!ValidSpotifyUri("spotify:track:"+std::string(21,'A')+"/","track"));
    for(bool little:{false,true}) {
        std::vector<unsigned char> payload{1};
        payload.push_back(little?255:254);payload.push_back(little?254:255);
        std::string text="SPOTIFY_URI";text+='\0';text+=a;
        for(unsigned char c:text){payload.push_back(little?c:0);payload.push_back(little?0:c);}
        bytes={'I','D','3',3,0,0,0,0,0,0,'T','X','X','X',0,0,0,0,0,0};
        const size_t length=10+payload.size();
        for(unsigned i=0;i<4;++i){bytes[9-i]=static_cast<unsigned char>((length>>(7*i))&127);bytes[17-i]=static_cast<unsigned char>(payload.size()>>(8*i));}
        bytes.insert(bytes.end(),payload.begin(),payload.end());
        assert(ParseRecordingIdentity(bytes.data(),bytes.size()).uri==a);
        for(size_t n=0;n<bytes.size();++n)assert(ParseRecordingIdentity(bytes.data(),n).state==IdentityState::Unknown);
        bytes[21]=0;bytes[22]=0;
        assert(ParseRecordingIdentity(bytes.data(),bytes.size()).state==IdentityState::Unknown);
    }
    std::cout<<"PASS: bounded structural identity, truncation, conflicting tags and payload spoofing\n";
}
