#include "../native/playback_speed_policy.h"
#include <cassert>
#include <limits>
using history::ClassicEffectiveSpeed;
int main() {
    assert(ClassicEffectiveSpeed(true,50)==50);
    assert(ClassicEffectiveSpeed(false,50)==1);
    assert(ClassicEffectiveSpeed(true,50)==50);
    assert(ClassicEffectiveSpeed(false,1)==1);
    assert(ClassicEffectiveSpeed(true,25)==25);
    assert(ClassicEffectiveSpeed(true,50,false)==1);
    assert(ClassicEffectiveSpeed(true,51)==1);
    assert(ClassicEffectiveSpeed(true,std::numeric_limits<double>::quiet_NaN())==1);
}
