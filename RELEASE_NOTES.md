# Soggfy v3.0.0-rc.13

RC13 is a diagnostic release for the remaining "starts but does not save" capture problem.

## Much more useful Soggfy.log

Normal logging now records:

- capture configuration, configured/effective playback speed and whether the native speed hook is active;
- Ogg BOS/EOS, stream duration, page/byte counts and capture elapsed time;
- exact Ogg sequence, serial, flags and granule values if a stream is rejected;
- raw Windows media position, Soggfy's extrapolated position, timeline age, duration and playing state;
- the exact reason a complete listen is invalidated;
- completed-listen to compressed-stream association details;
- publication destination, native/output format, FFmpeg selection and post-processing result.

With **Debug log** enabled, RC13 also records each 500 ms media sample and Ogg replay details.

## Why this matters versus Floggfy 1.1.0

Floggfy 1.1.0 downloads successfully on the same Spotify build using its normal 1x capture/listen path. Soggfy keeps that capture base but adds the Classic UI, optional 1-50x playback and rate-aware listen validation. The recent Soggfy log shows captures starting but being rejected as incomplete listens before publication, so RC13 instruments that Soggfy-specific layer rather than changing the working upstream hooks blindly.

The release remains one all-in-one Windows x64 ZIP.
