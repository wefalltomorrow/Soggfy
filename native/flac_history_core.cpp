#include "flac_history_core.h"
#include "vorbis_comments.h"
#include <cstring>
namespace history {
static bool StreamInfo(const uint8_t* b,FlacInfo& info) {
            info.min_block=(unsigned(b[0])<<8)|b[1]; info.max_block=(unsigned(b[2])<<8)|b[3];
            uint64_t packed=0; for(unsigned i=10;i<18;i++) packed=(packed<<8)|b[i];
            info.rate=unsigned(packed>>44); info.channels=unsigned((packed>>41)&7)+1;
            info.bits=unsigned((packed>>36)&31)+1; info.total_samples=packed&0xfffffffffULL;
            if(!info.rate || info.rate>655350 || info.bits<4 || info.min_block<16 || info.max_block<info.min_block) return false;
    return true;
}
FlacParse ParseFlacStreamInfo(const uint8_t* source,size_t length,FlacInfo& info) {
    if(length<4)return FlacParse::NeedMore;
    if(!source || memcmp(source,"fLaC",4))return FlacParse::Invalid;
    if(length<8)return FlacParse::NeedMore;
    if((source[4]&127)!=0 || source[5]!=0 || source[6]!=0 || source[7]!=34)return FlacParse::Invalid;
    if(length<42)return FlacParse::NeedMore;
    return StreamInfo(source+8,info)?FlacParse::Valid:FlacParse::Invalid;
}
template<class Source> static FlacParse Metadata(const Source& source,FlacInfo& info,size_t& audio) {
    audio=0;
    if(source.size()<4) return FlacParse::NeedMore;
    for(unsigned i=0;i<4;i++) if(source[i]!=uint8_t("fLaC"[i])) return FlacParse::Invalid;
    size_t p=4; bool first=true;
    for(;;) {
        if(p>16*1024*1024) return FlacParse::Invalid;
        if(source.size()-p<4) return FlacParse::NeedMore;
        unsigned kind=source[p]&127; bool last=(source[p]&128)!=0;
        size_t n=(size_t(source[p+1])<<16)|(size_t(source[p+2])<<8)|source[p+3];
        if(kind==127 || (first && (kind!=0 || n!=34)) || (!first && kind==0)) return FlacParse::Invalid;
        p+=4;
        if(n>source.size()-p) return FlacParse::NeedMore;
        if(first) {
            uint8_t b[34]; for(unsigned i=0;i<34;i++) b[i]=source[p+i];
            if(!StreamInfo(b,info)) return FlacParse::Invalid;
            first=false;
        }
        p+=n;
        if(last) { audio=p; return FlacParse::Valid; }
    }
}
FlacParse ParseFlacMetadata(const std::vector<uint8_t>& source,FlacInfo& info,size_t& audio) { return Metadata(source,info,audio); }
FlacParse ParseFlacMetadata(const CompressedBuffer& source,FlacInfo& info,size_t& audio) { return Metadata(source,info,audio); }
bool FlacCoverage::Frame(const FlacInfo& info,unsigned block,unsigned rate,unsigned channels,unsigned bits,unsigned number_type,uint64_t number) {
    if(!block || block>info.max_block || rate!=info.rate || channels!=info.channels || bits!=info.bits || number_type>1) return false;
    if(number_type==0 && (!info.max_block || number>info.total_samples/info.max_block)) return false;
    uint64_t position=number_type==1 ? number : number*info.max_block;
    if(position!=samples || !info.total_samples || samples>info.total_samples || block>info.total_samples-samples) return false;
    samples+=block; ++frames; return true;
}
static std::string Upper(std::string s) { for(char& c:s) if(c>='a' && c<='z') c=char(c-'a'+'A'); return s; }
static bool Fields(const uint8_t* b,size_t n,const Tags& tags,bool replace_picture,std::string& vendor,std::vector<std::string>& fields) {
    VorbisComments parsed;
    if(!ParseVorbisComments(b,n,false,parsed))return false;
    vendor=std::move(parsed.vendor);
    for(auto& s:parsed.fields) {
        auto key=Upper(s.substr(0,s.find('=')));
        bool replace=key=="METADATA_BLOCK_PICTURE" && replace_picture;
        for(const auto& tag:tags.fields) if(key==Upper(tag.first)) replace=true;
        if(!replace) fields.push_back(std::move(s));
    }
    return true;
}
bool TagFlac(const std::vector<uint8_t>& source,const Tags& tags,std::vector<uint8_t>& output,std::string& error) {
    output.clear(); error.clear();
    auto fail=[&](const char* why) { error=why; output.clear(); return false; };
    FlacInfo info; size_t audio;
    if(ParseFlacMetadata(source,info,audio)!=FlacParse::Valid || source.size()-audio<2) return fail("invalid or incomplete FLAC metadata/audio");
    std::vector<std::pair<unsigned,std::vector<uint8_t>>> blocks;
    std::vector<std::string> fields; std::string vendor="SpotifyRepair native history";
    std::vector<uint8_t> picture;
    const bool replace_picture=BuildPicture(tags,picture);
    size_t p=4; unsigned comment_blocks=0;
    while(p<audio) {
        unsigned kind=source[p]&127; size_t n=(size_t(source[p+1])<<16)|(size_t(source[p+2])<<8)|source[p+3]; p+=4;
        if(kind==4) {
            if(++comment_blocks>1 || !Fields(source.data()+p,n,tags,replace_picture,vendor,fields)) return fail("invalid FLAC Vorbis comments");
        } else if(kind!=1 && (kind!=6 || !replace_picture)) { // Preserve art without a usable replacement.
            blocks.push_back({kind,std::vector<uint8_t>(source.begin()+p,source.begin()+p+n)});
        }
        p+=n;
    }
    for(const auto& tag:tags.fields) fields.push_back(tag.first+'='+tag.second);
    std::vector<uint8_t> comments;
    if(!BuildVorbisComments(vendor,fields,false,comments))return fail("FLAC comments exceed metadata limits");
    blocks.push_back({4,std::move(comments)});
    if(replace_picture) blocks.push_back({6,std::move(picture)});
    output.reserve(source.size()+tags.cover.size()+65536); output={'f','L','a','C'};
    for(size_t i=0;i<blocks.size();i++) {
        const auto& block=blocks[i]; size_t n=block.second.size(); if(n>0xffffff) return fail("FLAC metadata block too large");
        output.push_back(uint8_t(block.first|(i+1==blocks.size()?128:0)));
        output.push_back(uint8_t(n>>16)); output.push_back(uint8_t(n>>8)); output.push_back(uint8_t(n));
        output.insert(output.end(),block.second.begin(),block.second.end());
    }
    output.insert(output.end(),source.begin()+audio,source.end()); return true;
}
}
