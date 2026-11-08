#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace history {
// RFC 9639 section 8.8. These are UNTRUSTED informational dimensions, never
// decoder allocation sizes or proof of image quality. Views borrow the input.
inline constexpr size_t PictureEnvelopeLimit=0xffffff;
struct PictureEnvelope {
    uint32_t type=0,width=0,height=0,depth=0,colors=0;
    std::string_view mime,description,data;
    bool linked=false;
};
// Owning summary for an inspection result; no borrowed views escape it.
struct PictureSummary {
    bool envelope_valid=false,linked=false;
    uint32_t type=0,width=0,height=0,depth=0,colors=0;
    size_t data_bytes=0;
    std::string mime;
};
namespace picture_detail {
inline bool Utf8(std::string_view s) {
    for(size_t i=0;i<s.size();) {
        const auto first=static_cast<unsigned char>(s[i++]);
        if(first<128)continue;
        unsigned more=0;uint32_t value=0,minimum=0;
        if(first>=0xc2&&first<=0xdf){more=1;value=first&31;minimum=0x80;}
        else if(first>=0xe0&&first<=0xef){more=2;value=first&15;minimum=0x800;}
        else if(first>=0xf0&&first<=0xf4){more=3;value=first&7;minimum=0x10000;}
        else return false;
        if(more>s.size()-i)return false;
        while(more--) {
            const auto c=static_cast<unsigned char>(s[i++]);
            if((c&0xc0)!=0x80)return false;
            value=(value<<6)|(c&63);
        }
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
    }
    return true;
}
inline int Digit(unsigned char c) {
    if(c>='A'&&c<='Z')return c-'A';
    if(c>='a'&&c<='z')return c-'a'+26;
    if(c>='0'&&c<='9')return c-'0'+52;
    return c=='+'?62:(c=='/'?63:-1);
}
}
// Structural envelope only: MIME semantics, URI syntax, image decoding,
// source provenance and replacement eligibility are deliberately separate.
// Unknown/reserved types and malformed envelopes must remain opaque to callers.
inline bool ParsePictureEnvelope(const uint8_t* bytes,size_t size,PictureEnvelope& output) {
    output={};
    if(!bytes||size<32||size>PictureEnvelopeLimit)return false;
    PictureEnvelope found;size_t at=0;
    auto number=[&](uint32_t& n) {
        if(size-at<4)return false;
        n=(uint32_t(bytes[at])<<24)|(uint32_t(bytes[at+1])<<16)|
            (uint32_t(bytes[at+2])<<8)|bytes[at+3];at+=4;return true;
    };
    auto field=[&](std::string_view& value) {
        uint32_t length=0;if(!number(length)||length>size-at)return false;
        value=std::string_view(reinterpret_cast<const char*>(bytes+at),length);
        at+=length;return true;
    };
    if(!number(found.type)||found.type>20||!field(found.mime)||found.mime.empty())return false;
    for(unsigned char c:found.mime)if(c<0x20||c>0x7e)return false;
    if(!field(found.description)||!picture_detail::Utf8(found.description)||
       !number(found.width)||!number(found.height)||!number(found.depth)||
       !number(found.colors)||!field(found.data)||at!=size)return false;
    found.linked=found.mime=="-->";
    output=found;return true;
}
// Canonical RFC 4648 base64, as used by METADATA_BLOCK_PICTURE. No whitespace,
// URL alphabet, omitted padding or nonzero padding bits. Decode one bounded
// block at a time. Failure clears output; it does not authorize deleting a tag.
inline bool DecodePictureBase64(std::string_view text,std::vector<uint8_t>& output) {
    output.clear();
    if(text.empty()||text.size()%4||text.size()/4>(PictureEnvelopeLimit+2)/3)return false;
    size_t size=text.size()/4*3;
    if(text.back()=='=')--size;
    if(text[text.size()-2]=='=')--size;
    if(size>PictureEnvelopeLimit)return false;
    std::vector<uint8_t> decoded;decoded.reserve(size);
    for(size_t at=0;at<text.size();at+=4) {
        const int a=picture_detail::Digit(static_cast<unsigned char>(text[at]));
        const int b=picture_detail::Digit(static_cast<unsigned char>(text[at+1]));
        const bool pad2=text[at+2]=='=',pad3=text[at+3]=='=';
        const int c=pad2?0:picture_detail::Digit(static_cast<unsigned char>(text[at+2]));
        const int d=pad3?0:picture_detail::Digit(static_cast<unsigned char>(text[at+3]));
        if(a<0||b<0||c<0||d<0||(pad2&&!pad3)||
           ((pad2||pad3)&&at+4!=text.size())||
           (pad2&&(b&15))||(!pad2&&pad3&&(c&3)))return false;
        const uint32_t n=(uint32_t(a)<<18)|(uint32_t(b)<<12)|(uint32_t(c)<<6)|uint32_t(d);
        decoded.push_back(uint8_t(n>>16));
        if(!pad2)decoded.push_back(uint8_t(n>>8));
        if(!pad3)decoded.push_back(uint8_t(n));
    }
    output=std::move(decoded);return true;
}
inline PictureSummary SummarizePicture(const uint8_t* bytes,size_t size) {
    PictureEnvelope picture;PictureSummary result;
    if(!ParsePictureEnvelope(bytes,size,picture))return result;
    result.envelope_valid=true;result.linked=picture.linked;result.type=picture.type;
    result.width=picture.width;result.height=picture.height;result.depth=picture.depth;
    result.colors=picture.colors;result.data_bytes=picture.data.size();result.mime=picture.mime;
    return result;
}
}
