#include "validation_process.h"
#include <vector>
namespace history {
namespace {
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle()=default;
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
};
struct Attributes {
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST list=nullptr;
    ~Attributes(){if(list)DeleteProcThreadAttributeList(list);}
};
}
bool ValidateRecordingProcess(HANDLE source,const std::wstring& worker,
    ValidationFormat format,ValidationReply& output,std::string& error,
    const std::function<bool()>& canceled,DWORD timeout_ms) {
    output={};output.magic=0;output.version=0;error.clear();
    auto fail=[&](const char* reason){error=reason;return false;};
    auto stop=[&](){try{return canceled&&canceled();}catch(...){return true;}};
    if(stop())return fail("validation canceled");
    if((format!=ValidationFormat::Vorbis&&format!=ValidationFormat::Flac)||
       !timeout_ms||timeout_ms>60000||worker.empty()||worker.find(L'\"')!=std::wstring::npos||
       worker.find(L'\0')!=std::wstring::npos)return fail("invalid validation request");
    // Require an absolute path: never search PATH or the current directory.
    if(!((worker.size()>2&&worker[1]==L':'&&(worker[2]==L'\\'||worker[2]==L'/'))||
         (worker.size()>2&&worker[0]==L'\\'&&worker[1]==L'\\')))
        return fail("validation worker path must be absolute");
    LARGE_INTEGER size{};
    if(GetFileType(source)!=FILE_TYPE_DISK||!GetFileSizeEx(source,&size)||size.QuadPart<=0||
       uint64_t(size.QuadPart)>ValidationInputLimit)return fail("invalid validation input size");
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    Handle mapping,reader,writer,job,process,thread;
    mapping.value=CreateFileMappingW(source,&security,PAGE_READONLY,0,0,nullptr);
    if(!mapping.value||!CreatePipe(&reader.value,&writer.value,&security,4096)||
       !SetHandleInformation(reader.value,HANDLE_FLAG_INHERIT,0))return fail("validation IPC creation failed");
    job.value=CreateJobObjectW(nullptr,nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|
        JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.BasicLimitInformation.ActiveProcessLimit=1;
    limits.ProcessMemoryLimit=768ull*1024*1024;
    if(!job.value||!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
        return fail("validation resource limits failed");
    Attributes attributes;SIZE_T length=0;
    InitializeProcThreadAttributeList(nullptr,1,0,&length);
    if(!length)return fail("validation handle list sizing failed");
    attributes.storage.resize(length);
    auto list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.storage.data());
    if(!InitializeProcThreadAttributeList(list,1,0,&length))return fail("validation handle list failed");
    attributes.list=list;
    HANDLE handles[]={mapping.value,writer.value};
    if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr))
        return fail("validation handle restriction failed");
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=list;
    PROCESS_INFORMATION info{};
    std::wstring command=L"\""+worker+L"\" "+std::to_wstring(uintptr_t(mapping.value))+L" "+
        std::to_wstring(uintptr_t(writer.value))+L" "+std::to_wstring(size.QuadPart)+L" "+
        std::to_wstring(uint32_t(format));
    if(!CreateProcessW(worker.c_str(),command.data(),nullptr,nullptr,TRUE,
       CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&info))
        return fail("validation worker launch failed");
    process.value=info.hProcess;thread.value=info.hThread;
    // No decoder instruction runs before the process belongs to the limited job.
    if(!AssignProcessToJobObject(job.value,process.value)||ResumeThread(thread.value)==DWORD(-1)) {
        TerminateProcess(process.value,1);WaitForSingleObject(process.value,2000);
        return fail("validation worker confinement failed");
    }
    CloseHandle(writer.value);writer.value=nullptr;
    const ULONGLONG start=GetTickCount64();
    for(;;) {
        if(stop()||GetTickCount64()-start>=timeout_ms) {
            TerminateJobObject(job.value,1);WaitForSingleObject(process.value,2000);
            return fail("validation canceled or timed out");
        }
        DWORD wait=WaitForSingleObject(process.value,10);
        if(wait==WAIT_OBJECT_0)break;
        if(wait!=WAIT_TIMEOUT)return fail("validation wait failed");
    }
    DWORD code=1,available=0,received=0;ValidationReply reply;
    if(stop()||!GetExitCodeProcess(process.value,&code)||code||
       !PeekNamedPipe(reader.value,nullptr,0,nullptr,&available,nullptr)||available!=sizeof(reply)||
       !ReadFile(reader.value,&reply,sizeof(reply),&received,nullptr)||received!=sizeof(reply)||
       !ValidReply(reply,format))return fail("validation worker rejected input or reply");
    output=reply;return true;
}
}
