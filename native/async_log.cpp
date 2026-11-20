#define WIN32_LEAN_AND_MEAN
#include "async_log.h"
#include "history_settings.h"
#include "log_policy.h"
#include "log_queue.h"
#include "stream_dump_core.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cwchar>
namespace history {namespace {
constexpr size_t cap=5*1024*1024;
constexpr uint64_t stream_raw_cap=48ull*1024*1024;
constexpr uint64_t stream_index_cap=16ull*1024*1024-1024;
constexpr uint64_t stream_index_hard_cap=16ull*1024*1024;
static LogQueue<> activity_queue,diagnostic_queue;
static StreamDumpQueue<16> stream_queue;
static HANDLE signal=nullptr;
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static std::atomic<bool> ready{false};
static HANDLE stream_payload=nullptr,stream_index=nullptr;
static uint64_t stream_raw_bytes=0,stream_index_bytes=0,stream_sequence=0;
static bool stream_initialized=false,stream_disabled=false,stream_limit_marked=false;
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
static bool WriteAll(HANDLE file,const void* data,size_t size) {
 const auto* bytes=static_cast<const unsigned char*>(data);
 while(size) {
  const DWORD amount=DWORD(std::min(size,size_t(MAXDWORD)));
  DWORD written=0;if(!WriteFile(file,bytes,amount,&written,nullptr)||!written)return false;
  bytes+=written;size-=written;
 }
 return true;
}
static uint32_t Crc32(const unsigned char* data,size_t size) {
 uint32_t crc=0xffffffffu;
 for(size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(0-int(crc&1)));}
 return ~crc;
}
static void StopStreamDump(const char* reason) {
 if(stream_payload){CloseHandle(stream_payload);stream_payload=nullptr;}
 if(stream_index){CloseHandle(stream_index);stream_index=nullptr;}
 if(!stream_disabled&&reason){
  if(DebugLoggingEnabled())QueueDiagnostic(reason);else LogActivity("warning",reason);
 }
 stream_disabled=true;
}
static bool StartStreamDump() {
 if(stream_initialized)return !stream_disabled;
 stream_initialized=true;
 const auto settings=GetSettings();
 if(settings.root.empty()) {StopStreamDump("stream dump disabled: save location unavailable");return false;}
 const auto parent=settings.root+L"\\Soggfy-stream-dump";
 if(!EnsureDirectory(parent)){StopStreamDump("stream dump disabled: cannot create output directory");return false;}
 SYSTEMTIME now{};GetSystemTime(&now);
 wchar_t session_name[112];
 const int session_chars=swprintf(session_name,sizeof(session_name)/sizeof(session_name[0]),
  L"session-%04u%02u%02uT%02u%02u%02uZ-%lu-%llu",
  now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
  GetCurrentProcessId(),static_cast<unsigned long long>(GetTickCount64()));
 if(session_chars<=0||size_t(session_chars)>=sizeof(session_name)/sizeof(session_name[0])) {
  StopStreamDump("stream dump disabled: cannot create session name");return false;
 }
 const auto folder=parent+L"\\"+session_name;
 if(!EnsureDirectory(folder)){StopStreamDump("stream dump disabled: cannot create session directory");return false;}
 const auto payload=folder+L"\\payload.bin",index=folder+L"\\records.jsonl";
 stream_payload=CreateFileW(WindowsPath(payload).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,
                            CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 stream_index=CreateFileW(WindowsPath(index).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,
                           CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(stream_payload==INVALID_HANDLE_VALUE)stream_payload=nullptr;
 if(stream_index==INVALID_HANDLE_VALUE)stream_index=nullptr;
 if(!stream_payload||!stream_index){StopStreamDump("stream dump disabled: cannot open output files");return false;}
 static constexpr char header[]="{\"format\":\"soggfy-stream-dump\",\"version\":1,\"data\":\"payload.bin\"}\n";
 if(!WriteAll(stream_index,header,sizeof(header)-1)){StopStreamDump("stream dump disabled: cannot write index header");return false;}
 stream_index_bytes=sizeof(header)-1;
 return true;
}
static int FormatStreamLine(const StreamDumpRecord& record,char* line,size_t capacity) {
 const uint32_t crc=record.length?Crc32(record.bytes,record.length):0;
 return snprintf(line,capacity,
  "{\"seq\":%llu,\"time_ms\":%llu,\"stream_id\":%llu,\"epoch\":%u,\"codec\":\"%s\",\"event\":\"%s\",\"state\":\"%s\",\"status\":%u,\"offset\":%llu,\"length\":%u,\"input_length\":%llu,\"dropped_before\":%llu,\"crc32\":\"%08x\"}\n",
  static_cast<unsigned long long>(stream_sequence),static_cast<unsigned long long>(record.time_ms),
  static_cast<unsigned long long>(record.stream_id),record.epoch,record.codec,record.event,record.state,
  record.status,static_cast<unsigned long long>(stream_raw_bytes),record.length,
  static_cast<unsigned long long>(record.input_length),static_cast<unsigned long long>(record.dropped_before),crc);
}
static bool WriteStreamLine(const StreamDumpRecord& record) {
 if(!StartStreamDump())return false;
 char line[640];
 ++stream_sequence;
 int n=FormatStreamLine(record,line,sizeof(line));
 const bool over_limit=n<=0||size_t(n)>=sizeof(line)||record.length>stream_raw_cap-stream_raw_bytes||
    uint64_t(n)>stream_index_cap-stream_index_bytes;
 if(over_limit) {
  --stream_sequence;
  if(!stream_limit_marked) {
   stream_limit_marked=true;
   StreamDumpRecord marker;marker.time_ms=GetTickCount64();marker.epoch=record.epoch;
   marker.status=1;std::strcpy(marker.codec,"none");std::strcpy(marker.event,"session_limit");
   std::strcpy(marker.state,"incomplete");++stream_sequence;
   n=FormatStreamLine(marker,line,sizeof(line));
   if(n>0&&size_t(n)<sizeof(line)&&uint64_t(n)<=stream_index_hard_cap-stream_index_bytes&&
      WriteAll(stream_index,line,size_t(n)))stream_index_bytes+=uint64_t(n);
  }
  StopStreamDump("stream dump stopped at its 64 MiB session limit");return false;
 }
 if((record.length&&!WriteAll(stream_payload,record.bytes,record.length))||!WriteAll(stream_index,line,size_t(n))) {
  StopStreamDump("stream dump stopped after a file write error");return false;
 }
 stream_raw_bytes+=record.length;stream_index_bytes+=uint64_t(n);
 return true;
}
static bool DrainStreamDump() {
 StreamDumpRecord record;
 const uint64_t dropped=stream_queue.TakeDropped();
 bool did_work=dropped!=0;
 if(dropped){
  record.time_ms=GetTickCount64();record.input_length=dropped;record.dropped_before=0;
  std::strcpy(record.codec,"none");std::strcpy(record.event,"queue_loss");std::strcpy(record.state,"incomplete");
  record.status=uint32_t(std::min<uint64_t>(dropped,UINT32_MAX));
  WriteStreamLine(record);
 }
 if(stream_queue.Pop(record)){WriteStreamLine(record);did_work=true;}
 if(stream_disabled)while(stream_queue.Pop(record))did_work=true;
 return did_work;
}
static DWORD WINAPI Run(LPVOID) {
 for(;;) {
  const bool activity=Drain(activity_queue,false),diagnostic=Drain(diagnostic_queue,true),stream=DrainStreamDump();
  if(!activity&&!diagnostic&&!stream)WaitForSingleObject(signal,1000);
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
static void CopyToken(char* output,size_t capacity,const char* value) {
 if(!capacity)return;
 size_t n=0;
 if(value)for(;n+1<capacity&&value[n];++n){const char c=value[n];output[n]=((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')?c:'_';}
 output[n]=0;if(!n){constexpr char fallback[]="unknown";size_t i=0;for(;i+1<capacity&&fallback[i];++i)output[i]=fallback[i];output[i]=0;}
}
void QueueStreamDump(uint64_t stream_id,uint32_t epoch,bool flac,const char* event,const char* state,
                     unsigned status,const void* bytes,size_t length,uint64_t input_length) {
 if(!StreamDumpEnabled())return;
 if(length>kStreamDumpMaxPayload||(!bytes&&length)) {
  stream_queue.RecordDrop();
  if(ready.load(std::memory_order_acquire)&&signal)SetEvent(signal);
  return;
 }
 StreamDumpRecord record;record.stream_id=stream_id;record.epoch=epoch;record.time_ms=GetTickCount64();
 record.status=status;record.length=uint32_t(length);record.input_length=input_length;
 CopyToken(record.codec,sizeof(record.codec),stream_id?(flac?"flac":"ogg"):"none");
 CopyToken(record.event,sizeof(record.event),event);CopyToken(record.state,sizeof(record.state),state);
 if(length)std::memcpy(record.bytes,bytes,length);
 stream_queue.Push(record);
 if(ready.load(std::memory_order_acquire)&&signal)SetEvent(signal);
}
}
