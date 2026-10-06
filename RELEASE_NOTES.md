# Soggfy v3.0.0-rc.31

RC31 fixes the two problems exposed by extended RC30 testing: intermittent blank-primary-monitor startup behavior and Spotify's transport UI/queue remaining at the raw 1x timeline during accelerated playback.

## Startup stability

The blank-screen problem was intermittent across RC27-RC30 and did not correlate reliably with either the Block Telemetry setting or the configured playback rate. Both settings could work or fail between launches, which points at startup timing rather than the feature state itself.

RC31 reduces invasive hook activity while Spotify and Chromium are still starting:

- Queues the three initial Classic CEF browser hooks and enables them in one MinHook apply instead of three separate thread-suspension cycles.
- Leaves the early Classic UI bridge available so the main Spotify browser can still be observed.
- Defers the optional CEF telemetry/ad request hook for five seconds after libcef appears.
- Increases the Spotify.dll loader grace from 1500 ms to 2500 ms.
- Staggers connectivity, playback-speed and audio-capture hook families rather than releasing the native hooks back-to-back.
- Adds concise startup-phase diagnostics so any remaining failure can be tied to the exact hook phase.

## 50x transport/progress fix

RC29 correctly proves accelerated completion from the validated native Ogg/FLAC EOS, but Spotify's own public transport clock still advances at roughly 1x. That could leave a completed 50x download sitting near the start of the visible scrubber and Spotify waiting on its raw timeline instead of moving on.

RC31 now:

- Maintains a display-only accelerated transport clock in the Classic UI.
- Updates Spotify's visible elapsed-time text and progress-bar transform at the configured effective rate.
- Never repeatedly seeks Spotify just to move the scrubber, so compressed capture integrity is not disturbed.
- Queues a one-shot native-to-CEF completion event when accelerated capture EOS proves the listen complete.
- Calls Spotify's own `skipToNext()` only if the same playback is still active when that completion event reaches the UI.
- Ignores duplicate or delayed completion events so the following track cannot be skipped accidentally.

RC29's accelerated capture/publication logic and RC30's original-style per-track status indicators are otherwise unchanged.
