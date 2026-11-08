#pragma once
#include "rich_metadata.h"
#include <cstdint>
#include <string>
namespace history {
inline std::string LyricNewlines(const std::string& text) {
    std::string result;result.reserve(text.size());
    for(size_t i=0;i<text.size();++i) {
        if(text[i]=='\r'&&i+1<text.size()&&text[i+1]=='\n')continue;
        result+=text[i];
    }
    return result;
}
// Deliberately accepts only a bounded, line-timed subset of LRC. Custom lyric
// headers, offsets and multiple timestamps remain opaque and are preserved.
inline bool SynchronizedLyricText(const std::string& input,uint64_t duration_ms,std::string& plain) {
    plain.clear();
    if(input.empty()||input.size()>65536||!duration_ms||duration_ms>86400000||!MetadataTextValid(input))return false;
    const auto text=LyricNewlines(input);std::string result;
    size_t at=0,lines=0;uint64_t previous=0;bool words=false;
    while(at<text.size()) {
        const size_t end=text.find('\n',at),stop=end==std::string::npos?text.size():end;
        size_t p=at;unsigned minutes=0,digits=0;
        if(++lines>2000||text[p++]!='[')return false;
        while(p<stop&&text[p]>='0'&&text[p]<='9'&&digits<4) {minutes=minutes*10+unsigned(text[p++]-'0');++digits;}
        if(digits<2||p>=stop||text[p++]!=':'||stop-p<6)return false;
        auto digit=[&](size_t pos){return text[pos]>='0'&&text[pos]<='9';};
        if(!digit(p)||!digit(p+1)||text[p+2]!='.')return false;
        const unsigned seconds=unsigned(text[p]-'0')*10+unsigned(text[p+1]-'0');p+=3;
        if(!digit(p)||!digit(p+1))return false;
        unsigned fraction=(unsigned(text[p]-'0')*10+unsigned(text[p+1]-'0'))*10;p+=2;
        if(p<stop&&digit(p))fraction+=unsigned(text[p++]-'0');
        if(p>=stop||text[p++]!=']'||seconds>=60)return false;
        const uint64_t time=(uint64_t(minutes)*60+seconds)*1000+fraction;
        if(time>duration_ms||(lines>1&&time<previous))return false;
        previous=time;
        if(lines>1)result+='\n';
        result.append(text,p,stop-p);words=words||stop>p;
        if(end==std::string::npos)break;
        at=end+1;
        if(at==text.size())return false; // do not silently discard a final line
    }
    if(!words)return false;
    plain=std::move(result);return true;
}
inline bool PlainLyrics(const std::string& text) {
    if(text.empty()||text.size()>65536||!MetadataTextValid(text))return false;
    // Bracket-led lines may be custom LRC metadata or existing timing.
    return text[0]!='['&&text.find("\n[")==std::string::npos;
}
}
