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
    puts("PASS: settings flush accepts coalesced snapshots, rejects failed/pending writes and handles generation wrap");
}
