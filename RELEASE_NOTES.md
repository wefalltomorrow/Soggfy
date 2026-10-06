# Soggfy v3.0.0-rc.38

RC38 abandons the Spotify 1.3.3.264 experiment line and returns Soggfy to the last known-good Spotify 1.3.1.234 implementation.

## Baseline

This release is based directly on the RC14 runtime tree. RC14 explicitly kept the native 1-50x playback-speed hook enabled on Spotify 1.3.1.234 because that player ABI had been runtime-validated.

The later RC15-RC37 experiments are not carried forward into this build.

## What was removed

RC38 removes the Spotify 1.3.3-specific work added after RC14, including:

- ContextPlayer and SessionTrackPlayer speed experiments;
- exact 1.3.3 track-player creation hooks;
- PCM-helper and live DecodeAudioData-equivalent speed hooks;
- accelerated native-EOS publication overrides;
- synthetic scrubber/progress overrides;
- forced accelerated next-track handoff;
- the RC31-RC37 CEF/MinHook startup isolation rewrites;
- delayed Spotify hook staging and hookless CEF browser discovery.

## What is restored

RC38 restores the RC14 behavior:

- Spotify 1.3.1.234 native 1-50x speed hook;
- RC14 Classic Soggfy UI;
- Skip Downloaded / Skip Ignored;
- status states already present in that baseline;
- native Ogg/FLAC capture and complete-listen validation;
- FFmpeg output presets and post-processing;
- metadata, artwork, lyrics and Canvas support;
- telemetry/ad filtering from the RC14 baseline;
- optional native To Disk troubleshooting UI;
- the normal single all-in-one Windows x64 release package.

Spotify 1.3.1.234 is the intended client for this release. Native playback speed fails closed on other Spotify versions rather than attempting an unvalidated player ABI.

This is intentionally a clean rollback. Once the 1.3.1.234 baseline is confirmed working again, any later feature can be reintroduced individually instead of carrying the 1.3.3 debugging stack forward.
