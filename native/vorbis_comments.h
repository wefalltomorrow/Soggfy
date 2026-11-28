#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace history {
inline constexpr size_t VorbisCommentByteLimit=16*1024*1024;
inline constexpr size_t VorbisCommentFieldLimit=100000;
struct VorbisComments {
    std::string vendor;
    // Keep raw fields, order, duplicate keys, case and Unicode unchanged.
    std::vector<std::string> fields;
};
// Input starts at the vendor length, after the Ogg packet signature (if any).
// This validates metadata structure only, never audio completeness/ownership.
inline bool ParseVorbisComments(const uint8_t* bytes,size_t size,bool framing,VorbisComments& output) {
    output={};
    if(!bytes||size<8||size>VorbisCommentByteLimit)return false;
    size_t offset=0;
    auto number=[&](uint32_t& value) {
        if(size-offset<4)return false;
        value=uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8)|
            (uint32_t(bytes[offset+2])<<16)|(uint32_t(bytes[offset+3])<<24);
        offset+=4;return true;
    };
    auto field=[&](std::string* value) {
        uint32_t length=0;
        if(!number(length)||length>size-offset)return false;
        if(value)value->assign(reinterpret_cast<const char*>(bytes+offset),length);
        offset+=length;return true;
    };
    // Validate all lengths before allocating any strings or publishing output.
    if(!field(nullptr))return false;
    uint32_t count=0;
    if(!number(count)||count>VorbisCommentFieldLimit||count>(size-offset)/4)return false;
    const size_t fields_start=offset;
    for(uint32_t i=0;i<count;++i)if(!field(nullptr))return false;
    if(framing?(size-offset!=1||bytes[offset]!=1):offset!=size)return false;
    VorbisComments parsed;
    offset=0;field(&parsed.vendor);
    offset=fields_start;parsed.fields.resize(count);
    for(auto& value:parsed.fields)field(&value);
    output=std::move(parsed);return true;
}
inline bool BuildVorbisComments(const std::string& vendor,const std::vector<std::string>& fields,
                               bool framing,std::vector<uint8_t>& output) {
    output.clear();
    if(fields.size()>VorbisCommentFieldLimit)return false;
    size_t size=8+size_t(framing);
    if(vendor.size()>VorbisCommentByteLimit-size)return false;
    size+=vendor.size();
    for(const auto& field:fields) {
        if(VorbisCommentByteLimit-size<4||field.size()>VorbisCommentByteLimit-size-4)return false;
        size+=4+field.size();
    }
    output.reserve(size);
    auto number=[&](uint32_t value) {
        for(unsigned i=0;i<4;++i)output.push_back(uint8_t(value>>(8*i)));
    };
    auto text=[&](const std::string& value) {
        number(uint32_t(value.size()));output.insert(output.end(),value.begin(),value.end());
    };
    text(vendor);number(uint32_t(fields.size()));
    for(const auto& field:fields)text(field);
    if(framing)output.push_back(1);
    return true;
}
}
