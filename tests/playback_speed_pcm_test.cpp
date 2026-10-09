#include "../native/playback_speed_pcm_core.h"
#include <cassert>
#include <limits>

using namespace history;

int main() {
    assert(ClassicPcmSamplesToKeep(0,50.0)==0);
    assert(ClassicPcmSamplesToKeep(2048,1.0)==2048);
    assert(ClassicPcmSamplesToKeep(2048,2.0)==1024);
    assert(ClassicPcmSamplesToKeep(2048,50.0)==40);
    assert(ClassicPcmSamplesToKeep(10,50.0)==1);
    assert(ClassicPcmSamplesToKeep(2048,100.0)==40);
    assert(ClassicPcmSamplesToKeep(2048,std::numeric_limits<double>::quiet_NaN())==2048);
}
