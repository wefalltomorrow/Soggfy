#pragma once
#include <windows.h>
#include <vector>

namespace history::post_process_detail {
// FFmpeg runs at loglevel=error. Any stderr byte invalidates the result.
// Peek instead of accumulating diagnostics: even a blocked writer is detected
// and terminated by the caller, with fixed kernel buffer space and no reader
// thread to outlive the job. Only these two handles are inherited.
struct ConverterErrorPipe {
    HANDLE reader=nullptr, writer=nullptr, null_file=INVALID_HANDLE_VALUE;
    STARTUPINFOEXW startup{};
    HANDLE handles[2]{};
    std::vector<unsigned char> attributes;
    bool initialized=false;
    bool Open() {
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        if(!CreatePipe(&reader,&writer,&security,4096) ||
           !SetHandleInformation(reader,HANDLE_FLAG_INHERIT,0))return false;
        null_file=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            &security,OPEN_EXISTING,0,nullptr);
        if(null_file==INVALID_HANDLE_VALUE)return false;
        SIZE_T size=0;
        InitializeProcThreadAttributeList(nullptr,1,0,&size);
        attributes.resize(size);
        startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&size))return false;
        initialized=true;
        handles[0]=writer;handles[1]=null_file;
        if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            handles,sizeof(handles),nullptr,nullptr))return false;
        startup.StartupInfo.cb=sizeof(startup);
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput=startup.StartupInfo.hStdOutput=null_file;
        startup.StartupInfo.hStdError=writer;
        return true;
    }
    bool HasError(bool& has_error) {
        DWORD available=0;
        if(PeekNamedPipe(reader,nullptr,0,nullptr,&available,nullptr)) {
            has_error=available!=0;return true;
        }
        if(GetLastError()==ERROR_BROKEN_PIPE){has_error=false;return true;}
        return false;
    }
    void CloseWriter() {if(writer){CloseHandle(writer);writer=nullptr;}}
    ~ConverterErrorPipe() {
        if(initialized)DeleteProcThreadAttributeList(startup.lpAttributeList);
        CloseWriter();
        if(reader)CloseHandle(reader);
        if(null_file!=INVALID_HANDLE_VALUE)CloseHandle(null_file);
    }
};
}
