# Soggfy v3.0.0-rc.49

RC49 adds conservative automatic recovery for the intermittent playback stalls observed in the RC48 diagnostic log.

## Diagnosed regression

At 50x, Spotify occasionally stops producing compressed-audio decoder callbacks while its session still reports the same playing track. The 50x media-position estimate continues to reach the end even though there is no complete captured stream, and no `playback_stuck` error is emitted. This was visible in the RC48 log on Joji's *worldstar money (interlude)* and Harry Styles' *Sign of the Times*. Manual Next moved playback forward.

## Changes

- New Classic UI watchdog checks elapsed wall time against the selected accelerated playback speed and the track's duration, with at least 18 seconds grace.
- For a confirmed overdue **playing** track, it requeues the same track from its beginning, up to two attempts.
- If both attempts remain stuck, it advances once to the next track rather than leaving the playlist stopped indefinitely.
- Disables itself at 1x/unsupported playback speed, when downloads are off, or when playback is intentionally paused. It does not repeatedly skip the same stuck identity.
- Records `classic playback recovery retry_1`, `retry_2` or `skip_stalled` in the log for reliable diagnostics.
- Adds regression tests covering normal transitions, pause safety, one-time skip and retry limits.

The RC46 decoder hook, RC47 position tracker, RC48 playlist indicators, SpotX and MP3 publication pipeline are unchanged.

This is a controlled recovery mechanism, not a demonstrated fix to Spotify's underlying decoder stall. Unsolicited paused states may still need a separate diagnostic because deliberate pauses cannot safely be overridden.
