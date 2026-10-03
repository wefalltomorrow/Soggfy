#include "../native/playback_speed_compat.h"
#include <cassert>
using namespace history;

int main() {
    assert(PlaybackSpeedVersionSupported(1,3,1,234));
    assert(!PlaybackSpeedVersionSupported(1,3,3,264));
    assert(!PlaybackSpeedVersionSupported(1,3,0,277));
    assert(!PlaybackSpeedVersionSupported(1,2,94,583));
    assert(!PlaybackSpeedVersionSupported(2,0,0,0));
}
