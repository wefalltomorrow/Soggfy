#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "file_publication.h"
#include "history_settings.h"
namespace history {
static bool WriteNew(const std::wstring& path,const void* bytes,size_t size,bool flush=false) {
    if(!size || size>MAXDWORD) return false;
    auto name=WindowsPath(path);
    HANDLE file=CreateFileW(name.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    DWORD written=0;
    bool ok=WriteFile(file,bytes,DWORD(size),&written,nullptr) && written==size && (!flush||FlushFileBuffers(file));
    CloseHandle(file);
    if(!ok) DeleteFileW(name.c_str());
    return ok;
}
bool ReplaceCompleteFile(const std::wstring& path,const void* bytes,size_t size) {
    static volatile LONG sequence=0;
    const auto staging=path+L".export-"+std::to_wstring(GetCurrentProcessId())+L"-"+
        std::to_wstring(InterlockedIncrement(&sequence));
    if(!WriteNew(staging,bytes,size,true))return false;
    const bool ok=MoveFileExW(WindowsPath(staging).c_str(),WindowsPath(path).c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(WindowsPath(staging).c_str());
    return ok;
}
FilePublication PublishBytes(const std::wstring& path,const Quality& quality,const void* bytes,size_t size) {
    const auto identity=ParseRecordingIdentity(static_cast<const unsigned char*>(bytes),size);
    const auto compatible=[&] {
        if(identity.state!=IdentityState::Tagged)return true;
        const auto existing=ReadRecordingIdentity(path);
        return existing.state==IdentityState::Untagged||(existing.state==IdentityState::Tagged&&existing.uri==identity.uri);
    };
    auto decision=DecideSave(path,quality);
    if(decision!=SaveDecision::NewFile&&!compatible())return FilePublication::Failed;
    if(decision==SaveDecision::Skip) return FilePublication::Skipped;
    if(decision==SaveDecision::NewFile) return WriteNew(path,bytes,size) ? FilePublication::Saved : FilePublication::Failed;
    // Stage the completed file only; never write track fragments to disk.
    static volatile LONG sequence=0;
    auto staging=path+L".complete-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(InterlockedIncrement(&sequence));
    if(!WriteNew(staging,bytes,size,true)) return FilePublication::Failed;
    // Hold the old identity against writes/deletes, then move it by handle.
    // Publish without replacement: a concurrent creator can never be overwritten.
    HANDLE old=CreateFileW(WindowsPath(path).c_str(),GENERIC_READ|DELETE,FILE_SHARE_READ,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    auto rename=[&](const std::wstring& target) {
        const auto name=WindowsPath(target);
        std::vector<unsigned char> storage(sizeof(FILE_RENAME_INFO)+name.size()*sizeof(wchar_t));
        auto info=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
        info->ReplaceIfExists=FALSE;info->RootDirectory=nullptr;
        info->FileNameLength=DWORD(name.size()*sizeof(wchar_t));
        std::memcpy(info->FileName,name.data(),info->FileNameLength);
        return SetFileInformationByHandle(old,FileRenameInfo,info,DWORD(storage.size()))!=0;
    };
    const auto backup=staging+L".previous";
    bool moved=old!=INVALID_HANDLE_VALUE&&DecideSave(path,quality)==SaveDecision::Upgrade&&compatible()&&rename(backup);
    bool ok=moved&&MoveFileExW(WindowsPath(staging).c_str(),WindowsPath(path).c_str(),MOVEFILE_WRITE_THROUGH);
    if(moved) {
        if(ok){FILE_DISPOSITION_INFO disposition{TRUE};SetFileInformationByHandle(old,FileDispositionInfo,&disposition,sizeof(disposition));}
        else rename(path); // If occupied, retain the complete old file at .previous.
    }
    if(old!=INVALID_HANDLE_VALUE)CloseHandle(old);
    if(!ok)DeleteFileW(WindowsPath(staging).c_str());
    return ok ? FilePublication::Upgraded : FilePublication::Failed;
}
}
