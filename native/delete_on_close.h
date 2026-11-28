#pragma once
#include <windows.h>

namespace history {
inline bool MarkOpenFileForDeletion(HANDLE file) noexcept {
    if(file==nullptr||file==INVALID_HANDLE_VALUE)return false;
    FILE_DISPOSITION_INFO disposition{TRUE};
    return SetFileInformationByHandle(file,FileDispositionInfo,&disposition,sizeof(disposition))!=0;
}
}
