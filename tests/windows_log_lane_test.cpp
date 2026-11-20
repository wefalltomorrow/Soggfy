// Use the actual queues, drain and file writer without starting a background worker.
#include "../native/async_log.cpp"
#include <cassert>
#include <csignal> // Keep the C library name visible on every test toolchain.
#include <fstream>
#include <filesystem>
namespace history {
static Settings test_settings;
Settings GetSettings(){return test_settings;}
unsigned LogLevel(){return test_settings.log_level;}
bool DebugLoggingEnabled(){return LogLevel()==4;}
bool StreamDumpEnabled(){return test_settings.stream_dump;}
bool EnsureDirectory(const std::wstring& path){
    std::error_code error;
    std::filesystem::create_directories(path,error);
    return !error&&std::filesystem::is_directory(path);
}
std::wstring WindowsPath(const std::wstring& path){return path;}
}
static std::string Read(const std::wstring& path) {
    std::ifstream input{std::filesystem::path(path)};return {std::istreambuf_iterator<char>(input),{}};
}
int main() {
    using namespace history;
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));
    test_settings.root=std::wstring(temp)+L"Soggfy-log-lanes-"+std::to_wstring(GetCurrentProcessId());
    assert(CreateDirectoryW(test_settings.root.c_str(),nullptr));
    history::signal=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(history::signal);ready.store(true);
    test_settings.log_level=4;
    for(unsigned i=0;i<129;++i)QueueDiagnostic("diagnostic-only");
    LogActivity("finished","completion-survives");
    assert(Drain(activity_queue,false));
    while(Drain(diagnostic_queue,true)){}
    auto activity=Read(test_settings.root+L"\\Soggfy.log");
    auto diagnostic=Read(test_settings.root+L"\\Soggfy-diagnostics.log");
    assert(activity.find("completion-survives")!=std::string::npos);
    assert(activity.find("diagnostic-only")==std::string::npos);
    assert(diagnostic.find("dropped=1 lane=diagnostic")!=std::string::npos);
    assert(diagnostic.find("completion-survives")==std::string::npos);
    QueueDiagnostic("disabled-before-drain");LogActivity("started","reduced-before-drain");
    test_settings.log_level=1;
    assert(Drain(activity_queue,false));assert(Drain(diagnostic_queue,true));
    LogActivity("conversion_retry_failed","failure-at-level-one");assert(Drain(activity_queue,false));
    LogActivity("started","omitted-at-level-one");assert(!Drain(activity_queue,false));
    test_settings.log_level=0;LogActivity("failed","omitted-at-level-zero");assert(!Drain(activity_queue,false));
    activity=Read(test_settings.root+L"\\Soggfy.log");diagnostic=Read(test_settings.root+L"\\Soggfy-diagnostics.log");
    assert(activity.find("failure-at-level-one")!=std::string::npos);
    assert(activity.find("reduced-before-drain")==std::string::npos);
    assert(activity.find("omitted-at-level") ==std::string::npos);
    assert(diagnostic.find("disabled-before-drain")==std::string::npos);
    test_settings.stream_dump=true;
    const unsigned char first_page[]="page-one";
    const unsigned char last_page[]="page-two";
    QueueStreamDump(9,3,false,"ogg_page","accepted",2,first_page,sizeof(first_page)-1,sizeof(first_page)-1);
    QueueStreamDump(9,3,false,"ogg_page","accepted",4,last_page,sizeof(last_page)-1,sizeof(last_page)-1);
    QueueStreamDump(0,3,false,"capture_queue_loss","incomplete",1,nullptr,0,1);
    while(DrainStreamDump()){}
    for(unsigned i=0;i<18;++i)
        QueueStreamDump(10,3,true,"flac_frame","observed",0,nullptr,0,0);
    while(DrainStreamDump()){}
    StopStreamDump(nullptr);
    const auto dump_root=std::filesystem::path(test_settings.root)/L"Soggfy-stream-dump";
    assert(std::filesystem::is_directory(dump_root));
    std::filesystem::path session;
    for(const auto& entry:std::filesystem::directory_iterator(dump_root))
        if(entry.is_directory()){session=entry.path();break;}
    assert(!session.empty());
    const auto raw=Read((session/L"payload.bin").wstring());
    const auto index=Read((session/L"records.jsonl").wstring());
    assert(raw==std::string(reinterpret_cast<const char*>(first_page),sizeof(first_page)-1)+
               std::string(reinterpret_cast<const char*>(last_page),sizeof(last_page)-1));
    assert(index.find("\"format\":\"soggfy-stream-dump\"")!=std::string::npos);
    assert(index.find("\"event\":\"capture_queue_loss\"")!=std::string::npos);
    assert(index.find("\"event\":\"queue_loss\"")!=std::string::npos);
    assert(DeleteFileW((session/L"payload.bin").c_str()));
    assert(DeleteFileW((session/L"records.jsonl").c_str()));
    assert(RemoveDirectoryW(session.c_str()));
    assert(RemoveDirectoryW(dump_root.c_str()));
    ready.store(false);CloseHandle(history::signal);history::signal=nullptr;
    assert(DeleteFileW((test_settings.root+L"\\Soggfy.log").c_str()));
    assert(DeleteFileW((test_settings.root+L"\\Soggfy-diagnostics.log").c_str()));
    assert(RemoveDirectoryW(test_settings.root.c_str()));
    puts("PASS: independent production log files, overflow accounting and settings recheck at drain");
}
