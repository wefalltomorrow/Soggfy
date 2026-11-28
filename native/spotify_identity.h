#pragma once
#include <string>
namespace history {
inline bool ValidSpotifyUri(const std::string& uri,const char* kind) {
    const std::string prefix=std::string("spotify:")+kind+":";
    if(uri.size()!=prefix.size()+22||uri.compare(0,prefix.size(),prefix)!=0)return false;
    for(size_t i=prefix.size();i<uri.size();++i) {
        const char c=uri[i];
        if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')))return false;
    }
    return true;
}
}
