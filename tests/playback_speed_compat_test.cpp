#include "../native/playback_speed_compat.h"
#include <cassert>
using namespace history;

int main() {
    assert(PlaybackSpeedBackendForVersion(1,3,1,234)==PlaybackSpeedBackend::ConstructorHook);
    assert(PlaybackSpeedBackendForVersion(1,3,3,264)==PlaybackSpeedBackend::TrackCreate133);
    assert(PlaybackSpeedBackendForVersion(1,3,0,277)==PlaybackSpeedBackend::Unsupported);
    assert(PlaybackSpeedBackendForVersion(1,2,94,583)==PlaybackSpeedBackend::Unsupported);
    assert(PlaybackSpeedBackendForVersion(2,0,0,0)==PlaybackSpeedBackend::Unsupported);

    assert(PlaybackSpeedVersionSupported(1,3,1,234));
    assert(PlaybackSpeedVersionSupported(1,3,3,264));
    assert(!PlaybackSpeedVersionSupported(1,3,0,277));
}
