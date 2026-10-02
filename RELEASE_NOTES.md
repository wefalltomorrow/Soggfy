# Soggfy v3.0.0-rc.1

First release of the modern x64 continuation.

Highlights:

- Modern Floggfy v1.1.0-rc.3 capture base for current Windows x64 Spotify.
- Native Ogg and FLAC capture with complete-listen validation and quality-aware atomic replacement.
- Soggfy-style configurable output path templates.
- Cached album artist, contributing artists, disc information and release year can be used in paths.
- Safer path-only artist separator cleanup that does not turn names such as AC/DC into AC, DC.
- Install/uninstall scripts with backup and restore of a pre-existing version.dll.
- Optional FFmpeg post-processing for MP3, AAC, Opus and FLAC copies while retaining native captures by default.
- Diagnostics script that writes Spotify version, DLL hash, configuration and recent log lines to Downloads.
- Full regression tests and reproducible Windows x64 CI packaging.

Compatibility is inherited from the Floggfy RC3 base: live tested there with Spotify 1.3.3.264, with resolver validation against several earlier signed x64 Spotify DLLs. This fork has not yet been independently live-tested against every Spotify build.
