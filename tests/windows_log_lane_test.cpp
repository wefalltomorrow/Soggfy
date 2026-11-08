// Use the actual queues, drain and file writer without starting a background worker.
#include "../native/async_log.cpp"
#include <cassert>
#include <fstream>
#include <filesystem>
namespace history {
static Settings test_settings;
Settings GetSettings(){return test_settings;}
unsigned LogLevel(){return test_settings.log_level;}
bool DebugLoggingEnabled(){return LogLevel()==4;}
bool EnsureDirectory(const std::wstring& path){return std::filesystem::is_directory(path);}
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
    signal=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(signal);ready.store(true);
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
    ready.store(false);CloseHandle(signal);signal=nullptr;
    assert(DeleteFileW((test_settings.root+L"\\Soggfy.log").c_str()));
    assert(DeleteFileW((test_settings.root+L"\\Soggfy-diagnostics.log").c_str()));
    assert(RemoveDirectoryW(test_settings.root.c_str()));
    puts("PASS: independent production log files, overflow accounting and settings recheck at drain");
}
