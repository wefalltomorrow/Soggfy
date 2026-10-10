#include "../native/playback_speed_probe.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace history;
static bool eq(const char* a,const char* b){return std::strcmp(a,b)==0;}
int main(){
    DecodeProbeSnapshot a{};
    assert(eq(DescribeDecodeProbe(a,a,12000),"no_calls_observed"));
    a.enters=4;a.exits=4;a.last_enter_ms=4000;a.last_exit_ms=4000;
    assert(eq(DescribeDecodeProbe(a,a,10000),"no_new_calls"));
    DecodeProbeSnapshot b=a;b.enters=5;b.last_enter_ms=5000;
    assert(eq(DescribeDecodeProbe(a,b,7000),"entered_no_return"));
    assert(eq(DescribeDecodeProbe(a,b,11000),"call_unreturned_5s"));
    b.exits=5;b.last_exit_ms=7000;
    assert(eq(DescribeDecodeProbe(a,b,7001),"calls_no_pcm"));
    b.decoded_samples=2048;
    assert(eq(DescribeDecodeProbe(a,b,7001),"pcm_no_reported_input_delta"));
    b.compressed_units=900;
    assert(eq(DescribeDecodeProbe(a,b,7001),"decode_progress"));
    DecodeProbeSnapshot c=b;c.last_enter_ms=8000;c.last_exit_ms=8000;
    assert(eq(DescribeDecodeProbe(b,c,9000),"no_new_calls"));
    assert(eq(DescribeDecodeProbe(b,c,8000),"no_new_calls"));
    // Diagnostics must never treat a silent decoder as completed capture.
    c.enters=6;c.exits=6;c.no_pcm_calls=1;
    assert(eq(DescribeDecodeProbe(b,c,9000),"calls_no_pcm"));
    std::puts("PASS: decoder progress vs no request vs blocked callback");
}
