// Include production implementation to control the persisted snapshot without
// timing-dependent sleeps or a test-only public API. No worker is started.
#include "../native/history_settings.cpp"
#include <cassert>
#include <climits>
int main() {
    using namespace history;
    settings.generation=42;persisted_generation=41;persist_failed=false;
    assert(!FlushSettings(0));
    persisted_generation=42;assert(FlushSettings(0));
    persisted_generation=43;assert(FlushSettings(0)); // coalesced newer write
    persist_failed=true;assert(!FlushSettings(0));
    persist_failed=false;
    settings.generation=UINT_MAX;persisted_generation=0;assert(FlushSettings(0));
    settings.generation=0;persisted_generation=UINT_MAX;assert(!FlushSettings(0));
    settings_signal=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(settings_signal);
    for(unsigned level:{0u,1u,2u,4u}) {
        assert(SetLogLevel(level));auto snapshot=GetSettings();
        assert(snapshot.log_level==level && snapshot.log==(level!=0) && snapshot.debug_log==(level==4));
        assert(LogLevel()==level && LoggingEnabled()==(level!=0) && DebugLoggingEnabled()==(level==4));
    }
    const auto generation=GetSettings().generation;
    assert(!SetLogLevel(3) && !SetLogLevel(UINT_MAX));assert(GetSettings().generation==generation);
    assert(SetLogging(false) && LogLevel()==0 && !DebugLoggingEnabled());
    assert(SetLogging(true) && LogLevel()==2);
    assert(SetDebugLogging(true) && LogLevel()==4);
    assert(SetDebugLogging(false) && LogLevel()==2);
    assert(SetLogLevel(1) && SetDebugLogging(false) && LogLevel()==1);
    CloseHandle(settings_signal);settings_signal=nullptr;
    puts("PASS: settings flush accepts coalesced snapshots, rejects failed/pending writes and handles generation wrap");
}
