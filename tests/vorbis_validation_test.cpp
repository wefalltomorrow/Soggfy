#include "../native/vorbis_validation.h"
#include "../native/ogg_history_core.h"
#include <ogg/ogg.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
using namespace history;
using Bytes=std::vector<uint8_t>;
static void Reject(const Bytes& source) {
    VorbisDecodedCoverage output{123,456,789,2};std::string error;
    assert(!DecodeVorbisRecording(source,output,error));
    assert(!error.empty()&&!output.samples&&!output.rate&&!output.channels&&!output.packets);
}
static size_t PageSize(const Bytes& source,size_t at) {
    assert(source.size()-at>=27);size_t header=27+source[at+26],size=header;
    assert(header<=source.size()-at);
    for(size_t i=27;i<header;++i)size+=source[at+i];
    assert(size<=source.size()-at);return size;
}
static void Crc(Bytes& source,size_t at) {
    const size_t header=27+source[at+26],size=PageSize(source,at);
    ogg_page page={source.data()+at,long(header),source.data()+at+header,long(size-header)};
    ogg_page_checksum_set(&page);
}
static Bytes ShortFirstAudio(const Bytes& source,size_t audio,size_t kept) {
    Bytes bad=source;const size_t header=27+bad[audio+26];size_t length=0,segments=0;
    do {assert(segments<bad[audio+26]);length+=bad[audio+27+segments++];}
    while(bad[audio+26+segments]==255);
    if(length<=kept)return {};
    assert(kept<255);
    bad.erase(bad.begin()+audio+header+kept,bad.begin()+audio+header+length);
    bad.erase(bad.begin()+audio+28,bad.begin()+audio+27+segments);
    bad[audio+26]-=uint8_t(segments-1);bad[audio+27]=uint8_t(kept);
    Crc(bad,audio);return bad;
}
int main(int argc,char** argv) {
    Reject({});Reject({1,2,3,4});
    assert(argc==2);
    std::ifstream input(argv[1],std::ios::binary);assert(input);
    Bytes source{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    VorbisDecodedCoverage output;std::string error;
    if(!DecodeVorbisRecording(source,output,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    assert(output.samples>0&&output.packets>=2&&error.empty());
    std::vector<size_t> pages;size_t last=0,audio=0,headers=0;
    for(size_t at=0;at<source.size();) {
        pages.push_back(at);last=at;
        if(headers>=3&&!audio)audio=at;
        for(size_t i=27;i<27u+source[at+26];++i)if(source[at+i]<255)++headers;
        at+=PageSize(source,at);
    }
    assert(audio>0);
    Bytes bad=source;bad.push_back(0);Reject(bad);
    bad=source;bad.pop_back();Reject(bad);
    bad=source;bad.back()^=128;Reject(bad);
    for(const size_t at:pages) {
        Reject(Bytes(source.begin(),source.begin()+at));
        Reject(Bytes(source.begin(),source.begin()+at+26));
    }
    // Recompute CRCs so these exercise decoder/coverage checks rather than CRC.
    bad=source;bad[audio+27+bad[audio+26]]|=1;Crc(bad,audio);Reject(bad);
    bad=source;bad[audio+18]+=2;Crc(bad,audio);Reject(bad);
    bad=source;bad[audio+5]|=1;Crc(bad,audio);Reject(bad);
    bad=source;uint64_t granule=0;
    for(unsigned i=0;i<8;++i)granule|=uint64_t(bad[last+6+i])<<(8*i);
    granule+=8192;for(unsigned i=0;i<8;++i)bad[last+6+i]=uint8_t(granule>>(8*i));
    Crc(bad,last);Reject(bad);
    // Retain headers and EOS, but remove the first audio packet's content.
    Reject(ShortFirstAudio(source,audio,0));
    const auto truncated=ShortFirstAudio(source,audio,1);
    if(!truncated.empty())Reject(truncated); // truncated codec bits, valid container
    // Cancellation must clear a previous success, both before and during decode.
    assert(!DecodeVorbisRecording(source,output,error,[]{return true;}));
    assert(!output.samples&&error=="Vorbis validation canceled");
    unsigned calls=0;assert(!DecodeVorbisRecording(source,output,error,[&]{return ++calls==6;}));
    assert(!output.samples&&error=="Vorbis validation canceled");
    std::puts("PASS: reference Vorbis decode, exact coverage, corruption and cancellation");
}
