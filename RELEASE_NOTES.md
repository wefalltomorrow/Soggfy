# Soggfy v3.0.0-rc.20

RC20 replaces the ineffective Spotify 1.3.3.264 ContextPlayer speed path with a version-specific hook on the actual track-player creation routine.

## What RC19 proved

The diagnostic scan covered roughly 250 MiB of writable Spotify process memory on each pass and repeatedly found exactly two objects with the expected AudioSessionImpl vtable. Both remained inactive: their current and prepared TrackPlayer fields stayed null even while a normal music track was actively playing. That means the ContextPlayer objects we were finding are not the live music session used for playback.

## Revalidated 1.3.3.264 path

Re-analysis of the official Spotify 1.3.3.264 x64 DLL identified the track-player creation routine at Spotify.dll+0x0057968c.

The routine itself provides stronger ABI evidence than the old RC13 pattern match:

- its fourth Windows x64 argument is copied directly from XMM3 and later logged by Spotify as `speed: %f`;
- its remaining stack arguments line up with the existing 13-argument Soggfy track-player hook ABI;
- two separate Spotify call sites pass their playback-speed double in XMM3;
- one caller immediately moves the returned TrackPlayer into AudioSessionImpl's prepared-player field;
- RC20 validates the exact function prologue, XMM3 speed-copy sequence, stack-argument layout and speed-log store before installing the hook.

RC20 therefore changes only the speed argument as Spotify creates a TrackPlayer. It does not use RC13's fuzzy target discovery and it does not scan for a live ContextPlayer.

## Behavior

- Spotify 1.3.3.264 uses the exact `0x0057968c` track-player creation backend.
- Spotify 1.3.1.234 keeps its existing validated constructor backend.
- Unknown Spotify builds still fail closed at 1x.
- A speed change applies when Spotify creates the next TrackPlayer. If you change the slider during a song, restart the song or start another track.
- `speed_effective` stays at the rate actually seen by the new hook rather than claiming a new slider value before a TrackPlayer has been recreated.
- The log records `Spotify 1.3.3 track-player create` with native, requested and applied rates whenever the exact hook fires.

Capture, conversion and the all-in-one release package are otherwise unchanged.
