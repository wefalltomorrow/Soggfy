# Soggfy v3.0.0-rc.23

RC23 fixes the Spotify startup crash introduced by RC22 and makes the 1.3.3.264 speed path safer to diagnose.

## RC22 crash diagnosis

Two independent RC22 crash dumps reproduce the same failure:

- exception: `0xc0000005` execute access violation
- attempted instruction address: `0x01f7e69c`
- the attempted address is outside every loaded module
- the immediate return address on the crashing thread is inside Spotify.dll at RVA `0x00052a0c`

That pattern is consistent with an invalid indirect interface call rather than a normal fault inside Soggfy's DLL.

RC22 had started treating a memory-scanned SessionTrackPlayer-shaped object as callable during Spotify startup. It could then invoke Spotify's native getter/setter before Spotify itself had ever exposed that object through the real methods. That was too aggressive.

## RC23 behavior

- Keeps the exact Spotify 1.3.3.264 SessionTrackPlayer setter/getter hooks and byte/vtable validation.
- A SessionTrackPlayer pointer is now considered callable only after Spotify itself passes that exact `this` pointer through the hooked native setter or getter.
- Process-memory scans are diagnostic-only. They can inspect the exact vtable object and log its player pointer, automation state and cached speed, but they never call a Spotify method on a scanned pointer.
- Adds `speed_session_hook setter` and `speed_session_hook getter` records for the first few genuine Spotify calls, including the real `this` pointer and native/requested rate.
- Once a genuine Spotify-owned SessionTrackPlayer has been observed, maintenance can safely apply the configured rate to that same object and verify it with Spotify's native getter.
- If Spotify never calls either speed method, the log now says so explicitly and includes setter/getter call counters.
- `speed_effective` remains at 1x until Spotify's genuine live object reports the requested rate.

The capture and conversion paths are unchanged.
