#include "../native/validation_process.h"
#include <cassert>
#include <cstdlib>
#include <cwchar>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    using namespace history;
    if(argc==5) {
        wchar_t mode[64]{};GetEnvironmentVariableW(L"SGF_TEST_ARTWORK_FAULT",mode,64);
        HANDLE pipe=reinterpret_cast<HANDLE>(uintptr_t(_wcstoui64(argv[2],nullptr,10)));
        if(!wcscmp(mode,L"hang")){Sleep(10000);return 0;}
        if(!wcscmp(mode,L"exit"))return 5;
        ArtworkValidationReply r;r.format=3;r.width=1;r.height=1;r.depth=8;r.channels=3;
        if(!wcscmp(mode,L"version"))r.version=1;
        if(!wcscmp(mode,L"format"))r.format=4;
        if(!wcscmp(mode,L"zero"))r.width=0;
        if(!wcscmp(mode,L"dimension"))r.width=8193;
        if(!wcscmp(mode,L"pixels")){r.width=8192;r.height=2049;}
        if(!wcscmp(mode,L"depth"))r.depth=3;
        if(!wcscmp(mode,L"channels"))r.channels=5;
        if(!wcscmp(mode,L"progressive"))r.progressive=1;
        if(!wcscmp(mode,L"memory")) {
            void* p=VirtualAlloc(nullptr,128ull*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            if(p){VirtualFree(p,0,MEM_RELEASE);return 6;}
        }
        if(!wcscmp(mode,L"inherit")) {
            wchar_t handle[32]{};GetEnvironmentVariableW(L"SGF_TEST_ARTWORK_HANDLE",handle,32);
            SetEvent(reinterpret_cast<HANDLE>(uintptr_t(_wcstoui64(handle,nullptr,10))));
        }
        DWORD written=0,size=!wcscmp(mode,L"short")?sizeof(r)-1:sizeof(r);
        if(!WriteFile(pipe,&r,size,&written,nullptr)||written!=size)return 7;
        if(!wcscmp(mode,L"extra"))WriteFile(pipe,&r,1,&written,nullptr);
        return 0;
    }
    assert(argc==3||argc==4);
    const bool faults=argc==4;
    const auto path=argv[faults?2:1];const auto worker_path=argv[faults?3:2];
    ValidationWorkerTrust trusted;std::string error;
    assert(trusted.OpenBuildPinned(worker_path,error));
    HANDLE source=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(source!=INVALID_HANDLE_VALUE);
    unsigned char magic=0;DWORD read=0;assert(ReadFile(source,&magic,1,&read,nullptr));
    auto format=magic==137?ValidationFormat::Png:ValidationFormat::Jpeg;
    ArtworkValidationReply reply;
    if(!faults) {
        const bool valid=ValidateArtworkProcess(source,trusted,format,reply,error);
        LARGE_INTEGER position{};assert(SetFilePointerEx(source,{},&position,FILE_CURRENT)&&position.QuadPart==read);
        CloseHandle(source);
        if(!valid){assert(!reply.magic&&!reply.width&&!reply.height&&!error.empty());return 1;}
        assert(ValidReply(reply,format));
        std::cout<<reply.width<<" "<<reply.height<<" "<<reply.depth<<" "<<reply.channels<<" "<<reply.progressive<<"\n";
        return 0;
    }
    HANDLE mutation=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(mutation==INVALID_HANDLE_VALUE);
    wchar_t self[32768]{};assert(GetModuleFileNameW(nullptr,self,32768));
    for(const auto* fault:{L"exit",L"version",L"format",L"zero",L"dimension",L"pixels",L"depth",L"channels",L"progressive",L"short",L"extra",L"hang"}) {
        assert(SetEnvironmentVariableW(L"SGF_TEST_ARTWORK_FAULT",fault));
        const auto started=GetTickCount64();
        assert(!ValidateArtworkProcess(source,self,ValidationFormat::Png,reply,error,{},!wcscmp(fault,L"hang")?100:60000));
        assert(!reply.magic&&!reply.width&&!error.empty());
        if(!wcscmp(fault,L"hang"))assert(GetTickCount64()-started<5000);
    }
    assert(SetEnvironmentVariableW(L"SGF_TEST_ARTWORK_FAULT",L"memory"));
    assert(ValidateArtworkProcess(source,self,ValidationFormat::Png,reply,error));
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE excluded=CreateEventW(&security,TRUE,FALSE,nullptr);assert(excluded);
    assert(SetEnvironmentVariableW(L"SGF_TEST_ARTWORK_HANDLE",std::to_wstring(uintptr_t(excluded)).c_str()));
    assert(SetEnvironmentVariableW(L"SGF_TEST_ARTWORK_FAULT",L"inherit"));
    assert(ValidateArtworkProcess(source,self,ValidationFormat::Png,reply,error));
    assert(WaitForSingleObject(excluded,0)==WAIT_TIMEOUT);CloseHandle(excluded);
    assert(SetEnvironmentVariableW(L"SGF_TEST_ARTWORK_FAULT",L"hang"));
    unsigned calls=0;
    assert(!ValidateArtworkProcess(source,self,format,reply,error,[&]{return ++calls>=3;}));
    assert(calls>=3&&!reply.width);
    assert(!ValidateArtworkProcess(source,self,format,reply,error,[]()->bool{throw 1;}));
    assert(!ValidateArtworkProcess(source,L"relative.exe",format,reply,error));
    assert(!ValidateArtworkProcess(source,trusted,format,reply,error,{},0));
    assert(!ValidateArtworkProcess(source,trusted,ValidationFormat::Flac,reply,error));
    ValidationReply audio;
    assert(!ValidateRecordingProcess(source,trusted,ValidationFormat::Png,audio,error));
    assert(!audio.magic&&!audio.samples);
    SetEnvironmentVariableW(L"SGF_TEST_ARTWORK_FAULT",nullptr);
    SetEnvironmentVariableW(L"SGF_TEST_ARTWORK_HANDLE",nullptr);
    CloseHandle(source);
    assert(!ValidateArtworkProcess(INVALID_HANDLE_VALUE,trusted,format,reply,error));
    std::cout<<"PASS: artwork supervisor reply bounds, source lock, handle isolation, memory, cancellation and deadline\n";
}
