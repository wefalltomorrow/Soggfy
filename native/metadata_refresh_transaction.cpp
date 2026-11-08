#include "metadata_refresh_transaction.h"
#include "validation_protocol.h"
#include <cstring>
namespace history {
namespace {
struct RenameTarget {
    std::vector<unsigned char> storage;
    explicit RenameTarget(const std::wstring& path):storage(sizeof(FILE_RENAME_INFO)+path.size()*sizeof(wchar_t)) {
        auto info=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
        info->ReplaceIfExists=FALSE;info->FileNameLength=DWORD(path.size()*sizeof(wchar_t));
        std::memcpy(info->FileName,path.data(),info->FileNameLength);
    }
    bool Apply(HANDLE file)noexcept {
        return SetFileInformationByHandle(file,FileRenameInfo,storage.data(),DWORD(storage.size()))!=0;
    }
};
bool Dispose(HANDLE file) {
    FILE_DISPOSITION_INFO disposition{TRUE};
    return SetFileInformationByHandle(file,FileDispositionInfo,&disposition,sizeof(disposition))!=0;
}
bool SameFile(const BY_HANDLE_FILE_INFORMATION& a,const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber&&a.nFileIndexHigh==b.nFileIndexHigh&&
        a.nFileIndexLow==b.nFileIndexLow;
}
struct Staging {
    HANDLE handle=INVALID_HANDLE_VALUE;
    bool published=false;
    ~Staging(){if(handle!=INVALID_HANDLE_VALUE){if(!published)Dispose(handle);CloseHandle(handle);}}
};
}
MetadataRefreshTransaction::~MetadataRefreshTransaction() {
    if(source_!=INVALID_HANDLE_VALUE)CloseHandle(source_);
    for(HANDLE directory:directories_)CloseHandle(directory);
}
bool MetadataRefreshTransaction::Open(const std::wstring& path) {
    if(source_!=INVALID_HANDLE_VALUE||used_)return false;
    used_=true; // failed opens also consume this instance
    source_=CreateFileW(path.c_str(),GENERIC_READ|DELETE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(source_==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION original{};
    if(!GetFileInformationByHandle(source_,&original)||original.nNumberOfLinks!=1||
       (original.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return false;
    DWORD count=GetFinalPathNameByHandleW(source_,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!count||count>32767)return false;
    std::vector<wchar_t> name(count+1);
    DWORD written=GetFinalPathNameByHandleW(source_,name.data(),DWORD(name.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!written||written>=name.size())return false;
    path_.assign(name.data(),written);
    // Network/redirected roots need a separately proven publication contract.
    if(path_.size()<8||path_.compare(0,4,L"\\\\?\\")||path_[5]!=L':'||path_[6]!=L'\\')return false;
    for(size_t pos=path_.find(L'\\',7);pos!=std::wstring::npos;pos=path_.find(L'\\',pos+1)) {
        HANDLE directory=CreateFileW(path_.substr(0,pos).c_str(),FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(directory==INVALID_HANDLE_VALUE)return false;
        directories_.push_back(directory);
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(directory,&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||
           (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
    }
    // Ancestor locks were acquired after resolving the source: prove the name
    // still refers to the same held file before allowing any transaction.
    HANDLE check=CreateFileW(path_.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    BY_HANDLE_FILE_INFORMATION current{};
    const bool same=check!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(check,&current)&&SameFile(original,current);
    if(check!=INVALID_HANDLE_VALUE)CloseHandle(check);
    if(!same)return false;
    used_=false;return true;
}
bool MetadataRefreshTransaction::Read(std::vector<uint8_t>& bytes)const {
    bytes.clear();LARGE_INTEGER size{};
    if(used_||source_==INVALID_HANDLE_VALUE||!GetFileSizeEx(source_,&size)||size.QuadPart<=0||
       uint64_t(size.QuadPart)>ValidationInputLimit||!SetFilePointerEx(source_,{},nullptr,FILE_BEGIN))return false;
    bytes.resize(size_t(size.QuadPart));DWORD read=0;
    if(!ReadFile(source_,bytes.data(),DWORD(bytes.size()),&read,nullptr)||read!=bytes.size()) {bytes.clear();return false;}
    return true;
}
RefreshTransactionResult MetadataRefreshTransaction::Commit(const std::vector<uint8_t>& bytes,
    const std::function<bool(RefreshTransactionStage)>& still_current) {
    RefreshTransactionResult result;result.error.reserve(128);
    auto fail=[&](const char* why){result.error=why;return std::move(result);};
    auto current=[&](RefreshTransactionStage stage) {
        try{return still_current&&still_current(stage);}catch(...){return false;}
    };
    if(used_||source_==INVALID_HANDLE_VALUE||bytes.empty()||bytes.size()>ValidationInputLimit)
        return fail("invalid refresh transaction");
    used_=true;
    if(!current(RefreshTransactionStage::Prepared))return fail("refresh canceled before staging");
    static volatile LONG sequence=0;
    const auto staging=path_+L".refresh-"+std::to_wstring(GetCurrentProcessId())+L"-"+
        std::to_wstring(InterlockedIncrement(&sequence));
    const auto backup=staging+L".previous";
    // Allocate all rename/recovery state before changing the namespace.
    RenameTarget destination(path_),previous(backup);
    Staging next;
    next.handle=CreateFileW(staging.c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,FILE_SHARE_READ,
        nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    DWORD written=0;
    if(next.handle==INVALID_HANDLE_VALUE||!WriteFile(next.handle,bytes.data(),DWORD(bytes.size()),&written,nullptr)||
       written!=bytes.size()||!FlushFileBuffers(next.handle))return fail("refresh staging failed");
    if(!current(RefreshTransactionStage::Prepared))return fail("refresh canceled before publication");
    result.retained_original=backup;
    if(!previous.Apply(source_)) {
        result.retained_original.clear();return fail("refresh original rename failed");
    }
    // Every rename forbids replacement. A concurrent creator wins its name;
    // rollback then leaves our original at the explicitly reported backup.
    if(!current(RefreshTransactionStage::SourceMoved)||!destination.Apply(next.handle)) {
        if(destination.Apply(source_))result.retained_original.clear();
        return fail("refresh publication canceled or failed");
    }
    next.published=true;result.committed=true;
    if(Dispose(source_))result.retained_original.clear();
    return result;
}
}
