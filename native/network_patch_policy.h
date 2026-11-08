#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>
namespace history {
// Restart-only opt-in. Profile APIs can silently truncate long input lines;
// parse a bounded, held file so duplicates cannot disappear behind truncation.
inline bool ReadNetworkPatchText(std::wstring_view text) {
    if(text.find(wchar_t(0))!=std::wstring_view::npos)return false;
    auto trim=[](std::wstring_view value) {
        while(!value.empty()&&(value.front()==L' '||value.front()==L'\t'))value.remove_prefix(1);
        while(!value.empty()&&(value.back()==L' '||value.back()==L'\t'))value.remove_suffix(1);
        return value;
    };
    auto equal=[](std::wstring_view a,std::wstring_view b) {
        return a.size()==b.size()&&!_wcsnicmp(a.data(),b.data(),a.size());
    };
    bool in_section=false,seen=false,enabled=false;
    while(!text.empty()) {
        const auto end=text.find(L'\n');
        auto line=text.substr(0,end);
        if(end==std::wstring_view::npos)text={};else text.remove_prefix(end+1);
        if(!line.empty()&&line.back()==L'\r')line.remove_suffix(1);
        line=trim(line);
        if(line.empty()||line.front()==L';')continue;
        if(line.front()==L'[') {
            if(line.back()!=L']')return false;
            in_section=equal(trim(line.substr(1,line.size()-2)),L"To Disk");continue;
        }
        if(!in_section)continue;
        const auto equals=line.find(L'=');
        const auto key=trim(line.substr(0,equals));
        if(!equal(key,L"NetworkPatch"))continue;
        if(seen)return false;
        seen=true;
        enabled=equals!=std::wstring_view::npos&&trim(line.substr(equals+1))==L"1";
    }
    return enabled;
}
inline bool ReadNetworkPatch(const std::wstring& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    struct Held {HANDLE file;~Held(){CloseHandle(file);}} held{file};
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart>65536)return false;
    std::vector<unsigned char> bytes(size_t(size.QuadPart));DWORD read=0;
    if(!ReadFile(file,bytes.data(),DWORD(bytes.size()),&read,nullptr)||read!=bytes.size())return false;
    std::wstring text;
    if(bytes.size()>=2&&bytes[0]==0xff&&bytes[1]==0xfe) {
        if(bytes.size()%2)return false;
        text.reserve((bytes.size()-2)/2);
        for(size_t i=2;i<bytes.size();i+=2)text.push_back(wchar_t(unsigned(bytes[i])|(unsigned(bytes[i+1])<<8)));
    } else {
        if(bytes.size()>=2&&bytes[0]==0xfe&&bytes[1]==0xff)return false;
        const bool utf8=bytes.size()>=3&&bytes[0]==0xef&&bytes[1]==0xbb&&bytes[2]==0xbf;
        const size_t offset=utf8?3:0;
        const auto data=reinterpret_cast<const char*>(bytes.data()+offset);
        const int length=int(bytes.size()-offset);
        const UINT encoding=utf8?CP_UTF8:CP_ACP;
        const int count=MultiByteToWideChar(encoding,MB_ERR_INVALID_CHARS,data,length,nullptr,0);
        if(count<=0)return false;
        text.resize(size_t(count));
        if(MultiByteToWideChar(encoding,MB_ERR_INVALID_CHARS,data,length,text.data(),count)!=count)return false;
    }
    return ReadNetworkPatchText(text);
}
}
