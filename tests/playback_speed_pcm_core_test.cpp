#include "../native/playback_speed_pcm_core.h"
#include <cassert>
#include <limits>
using namespace history;

int main() {
    assert(ClassicPcmSamplesToKeep(0,5.0)==0);
    assert(ClassicPcmSamplesToKeep(4410,1.0)==4410);
    assert(ClassicPcmSamplesToKeep(4410,5.0)==882);
    assert(ClassicPcmSamplesToKeep(4410,10.0)==441);
    assert(ClassicPcmSamplesToKeep(100,3.0)==33);
    assert(ClassicPcmSamplesToKeep(3,26.0)==1);
    assert(ClassicPcmSamplesToKeep(100,std::numeric_limits<double>::quiet_NaN())==100);
}
