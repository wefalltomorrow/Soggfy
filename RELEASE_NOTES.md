# Soggfy v3.0.0-rc.2

This is the current best-of-all x64 release.

It keeps the Floggfy v1.1.0-rc.4 capture engine and quality reporting, then adds the useful Soggfy-side features that are missing from stock Floggfy:

- Soggfy-style configurable output path templates, including `{release_date}`.
- Cached album artist, contributing artists, disc information and release year available to path templates.
- Conservative path-only artist separator cleanup that preserves names such as AC/DC.
- Install/uninstall scripts that back up and restore a pre-existing version.dll.
- Optional SpotX invocation using the current script URL and additive TLS 1.2 compatibility.
- Optional external FFmpeg post-processing for MP3, AAC/M4A, Opus and FLAC copies while native captures are retained by default.
- A diagnostics script that writes Spotify version, DLL hash, configuration and recent Soggfy log lines to Downloads.
- Current GitHub Actions, strict native regression tests, Windows x64 cross-builds, deterministic packaging and SHA-256 checksums.
- The original 2024 x86 Soggfy source preserved under legacy/ rather than mixed into the active build.

From Floggfy RC4, this also includes the read-only **To Disk** playback panel for current song, Spotify quality level, codec, average bitrate, sample rate and FLAC bit depth, including bounded observation when Downloads is off.

Compatibility is inherited from the Floggfy RC4 base: live tested there with Spotify 1.3.3.264 and resolver-validated against several earlier signed x64 Spotify DLLs. This fork has not yet been independently live-tested against every Spotify build.
