#include "../native/log_queue.h"
#include "../native/log_policy.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>
#include <string>
#include <cstdio>
using namespace history;
int main() {
    assert(ReadLogLevel(L"",false,true)==0);
    assert(ReadLogLevel(L"",true,false)==2);
    assert(ReadLogLevel(L"",true,true)==4);
    for(unsigned level:{0u,1u,2u,4u})assert(ReadLogLevel(std::to_wstring(level),false,false)==level);
    for(auto text:{L"3",L"-1",L"04",L"4junk",L"99999"})assert(ReadLogLevel(text,true,true)==1);
    assert(ActivityLogLevel("finished")==1 && ActivityLogLevel("warning")==1);
    assert(ActivityLogLevel("conversion_retry_done")==1);
    assert(ActivityLogLevel("conversion_retry_failed")==1 && ActivityLogLevel("started")==2);
    LogQueue<4> activity,diagnostics;LogRecord record;
    for(unsigned i=0;i<4;++i)assert(diagnostics.Push("debug",std::to_string(i)));
    assert(!diagnostics.Push("debug","overflow"));
    assert(activity.Push("finished","A\r\nB"));
    assert(activity.Pop(record)&&!std::strcmp(record.detail,"A  B"));
    assert(diagnostics.TakeDropped()==1 && diagnostics.TakeDropped()==0);
    for(unsigned i=0;i<4;++i){assert(diagnostics.Pop(record));assert(record.detail==std::to_string(i));}
    assert(!diagnostics.Pop(record));
    std::string unicode(1534,'x');unicode+="\xe6\x9d\xb1";
    assert(activity.Push("finished",unicode));assert(activity.Pop(record));
    assert(std::strlen(record.detail)==1534); // No partial UTF-8 character.
    LogQueue<> concurrent;std::atomic<unsigned> accepted{0};
    std::vector<std::thread> producers;
    for(unsigned t=0;t<8;++t)producers.emplace_back([&]{for(unsigned i=0;i<1000;++i)if(concurrent.Push("debug","concurrent"))++accepted;});
    for(auto& thread:producers)thread.join();
    unsigned drained=0;while(concurrent.Pop(record)){++drained;assert(!std::strcmp(record.detail,"concurrent"));}
    assert(drained==accepted && drained<=128 && concurrent.TakeDropped()+drained==8000);
    assert(concurrent.Push("finished","after overflow") && concurrent.Pop(record));
    puts("PASS: log migration, severity, independent lanes, UTF-8 truncation and concurrent bounded producers");
}
