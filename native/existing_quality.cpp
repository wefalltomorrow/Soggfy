#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "existing_quality.h"
#include "history_settings.h"
#include "existing_quality_core.h"
namespace history {
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
