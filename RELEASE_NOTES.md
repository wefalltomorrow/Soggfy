# Soggfy v3.0.0-rc.3

This release syncs the active x64 engine through Floggfy v1.1.0-rc.5 while retaining the extra Soggfy-side features in this fork.

RC5 fixes the To Disk footer sometimes showing no song or the previous song. Current-track state is refreshed from Spotify's already-cached player state once per second, visible labels are formatted through CEF's public FormatLabel callback, and decoder details are kept only when current identity and quality agree. The footer now shows Song, Quality, Format and Sample rate/bit depth; the Bitrate row was intentionally removed upstream.

On top of RC5, this fork still adds:

- Soggfy-style configurable output path templates, including `{release_date}`.
- Cached album artist, contributing artists, disc information and release year/date available to path templates.
- Conservative path-only artist separator cleanup that preserves names such as AC/DC.
- Install/uninstall scripts that back up and restore a pre-existing version.dll.
- Optional SpotX invocation using the current script URL and additive TLS 1.2 compatibility.
- Optional external FFmpeg post-processing for MP3, AAC/M4A, Opus and FLAC copies while native captures are retained by default.
- A diagnostics script that writes Spotify version, DLL hash, configuration and recent Soggfy log lines to Downloads.
- Current GitHub Actions, strict native regression tests, Windows x64 cross-builds, deterministic packaging and SHA-256 checksums.
- The original 2024 x86 Soggfy source preserved under legacy/.

Compatibility is inherited from the Floggfy RC5 base: RC5 was live-validated on Windows Spotify 1.3.3.264 for first menu opening and consecutive track skips. Older-client live behavior remains unvalidated.
