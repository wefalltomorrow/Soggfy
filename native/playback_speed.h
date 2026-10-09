#pragma once
#include <windows.h>
namespace history {
void StartPlaybackSpeed(HMODULE spotify_module);
bool PlaybackSpeedSupported();
void ArmPlaybackSpeedRuntime();
bool PlaybackSpeedRuntimeArmed();
}
