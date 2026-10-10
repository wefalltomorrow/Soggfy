#include "../native/playback_speed_policy.h"
#include <cassert>
#include <limits>
using history::ClassicEffectiveSpeed;
int main() {
    assert(ClassicEffectiveSpeed(true,30)==30);
    assert(ClassicEffectiveSpeed(false,30)==1);
    assert(ClassicEffectiveSpeed(true,25)==25);
    assert(ClassicEffectiveSpeed(false,1)==1);
    assert(ClassicEffectiveSpeed(true,30,false)==1);
    // Preserve old saved preferences at a capped 30x, never restore 31-50x.
    assert(ClassicEffectiveSpeed(true,31)==30);
    assert(ClassicEffectiveSpeed(true,50)==30);
    assert(ClassicEffectiveSpeed(true,51)==30);
    assert(ClassicEffectiveSpeed(false,50)==1);
    assert(ClassicEffectiveSpeed(true,0)==1);
    assert(ClassicEffectiveSpeed(true,std::numeric_limits<double>::quiet_NaN())==1);
}
