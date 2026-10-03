# Soggfy v3.0.0-rc.17

RC17 fixes the RC16 case where the speed slider changed but Spotify 1.3.3.264 still audibly played at 1x.

## What the RC16 log proved

RC16 reported `speed_hook=1` and copied the configured 4x/14x value into `speed_effective`, but the raw Spotify media timeline was still advancing at roughly normal speed. That also caused Soggfy's rate-aware listen validator to extrapolate too far and reject the next song as a rewind.

The problem was that RC16 trusted Spotify's ContextPlayer speed wrapper return value instead of verifying the nested TrackPlayer's actual speed.

## RC17 behavior

- Reads the real TrackPlayer speed through Spotify's native vtable getter.
- Calls the normal Spotify 1.3.3 ContextPlayer wrappers first.
- If the wrapper did not actually change the TrackPlayer speed, falls back to the low-level TrackPlayer setter that those wrappers call internally.
- Uses mode 0 for the current player and mode 2 for the prepared player, matching the 1.3.3.264 disassembly.
- Re-reads the native speed after the call and only treats the requested rate as effective when it is actually reported by the player.
- Capture timing now uses that verified effective rate, so a failed speed change cannot make Soggfy falsely reject a normal 1x listen.
- `Soggfy.log` records `speed_verified` or `speed_failed` with before/after native speed values for troubleshooting.

All RC16/RC15 capture, MP3 conversion and single-ZIP release behavior remains included.
