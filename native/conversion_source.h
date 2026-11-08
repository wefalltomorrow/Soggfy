#pragma once
#include <windows.h>
#include "history_settings.h"

namespace history { namespace post_process_detail {
struct SourceLock {
    HANDLE handle=INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION original{};
    explicit SourceLock(const std::wstring& path) {
        handle=CreateFileW(WindowsPath(path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle!=INVALID_HANDLE_VALUE&&!GetFileInformationByHandle(handle,&original))Close();
    }
    SourceLock(const SourceLock&)=delete;
    SourceLock& operator=(const SourceLock&)=delete;
    void Close(){if(handle!=INVALID_HANDLE_VALUE){CloseHandle(handle);handle=INVALID_HANDLE_VALUE;}}
    ~SourceLock(){Close();}
    bool DeleteValidated(const std::wstring& path) {
        // Reopen with DELETE access only after FFmpeg has closed its readers.
        // Compare the locked source snapshot, then delete this opened identity.
        Close();
        handle=CreateFileW(WindowsPath(path).c_str(),GENERIC_READ|DELETE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        BY_HANDLE_FILE_INFORMATION current{};
        if(handle==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(handle,&current))return false;
        if(current.dwVolumeSerialNumber!=original.dwVolumeSerialNumber||
           current.nFileIndexHigh!=original.nFileIndexHigh||current.nFileIndexLow!=original.nFileIndexLow||
           current.nFileSizeHigh!=original.nFileSizeHigh||current.nFileSizeLow!=original.nFileSizeLow||
           CompareFileTime(&current.ftLastWriteTime,&original.ftLastWriteTime)!=0||
           CompareFileTime(&current.ftCreationTime,&original.ftCreationTime)!=0)return false;
        FILE_DISPOSITION_INFO disposition{TRUE};
        return SetFileInformationByHandle(handle,FileDispositionInfo,&disposition,sizeof(disposition))!=0;
    }
};
}}
