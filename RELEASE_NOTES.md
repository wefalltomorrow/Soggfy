# Soggfy v3.0.0-rc.18

RC18 fixes another Spotify 1.3.3.264 playback-speed bug exposed by the RC17 log.

## What RC17 showed

RC17 was correctly verifying the native TrackPlayer speed, but it had cached a structurally valid ContextPlayer created during startup. That object had no current or prepared TrackPlayer, so every speed request stayed at 1x even after music started.

The giveaway in the log was a stable ContextPlayer address with both `current=0` and `prepared=0` while tracks were actively playing.

## RC18 behavior

- Never caches an inactive Spotify 1.3.3 ContextPlayer just because its vtable/layout is valid.
- Keeps rescanning until a ContextPlayer with a real current or prepared TrackPlayer exists.
- Prefers a unique current-track ContextPlayer over a prepared-only player.
- Drops and reacquires the cached ContextPlayer whenever it becomes inactive across startup or track transitions.
- Keeps the full memory scan off the settings/UI caller; the normal maintenance worker performs it.
- Logs the active ContextPlayer together with its current/prepared TrackPlayer addresses when one is acquired.
- Retains RC17's native getter verification and low-level TrackPlayer speed fallback.

All existing capture, conversion and single-ZIP release behavior remains included.
