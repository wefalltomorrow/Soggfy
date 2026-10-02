# Changelog

## 3.0.0-rc.2

- Synced the active x64 engine through Floggfy v1.1.0-rc.4.
- Added read-only current playback information to **To Disk**: song, Spotify quality level, codec, average bitrate, sample rate and FLAC bit depth.
- Quality observation remains available with Downloads off and uses bounded decoder/header observation rather than full capture buffers.
- Added RC4's freshness, identity, ambiguity, replay/seek and codec-conflict protections for quality reporting.
- Added the RC4 Linux and Windows playback-quality regression tests.
- Kept Soggfy's custom path templates and cached-metadata path enrichment on top of the RC4 engine.
- Restored the old Soggfy `{release_date}` path token using identity-checked cached metadata.
- Store optional AAC conversions in an M4A container so metadata and artwork have a proper container.

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
