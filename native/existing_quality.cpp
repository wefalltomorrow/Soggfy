#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "existing_quality.h"
#include "history_settings.h"
#include "existing_quality_core.h"
namespace history {
RecordingIdentity ReadRecordingIdentity(const std::wstring& path) {
    HANDLE file=CreateFileW(WindowsPath(path).c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    std::vector<unsigned char> bytes(65536);DWORD read=0;
    if(!ReadFile(file,bytes.data(),DWORD(bytes.size()),&read,nullptr)){CloseHandle(file);return {};}
    auto result=ParseRecordingIdentity(bytes.data(),read);
    if(result.state==IdentityState::Unknown&&read==bytes.size()) {
        bytes.resize(2*1024*1024);DWORD tail=0;
        if(ReadFile(file,bytes.data()+read,DWORD(bytes.size()-read),&tail,nullptr))
            result=ParseRecordingIdentity(bytes.data(),read+tail);
    }
    CloseHandle(file);return result;
}
bool RecordingIdentityCompatible(const std::wstring& path,const std::string& uri,bool allow_legacy) {
    const auto id=ReadRecordingIdentity(path);
    return id.state==IdentityState::Tagged?id.uri==uri:allow_legacy;
}
std::wstring ResolveRecordingPath(const std::wstring& root,const std::wstring& canonical,
    const std::string& uri,const std::wstring& output_extension) {
    if(!ValidSpotifyUri(uri,"track"))return canonical;
    // Search the native and configured converted forms as one destination family.
    auto inspect=[&](const std::wstring& path) {
        bool matching=false,foreign=false,legacy=false;
        auto extensions=std::vector<std::wstring>{L".ogg",L".flac"};
        if(!output_extension.empty())extensions.push_back(output_extension.front()==L'.'?output_extension:L"."+output_extension);
        for(const auto& ext:extensions) {
            const auto candidate=path.substr(0,path.find_last_of(L'.'))+ext;
            DWORD attr=GetFileAttributesW(WindowsPath(candidate).c_str());
            if(attr==INVALID_FILE_ATTRIBUTES) {
                const auto error=GetLastError();
                if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)legacy=true;
                continue;
            }
            const auto id=ReadRecordingIdentity(candidate);
            if(id.state!=IdentityState::Tagged)legacy=true;
            else if(id.uri==uri)matching=true;
            else foreign=true;
        }
        // 0 vacant, 1 exact, 2 legacy/uncertain, 3 foreign, 4 conflicting family.
        return foreign?(matching||legacy?4:3):matching?1:legacy?2:0;
    };
    const int ordinary=inspect(canonical);
    if(ordinary==4)return {};
    if(ordinary!=3)return canonical;
    const auto candidates=DuplicatePaths(root,canonical,uri);
    std::wstring vacant;
    for(const auto& path:candidates) {
        const int state=inspect(path);
        if(state==1)return path;
        if(state==0&&vacant.empty())vacant=path;
    }
    return vacant;
}
bool ReadQuality(const std::wstring& path,Quality& out) {
    auto name=WindowsPath(path);
    HANDLE file=CreateFileW(name.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    unsigned char b[65307]; DWORD n=0;
    bool ok=ReadFile(file,b,sizeof(b),&n,nullptr)!=0; CloseHandle(file);
    return ok&&ParseExistingQualityHeader(b,n,out);
}
SaveDecision DecideSave(const std::wstring& path,const Quality& incoming) {
    auto name=WindowsPath(path); DWORD a=GetFileAttributesW(name.c_str());
    if(a==INVALID_FILE_ATTRIBUTES) return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND ? SaveDecision::NewFile : SaveDecision::Skip;
    Quality existing;
    if((a&FILE_ATTRIBUTE_DIRECTORY) || !ReadQuality(path,existing)) return SaveDecision::Skip;
    return HigherQuality(incoming,existing) ? SaveDecision::Upgrade : SaveDecision::Skip;
}
}
