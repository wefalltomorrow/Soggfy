#include "../native/vorbis_comments.h"
#include <cassert>
#include <cstdio>
using namespace history;
static void number(std::vector<uint8_t>& bytes,uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes.push_back(uint8_t(value>>(i*8)));
}
static void field(std::vector<uint8_t>& bytes,const std::string& value) {
    number(bytes,uint32_t(value.size()));bytes.insert(bytes.end(),value.begin(),value.end());
}
int main() {
    const std::vector<std::string> fields={"ARTIST=first","artist=second",u8"TITLE=日本語",
        "CUSTOM=value=with=equals","MALFORMED-BUT-PRESERVED",std::string("NUL=a\0b",7),"EMPTY="};
    std::vector<uint8_t> plain;field(plain,"custom vendor");number(plain,uint32_t(fields.size()));
    for(const auto& value:fields)field(plain,value);
    VorbisComments parsed;
    assert(ParseVorbisComments(plain.data(),plain.size(),false,parsed));
    assert(parsed.vendor=="custom vendor"&&parsed.fields==fields);
    auto framed=plain;framed.push_back(1);
    assert(ParseVorbisComments(framed.data(),framed.size(),true,parsed)&&parsed.fields==fields);
    std::vector<uint8_t> encoded;
    assert(BuildVorbisComments("custom vendor",fields,false,encoded)&&encoded==plain);
    assert(BuildVorbisComments("custom vendor",fields,true,encoded)&&encoded==framed);
    auto rejected=[&](const uint8_t* bytes,size_t size,bool framing) {
        parsed={"stale",{"SPOTIFY_URI=stale"}};
        assert(!ParseVorbisComments(bytes,size,framing,parsed));
        assert(parsed.vendor.empty()&&parsed.fields.empty());
    };
    for(size_t n=0;n<plain.size();++n)rejected(plain.data(),n,false);
    for(size_t n=0;n<framed.size();++n)rejected(framed.data(),n,true);
    rejected(nullptr,8,false);
    rejected(plain.data(),plain.size(),true);
    rejected(framed.data(),framed.size(),false);
    framed.back()=0;rejected(framed.data(),framed.size(),true);
    framed.back()=3;rejected(framed.data(),framed.size(),true);
    auto bad=plain;bad[0]=bad[1]=bad[2]=bad[3]=255;rejected(bad.data(),bad.size(),false);
    bad=plain;bad.push_back(0);rejected(bad.data(),bad.size(),false);
    std::vector<uint8_t> many;field(many,"");number(many,100001);many.resize(8+4*100001);
    rejected(many.data(),many.size(),false);
    many[4]=0xa0;many[5]=0x86; // 100000 empty fields is the supported boundary.
    many.resize(8+4*100000);
    assert(ParseVorbisComments(many.data(),many.size(),false,parsed)&&parsed.fields.size()==100000);
    // Oversized metadata is rejected before accessing its declared body.
    rejected(plain.data(),16*1024*1024+1,false);
    std::vector<uint8_t> empty(8);
    assert(ParseVorbisComments(empty.data(),empty.size(),false,parsed)&&parsed.fields.empty());
    assert(BuildVorbisComments("",{},false,encoded)&&encoded==empty);
    std::vector<std::string> excessive(VorbisCommentFieldLimit+1);
    assert(!BuildVorbisComments("",excessive,false,encoded)&&encoded.empty());
    const std::string largest_vendor(VorbisCommentByteLimit-8,'x');
    assert(BuildVorbisComments(largest_vendor,{},false,encoded)&&encoded.size()==VorbisCommentByteLimit);
    assert(ParseVorbisComments(encoded.data(),encoded.size(),false,parsed)&&parsed.vendor==largest_vendor);
    assert(!BuildVorbisComments(largest_vendor,{},true,encoded)&&encoded.empty());
    assert(!BuildVorbisComments("",{largest_vendor},false,encoded)&&encoded.empty());
    std::puts("PASS: bounded Vorbis comments, framing, truncation, raw fields and cleared failure output");
}
