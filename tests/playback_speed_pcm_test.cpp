#include "../native/playback_speed_pcm_core.h"
#include <cassert>
#include <limits>
using namespace history;
int main() {
    assert(kMaxPlaybackSpeed==30.0);
    assert(ClassicPcmSamplesToKeep(0,30.0)==0);
    assert(ClassicPcmSamplesToKeep(2048,1.0)==2048);
    assert(ClassicPcmSamplesToKeep(2048,2.0)==1024);
    assert(ClassicPcmSamplesToKeep(2048,30.0)==68);
    assert(ClassicPcmSamplesToKeep(10,30.0)==1);
    // Legacy out-of-range settings must never execute above 30x.
    assert(ClassicPcmSamplesToKeep(2048,31.0)==68);
    assert(ClassicPcmSamplesToKeep(2048,50.0)==68);
    assert(ClassicPcmSamplesToKeep(2048,100.0)==68);
    assert(ClassicPcmSamplesToKeep(2048,std::numeric_limits<double>::quiet_NaN())==2048);
}
