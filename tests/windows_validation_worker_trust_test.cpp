#include "../native/validation_worker_trust.h"
#include <cassert>
#include <cwchar>
#include <filesystem>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc==2&&!wcscmp(argv[1],L"--child"))return 77;
    assert(argc==2);
    std::string expected;for(const auto* p=argv[1];*p;++p)expected+=char(*p);
    wchar_t self[32768]{},temporary[32768]{};
    assert(GetModuleFileNameW(nullptr,self,32768)&&GetTempPathW(32768,temporary));
    const auto root=std::wstring(temporary)+L"soggfy-trusted-worker-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    assert(CreateDirectoryW(root.c_str(),nullptr));
    const auto path=root+L"\\worker \u97f3\u697d.exe",alias=root+L"\\alias.exe";
    assert(CopyFileW(self,path.c_str(),TRUE));std::string error;
    {
        history::ValidationWorkerTrust worker;
        assert(worker.Path().empty());
        assert(worker.Open(path,expected,error)&&error.empty()&&!worker.Path().empty());
        assert(!DeleteFileW(path.c_str()));
        assert(!MoveFileW(path.c_str(),alias.c_str()));
        assert(!MoveFileW(root.c_str(),(root+L"-moved").c_str()));
        HANDLE writer=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        assert(writer==INVALID_HANDLE_VALUE);
        STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION process{};
        auto command=L"\""+worker.Path()+L"\" --child";
        assert(CreateProcessW(worker.Path().c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
                              nullptr,nullptr,&start,&process));
        assert(WaitForSingleObject(process.hProcess,5000)==WAIT_OBJECT_0);
        DWORD code=0;assert(GetExitCodeProcess(process.hProcess,&code)&&code==77);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        assert(!worker.Open(path,expected,error));
    }
    auto rejected=[&](const std::wstring& name,const std::string& hash) {
        history::ValidationWorkerTrust worker;
        assert(!worker.Open(name,hash,error)&&!error.empty()&&worker.Path().empty());
    };
    rejected(path,std::string(64,'0'));rejected(path,expected.substr(1));
    auto malformed=expected;malformed[0]='g';rejected(path,malformed);
    rejected(L"worker.exe",expected);rejected(root,expected);
    rejected(path+L"-missing",expected);
    {
        HANDLE writer=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        assert(writer!=INVALID_HANDLE_VALUE);
        HANDLE mapping=CreateFileMappingW(writer,nullptr,PAGE_READWRITE,0,0,nullptr);assert(mapping);
        auto view=MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,0);assert(view);
        CloseHandle(writer);rejected(path,expected);
        assert(UnmapViewOfFile(view));CloseHandle(mapping);
    }
    assert(CreateHardLinkW(alias.c_str(),path.c_str(),nullptr));
    rejected(path,expected);assert(DeleteFileW(alias.c_str()));
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
    assert(file!=INVALID_HANDLE_VALUE);DWORD written=0;
    const unsigned char mutation=0;assert(WriteFile(file,&mutation,1,&written,nullptr)&&written==1);CloseHandle(file);
    rejected(path,expected);assert(DeleteFileW(path.c_str()));
    assert(std::filesystem::is_empty(root));assert(RemoveDirectoryW(root.c_str()));
    std::cout<<"PASS: build-pinned helper digest, immutable executable/ancestor names, locked launch, Unicode, mismatch and hard-link rejection\n";
    return 0;
}
