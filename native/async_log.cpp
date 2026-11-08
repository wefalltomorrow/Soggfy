#define WIN32_LEAN_AND_MEAN
#include "async_log.h"
#include "history_settings.h"
#include "log_policy.h"
#include "log_queue.h"
#include <cstring>
#include <cstdio>
namespace history {namespace {
constexpr size_t cap=5*1024*1024;
static LogQueue<> activity_queue,diagnostic_queue;
static HANDLE signal=nullptr;
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static std::atomic<bool> ready{false};
static void Push(LogQueue<>& queue,const char* event,const char* text) {
 if(!event||!text||!ready.load(std::memory_order_acquire))return;
 // Input views are bounded before copying, including diagnostics from callbacks.
 if(queue.Push({event,strnlen(event,128)},{text,strnlen(text,1536)}))SetEvent(signal);
}
static bool Drain(LogQueue<>& queue,bool diagnostic) {
 LogRecord record{};const bool got=queue.Pop(record);
 const unsigned dropped=queue.TakeDropped();
 if(!got&&!dropped)return false;
 const auto settings=GetSettings();
 if(!settings.log_level||settings.root.empty()||(diagnostic&&settings.log_level<4))return true;
 if(!EnsureDirectory(settings.root))return true;
 const auto path=settings.root+(diagnostic?L"\\Soggfy-diagnostics.log":L"\\Soggfy.log");
 SYSTEMTIME t;GetSystemTime(&t);
 auto write=[&](const char* event,const char* detail) {
  char line[1800];const int n=snprintf(line,sizeof(line),"%04u-%02u-%02uT%02u:%02u:%02uZ %s %s\r\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,event,detail);
  if(n>0&&n<int(sizeof(line)))WriteCappedLog(path,std::string(line,size_t(n)));
 };
 if(dropped){char detail[96];snprintf(detail,sizeof(detail),"dropped=%u lane=%s",dropped,diagnostic?"diagnostic":"activity");write("warning",detail);}
 if(got&&(diagnostic||settings.log_level>=ActivityLogLevel(record.event)))write(record.event,record.detail);
 return true;
}
static DWORD WINAPI Run(LPVOID) {
 for(;;) {
  const bool activity=Drain(activity_queue,false),diagnostic=Drain(diagnostic_queue,true);
  if(!activity&&!diagnostic)WaitForSingleObject(signal,1000);
 }
}
static BOOL CALLBACK InitializeLogger(PINIT_ONCE,PVOID,PVOID*) {
 signal=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!signal)return FALSE;
 HANDLE thread=CreateThread(nullptr,0,Run,nullptr,0,nullptr);
 if(!thread){CloseHandle(signal);signal=nullptr;return FALSE;}
 SetThreadPriority(thread,THREAD_PRIORITY_BELOW_NORMAL);CloseHandle(thread);
 ready.store(true,std::memory_order_release);return TRUE;
}
}
bool WriteCappedLog(const std::wstring& path,const std::string& line){
 if(line.empty()||line.size()>cap)return false;
 HANDLE h=CreateFileW(WindowsPath(path).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{},pos{};bool ok=GetFileSizeEx(h,&size)!=0;
 if(ok){if(size.QuadPart<0||size.QuadPart>static_cast<LONGLONG>(cap-line.size())){pos.QuadPart=0;ok=SetFilePointerEx(h,pos,nullptr,FILE_BEGIN)&&SetEndOfFile(h);}else ok=SetFilePointerEx(h,pos,nullptr,FILE_END)!=0;}
 DWORD written=0;if(ok)ok=WriteFile(h,line.data(),DWORD(line.size()),&written,nullptr)&&written==line.size();CloseHandle(h);return ok;
}
void StartLogger(){InitOnceExecuteOnce(&once,InitializeLogger,nullptr,nullptr);}
void LogActivity(const char* event,const std::string& detail) {
 if(event&&LogLevel()>=ActivityLogLevel({event,strnlen(event,128)}))Push(activity_queue,event,detail.c_str());
}
void QueueDiagnostic(const char* message){if(DebugLoggingEnabled())Push(diagnostic_queue,"debug",message);}
}
