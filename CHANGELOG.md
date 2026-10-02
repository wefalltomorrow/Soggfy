# Changelog

## 3.0.0-rc.1

- Rebased the active implementation on Floggfy v1.1.0-rc.3 instead of the obsolete x86 Soggfy hooks.
- Added native Ogg and FLAC capture with complete-listen validation, atomic publication and quality-aware replacement.
- Added dynamic Spotify audio-hook discovery and import-name based connectivity repair.
- Added local/cache-only rich metadata enrichment.
- Added Soggfy-style configurable path templates.
- Added cached album-artist, contributing-artist, disc and release-year values to template rendering.
- Added conservative path-only artist separator normalization that leaves names such as AC/DC intact.
- Added PowerShell install/uninstall scripts with backup/restore handling.
- Added optional FFmpeg post-processing outside the injected capture DLL.
- Added Linux regression tests, MinGW x64 build CI and deterministic release packaging.
- Preserved the previous x86 code under legacy/.
