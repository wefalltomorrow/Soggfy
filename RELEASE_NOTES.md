# Soggfy v3.0.0-rc.26

RC26 fixes the two problems exposed by the first stable RC25 run: current Spotify ad endpoints were escaping the old URL filter, and the experimental native playback-speed setters were not part of the active music path.

## What the RC25 log proved

RC25 starts safely and releases the deferred Spotify hooks after the loader gate.

The CEF request filter is also alive: the log records blocked classic telemetry requests before and after Spotify startup. That means the ad-block regression is not a dead hook; the URL denylist is simply too narrow for current regional Spotify endpoints.

For playback speed, Spotify 1.3.3.264 repeatedly reports:

- configured rates changing from 11x to 26x;
- effective rate remaining 1x;
- zero calls to both hooked SessionTrackPlayer methods;
- one memory-scan vtable hit whose surrounding fields are clearly not the assumed live player layout.

The SessionTrackPlayer/ContextPlayer experiments from RC16-RC25 are therefore retired for 1.3.3.264 rather than loosened again.

## Restored classic Soggfy speed method

The original Soggfy did not depend on Spotify's internal playback-speed APIs. It accelerated playback after decode by consuming the complete decoded stream while exposing only a fraction of each PCM block to the audio sink.

RC26 ports that behavior to the current x64 client.

For the exact supported Spotify 1.3.3.264 DLL, RC26 validates:

- the PCM filter-chain process function at `Spotify.dll+0x00463954`;
- the exact function prologue;
- the internal process call;
- the instructions that return the processed PCM span;
- the matching vtable slot at `Spotify.dll+0x019c7c68`.

Only after all checks pass is the hook installed.

When speed is above 1x, the complete PCM block is processed first and Soggfy then reduces the returned sample count by the configured factor, matching the old Soggfy playback-speed strategy. The compressed Ogg/FLAC capture remains untouched.

The log now records `speed_pcm_hook` with input, produced and kept sample counts. `speed_effective` changes only after the PCM hook has actually thinned a live block.

## Ad blocking

The Classic CEF filter now blocks both:

- the historical `spclient.wg.spotify.com/ads/` and `/ad-logic/` endpoints;
- current regional `*-spclient.spotify.com/ads/` and `/ad-logic/` endpoints.

The existing wg `gabo-receiver-service` and `dodo-receiver-service` tracking blocks remain. Metadata, audio-CDN, login and update traffic are still left alone.

RC25's loader-readiness gate and read-only IAT fix remain unchanged.
