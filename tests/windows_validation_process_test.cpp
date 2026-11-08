#include "../native/validation_process.h"
#include <cassert>
#include <cstdlib>
#include <cwchar>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc==5) {
        // A deliberately faulty trusted test helper exercises the supervisor.
        wchar_t mode[64]{};GetEnvironmentVariableW(L"SGF_TEST_VALIDATOR_FAULT",mode,64);
        HANDLE pipe=reinterpret_cast<HANDLE>(uintptr_t(_wcstoui64(argv[2],nullptr,10)));
        if(!wcscmp(mode,L"hang")){Sleep(10000);return 0;}
        if(!wcscmp(mode,L"exit"))return 5;
        history::ValidationReply reply;
        reply.format=2;reply.samples=1;reply.units=1;reply.rate=44100;reply.channels=2;reply.bits=24;
        if(!wcscmp(mode,L"memory")) {
            void* block=VirtualAlloc(nullptr,768ull*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            if(block){VirtualFree(block,0,MEM_RELEASE);return 6;}
        }
        if(!wcscmp(mode,L"inherit")) {
            wchar_t handle[32]{};GetEnvironmentVariableW(L"SGF_TEST_EXCLUDED_HANDLE",handle,32);
            SetEvent(reinterpret_cast<HANDLE>(uintptr_t(_wcstoui64(handle,nullptr,10))));
        }
        if(!wcscmp(mode,L"wrong"))reply.version=99;
        if(!wcscmp(mode,L"zero"))reply.samples=0;
        DWORD written=0;
        DWORD size=!wcscmp(mode,L"short")?sizeof(reply)-1:sizeof(reply);
        if(!WriteFile(pipe,&reply,size,&written,nullptr)||written!=size)return 7;
        if(!wcscmp(mode,L"extra"))WriteFile(pipe,&reply,1,&written,nullptr);
        return 0;
    }
    assert(argc==4);
    std::string expected;for(const auto* p=argv[3];*p;++p)expected+=char(*p);
    history::ValidationWorkerTrust trusted;std::string trust_error;
    assert(trusted.Open(argv[2],expected,trust_error));
    HANDLE source=CreateFileW(argv[1],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(source!=INVALID_HANDLE_VALUE);
    HANDLE mutation=CreateFileW(argv[1],GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(mutation==INVALID_HANDLE_VALUE);
    unsigned char magic[4]{};DWORD read=0;
    assert(ReadFile(source,magic,4,&read,nullptr)&&read==4);
    auto format=magic[0]=='f'?history::ValidationFormat::Flac:history::ValidationFormat::Vorbis;
    LARGE_INTEGER before{};assert(SetFilePointerEx(source,{},&before,FILE_CURRENT));
    history::ValidationReply reply;std::string error;
    if(!history::ValidateRecordingProcess(source,trusted,format,reply,error)) {
        std::cerr<<error<<"\n";return 1;
    }
    assert(history::ValidReply(reply,format));
    LARGE_INTEGER after{};assert(SetFilePointerEx(source,{},&after,FILE_CURRENT)&&after.QuadPart==before.QuadPart);
    wchar_t self[32768]{};assert(GetModuleFileNameW(nullptr,self,32768));
    for(const auto* fault:{L"exit",L"wrong",L"zero",L"short",L"extra",L"hang"}) {
        assert(SetEnvironmentVariableW(L"SGF_TEST_VALIDATOR_FAULT",fault));
        const auto started=GetTickCount64();
        assert(!history::ValidateRecordingProcess(source,self,history::ValidationFormat::Flac,reply,error,{},
            !wcscmp(fault,L"hang")?100:60000));
        assert(!reply.magic&&!reply.samples&&!error.empty());
        if(!wcscmp(fault,L"hang"))assert(GetTickCount64()-started<5000);
    }
    assert(SetEnvironmentVariableW(L"SGF_TEST_VALIDATOR_FAULT",L"memory"));
    assert(history::ValidateRecordingProcess(source,self,history::ValidationFormat::Flac,reply,error));
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE excluded=CreateEventW(&security,TRUE,FALSE,nullptr);assert(excluded);
    const auto handle=std::to_wstring(uintptr_t(excluded));
    assert(SetEnvironmentVariableW(L"SGF_TEST_EXCLUDED_HANDLE",handle.c_str()));
    assert(SetEnvironmentVariableW(L"SGF_TEST_VALIDATOR_FAULT",L"inherit"));
    assert(history::ValidateRecordingProcess(source,self,history::ValidationFormat::Flac,reply,error));
    assert(WaitForSingleObject(excluded,0)==WAIT_TIMEOUT);CloseHandle(excluded);
    SetEnvironmentVariableW(L"SGF_TEST_EXCLUDED_HANDLE",nullptr);
    assert(SetEnvironmentVariableW(L"SGF_TEST_VALIDATOR_FAULT",L"hang"));
    unsigned calls=0;
    assert(!history::ValidateRecordingProcess(source,self,format,reply,error,[&]{return ++calls>=3;}));
    assert(calls>=3&&!reply.samples);
    assert(!history::ValidateRecordingProcess(source,self,format,reply,error,[]()->bool{throw 1;}));
    assert(!history::ValidateRecordingProcess(source,L"relative.exe",format,reply,error));
    assert(!history::ValidateRecordingProcess(source,argv[2],format,reply,error,{},0));
    SetEnvironmentVariableW(L"SGF_TEST_VALIDATOR_FAULT",nullptr);
    CloseHandle(source);
    assert(!history::ValidateRecordingProcess(INVALID_HANDLE_VALUE,argv[2],format,reply,error));
    std::cout<<"PASS: isolated decoding, source lock/position, malformed replies, memory limit, timeout and cancellation\n";
    return 0;
}
