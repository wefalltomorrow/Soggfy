#pragma once
#include <windows.h>

namespace history {
void StartPlaybackSpeed(HMODULE spotify_module);
bool PlaybackSpeedSupported();
bool PlaybackSpeedImmediate();
double PlaybackSpeedEffective();

// Kept for compatibility with RC42-RC45 UI/backend messages. The 1.3.1 PCM
// backend applies speed per decode call and does not need a startup arm gate.
void ArmPlaybackSpeedRuntime();
bool PlaybackSpeedRuntimeArmed();
}
