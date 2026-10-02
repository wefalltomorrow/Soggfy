# Changelog

## 3.0.0-rc.5

- Replaced the Floggfy-style To Disk menu as the default interface with a modern reimplementation of old Soggfy's Spotify integration.
- Added the old-style top-bar Downloads toggle and Soggfy settings button/modal.
- Wired Classic UI controls directly to the modern x64 capture settings: Downloads, Ogg, FLAC, save location, path templates, artist normalization, metadata and logging.
- Kept the native To Disk menu as an opt-in fallback only (`Classic UI=0`, `Native Menu=1`).
- Kept the CEF bridge alive when metadata is disabled so the Soggfy UI itself still works.
- Added JavaScript syntax validation for the injected Classic UI.
- Did not restore the obsolete localhost WebSocket server, x86 hooks, playback-speed downloader or other legacy transport code.

# Changelog

## 3.0.0-rc.4

- Hardened the release build without changing the RC5-derived capture runtime.
- Moved release DLL production to a newer Windows/MSYS2 MinGW64 path with GCC 14+ and binutils 2.44+ minimum checks.
- Added Windows VERSIONINFO metadata generated from the repository VERSION file.
- Added exact build-toolchain recording in BUILDINFO.txt.
- Added raw DLL release assets and external SHA-256 coverage for both the DLL and final ZIP.
- Added CI verification for the version resource, MSVCRT target and release checksum manifest.

# Changelog

## 3.0.0-rc.3

- Synced the active x64 engine through Floggfy v1.1.0-rc.5.
- Fixed the To Disk footer sometimes showing no song or the previous song.
- Refreshes current-track state from Spotify's cached player state once per second without endpoint requests.
- Formats visible To Disk footer labels through CEF's public FormatLabel callback before display.
- Removed the bitrate footer row to match RC5's stricter current-track reporting.
- Keeps decoder details only when current track identity and quality agree; stale or conflicting state remains unavailable.
- Retains Soggfy path templates, cached metadata path enrichment, installer/uninstaller, diagnostics and post-processing on top of RC5.

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
