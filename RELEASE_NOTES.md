# Soggfy v3.0.0-rc.52

RC52 addresses the intermittent 50× playback stall still occurring on RC51, while preserving RC51's now-working playlist status indicators and MP3 publication.

## Verified RC51 diagnostic

The supplied `Soggfy(10).log` shows Alex K — *Pretty Green Eyes - Album Edit* selected at 02:12:43 UTC. The synthetic media position reached its full 197.903 s by approximately 02:12:48 while the Spotify source position stayed at 0.000. The captured decoder-call and page counts remained frozen at 28,474 / 22,661 for over 100 seconds. No `classic playback recovery` attempts occurred. Prior tracks successfully exported MP3s.

## Recovery issue

RC49's watchdog required `state.item.uri` and camelCase flags `state.isPaused` / `state.isPlaying`, but Spotify's internal player state can expose `track.uri`, `is_paused`, `is_playing`. With a missing identity or missing play flag, the watchdog silently reset its timer instead of recovering.

## Changes

- Normalize native snake_case and legacy camelCase player state for URI, play/pause flags and duration.
- Accept the Spotify transport Pause/Play button only as a fallback if the player state has no explicit play flag. Explicit manual pauses remain respected.
- Remove an unnecessary `speedImmediate` gating condition; native speed support and accelerated downloads are still required.
- Make the retry helper recognize a native `track.uri`, parse snake_case position/speed fields, and await Spotify's queue snapshot.
- Fall back to Spicetify's Next control if the native player's skip method isn't exposed.
- Log watchdog initialization and infrequent missing/unknown-state diagnostic markers.
- Test a synthetic replica of the RC51 197.903 s/50× stall against snake_case Spotify state, including two retries, final bounded skip, pause/resume safety, camelCase compatibility and UI play-button fallback.

This is a recovery fix rather than a verified cure for the underlying decoder stall. The native PCM decoder, Ogg/FLAC capture, MP3 output, SpotX integration and RC51 status icons are unchanged. In-app testing is still required.
