#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace history {
// Append-only journal format. Every field is UTF-8 encoded as hex so newlines,
// Unicode and arbitrary error messages never create extra records. Each line
// has a checksum; truncated/corrupted lines are ignored on the next launch.
struct StatusJournalRow {
    std::string title,artist,album,status,path,message;
};

inline std::uint32_t StatusJournalChecksum(std::string_view s) noexcept {
    std::uint32_t hash=2166136261u;
    for(unsigned char c:s){hash^=c;hash*=16777619u;}
    return hash;
}

inline std::string StatusHexEncode(std::string_view src) {
    constexpr char hex[]="0123456789abcdef";
    std::string out;out.reserve(src.size()*2);
    for(unsigned char c:src){out.push_back(hex[c>>4]);out.push_back(hex[c&15]);}
    return out;
}
inline bool StatusHexDecode(std::string_view src,std::string& out) {
    if(src.size()%2 || src.size()>16384)return false;
    const auto digit=[](char c)->int{
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        if(c>='A'&&c<='F')return c-'A'+10;
        return -1;
    };
    out.clear();out.reserve(src.size()/2);
    for(std::size_t i=0;i<src.size();i+=2){
        const int a=digit(src[i]),b=digit(src[i+1]);
        if(a<0||b<0)return false;
        out.push_back(static_cast<char>((a<<4)|b));
    }
    return true;
}

inline std::string EncodeStatusJournalRow(const StatusJournalRow& row) {
    if(row.title.empty() || (row.status!="DONE"&&row.status!="ERROR"))
        return {};
    std::array<std::string,6> fields{row.title,row.artist,row.album,row.status,row.path,row.message};
    std::string line="SGF1";
    for(const auto& field:fields){
        if(field.size()>8192)return {};
        line.push_back('\t');line+=StatusHexEncode(field);
    }
    char hash[16]{};
    std::snprintf(hash,sizeof(hash),"%08x",StatusJournalChecksum(line));
    line.push_back('\t');line+=hash;line.push_back('\n');
    return line;
}
inline bool DecodeStatusJournalRow(std::string_view line,StatusJournalRow& out) {
    if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);
    if(line.size()<14||line.size()>100000 ||line.substr(0,5)!="SGF1\t")return false;
    const auto final=line.rfind('\t');
    if(final==std::string_view::npos||line.size()-final-1!=8)return false;
    std::uint32_t expected=0;
    const auto hexDigit=[](char c)->int{
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        if(c>='A'&&c<='F')return c-'A'+10;
        return -1;
    };
    for(std::size_t i=final+1;i<line.size();++i){
        const int n=hexDigit(line[i]);
        if(n<0)return false;
        expected=(expected<<4)|static_cast<std::uint32_t>(n);
    }
    if(expected!=StatusJournalChecksum(line.substr(0,final)))return false;
    std::array<std::string,6> values{};
    std::size_t start=5;
    for(std::size_t i=0;i<values.size();++i){
        const auto end=line.find('\t',start);
        if(end==std::string_view::npos || (i==values.size()-1 && end!=final) || end>final ||
           !StatusHexDecode(line.substr(start,end-start),values[i]))return false;
        start=end+1;
    }
    if(values[0].empty() || (values[3]!="DONE"&&values[3]!="ERROR"))return false;
    out={values[0],values[1],values[2],values[3],values[4],values[5]};
    return true;
}
}
