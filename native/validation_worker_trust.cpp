#include "validation_worker_trust.h"
#include <bcrypt.h>
#include <array>
namespace history {
namespace {
struct Hash {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    std::vector<unsigned char> object;
    ~Hash(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
};
bool Digest(HANDLE file,std::array<unsigned char,32>& result) {
    Hash hash;DWORD size=0,written=0;
    if(BCryptOpenAlgorithmProvider(&hash.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||
       BCryptGetProperty(hash.algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&written,0)<0||
       written!=sizeof(size)||!size||size>1024*1024)return false;
    hash.object.resize(size);
    if(BCryptCreateHash(hash.algorithm,&hash.hash,hash.object.data(),size,nullptr,0,0)<0)return false;
    std::array<unsigned char,65536> bytes{};DWORD count=0;
    for(;;) {
        if(!ReadFile(file,bytes.data(),DWORD(bytes.size()),&count,nullptr))return false;
        if(!count)break;
        if(BCryptHashData(hash.hash,bytes.data(),count,0)<0)return false;
    }
    return BCryptFinishHash(hash.hash,result.data(),DWORD(result.size()),0)>=0;
}
}
ValidationWorkerTrust::~ValidationWorkerTrust() {
    if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);
    for(auto directory:directories_)CloseHandle(directory);
}
bool ValidationWorkerTrust::Open(const std::wstring& path,const std::string& expected,std::string& error) {
    error.clear();auto fail=[&](const char* why){error=why;return false;};
    if(attempted_)return fail("worker trust instance already used");
    attempted_=true;
    if(expected.size()!=64||expected.find_first_not_of("0123456789abcdef")!=std::string::npos||
       path.size()<3||path[1]!=L':'||(path[2]!=L'\\'&&path[2]!=L'/')||
       path.find(L'\0')!=std::wstring::npos||path.find(L'\"')!=std::wstring::npos)
        return fail("invalid worker trust request");
    file_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    BY_HANDLE_FILE_INFORMATION original{};LARGE_INTEGER size{};
    if(file_==INVALID_HANDLE_VALUE||GetFileType(file_)!=FILE_TYPE_DISK||
       !GetFileInformationByHandle(file_,&original)||original.nNumberOfLinks!=1||
       (original.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||
       !GetFileSizeEx(file_,&size)||size.QuadPart<=0||size.QuadPart>64ll*1024*1024)
        return fail("worker file rejected");
    const DWORD count=GetFinalPathNameByHandleW(file_,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!count||count>32767)return fail("worker path resolution failed");
    std::vector<wchar_t> buffer(count+1);
    const DWORD written=GetFinalPathNameByHandleW(file_,buffer.data(),DWORD(buffer.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!written||written>=buffer.size())return fail("worker path resolution failed");
    std::wstring resolved(buffer.data(),written);
    if(resolved.size()<8||resolved.compare(0,4,L"\\\\?\\")||resolved[5]!=L':'||resolved[6]!=L'\\')
        return fail("worker requires a local drive");
    for(size_t pos=resolved.find(L'\\',7);pos!=std::wstring::npos;pos=resolved.find(L'\\',pos+1)) {
        HANDLE directory=CreateFileW(resolved.substr(0,pos).c_str(),FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(directory==INVALID_HANDLE_VALUE)return fail("worker ancestor lock failed");
        directories_.push_back(directory);
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(directory,&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||
           (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return fail("worker ancestor rejected");
    }
    HANDLE check=CreateFileW(resolved.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    BY_HANDLE_FILE_INFORMATION current{};
    const bool same=check!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(check,&current)&&
        original.dwVolumeSerialNumber==current.dwVolumeSerialNumber&&
        original.nFileIndexHigh==current.nFileIndexHigh&&original.nFileIndexLow==current.nFileIndexLow;
    if(check!=INVALID_HANDLE_VALUE)CloseHandle(check);
    if(!same)return fail("worker identity changed");
    std::array<unsigned char,32> digest{};
    if(!Digest(file_,digest))return fail("worker digest failed");
    static constexpr char hex[]="0123456789abcdef";
    for(size_t i=0;i<digest.size();++i)
        if(hex[digest[i]>>4]!=expected[2*i]||hex[digest[i]&15]!=expected[2*i+1])
            return fail("worker build digest mismatch");
    path_=std::move(resolved);return true;
}
}
