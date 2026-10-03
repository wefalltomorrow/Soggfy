#pragma once
#include <windows.h>

namespace history {

void StartPlaybackSpeed(HMODULE spotify_module);
void MaintainPlaybackSpeed(HMODULE spotify_module);
void ApplyPlaybackSpeedNow();
bool PlaybackSpeedSupported();
bool PlaybackSpeedImmediate();

}
