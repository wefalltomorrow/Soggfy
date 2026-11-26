# Floggfy upstream review — November 26, 2026

## Reviewed source

The current `main` branch and `v1.4.1` tag both resolve to
`15e1e286ea1ac474be4aff30e6db1541ba790340` (rechecked November 26, 2026).
The releases list still identifies v1.4.1, published November 21, 2026, as
the newest release. The comparison uses the tagged release and its source, rather than treating release
notes as a compatibility or correctness test for Soggfy. References: [v1.4.1
release](https://github.com/Mainkill1/Floggfy/releases/tag/v1.4.1), [reviewed
source commit](https://github.com/Mainkill1/Floggfy/commit/15e1e286ea1ac474be4aff30e6db1541ba790340),
and [upstream implementation audit PR #34](https://github.com/Mainkill1/Floggfy/pull/34).

The release line adds a Saved-date catalog and sorting, playback following that
order, expanded telemetry controls and diagnostics, network blocking across
helper processes, offline-session telemetry suppression, metadata/artwork/lyrics
fallbacks, WebP artwork decoding, and automatic finalization when an MP3 podcast
handoff ends. It also includes broad cleanup and performance changes. The audit
PR is useful as a feature inventory; its proposals and aggregate counts are not
independent evidence that each change should be copied.

## Comparison and port decisions

| Area | Soggfy today | Decision |
| --- | --- | --- |
| Saved-date library and playback order | No saved-date catalog or ordering workflow | Product candidate. Design catalog ownership, refresh, persistence, and queue consistency first; this is a feature, not a safe source-level cherry-pick. |
| Telemetry controls | Default-on privacy filter for the Gabo/Dodo receiver endpoints; ads and update traffic remain SpotX's responsibility | Keep the narrow boundary. Floggfy's telemetry categories/dumps may inform diagnostics, but broad endpoint blocking should not be imported without separate behavior and privacy tests. |
| Network controls | Separate, explicit default-off `NetworkPatch` compatibility option | Not equivalent to Floggfy's process-wide/helper-process kill switch. Any broader switch needs its own opt-in, process coverage, recovery behavior, and tests against the pinned Spotify client. |
| Artwork and lyrics | Existing metadata cache, artwork embedding/sidecars, and lyric embedding/sidecars | Reuse the fallback/refresh ideas selectively after mapping them to Soggfy's cache and metadata-preservation rules; avoid replacing the existing path wholesale. |
| WebP artwork | No WebP decoder path found in the reviewed Soggfy source; PNG/JPEG validation is present | Low-priority candidate. Add only with bounded input limits and decoder fixtures if WebP artwork is observed in the pinned client. |
| Output formats and podcasts | Ogg/FLAC native capture, with FFmpeg conversion presets including MP3 and M4A/AAC; podcast paths and metadata exist | Floggfy's MP3 podcast handoff is not native MP3 capture. Keep production native MP3 capture gated by Soggfy's separate identity, lifetime, and completeness evidence. |
| Performance and cleanup | Soggfy has bounded queues, transactional publication, and strict validation; the current change fixes a pathname cleanup race | Port only measured changes. The local race fix below addresses a concrete correctness issue and has a Windows regression test. |

Floggfy's published compatibility/test set includes Spotify 1.2.97, 1.3.9, and
1.3.10. That does not establish behavior on Soggfy's pinned Spotify
1.3.1.234.g59d6bf59. No upstream feature is considered accepted for Soggfy until
it passes Soggfy-specific tests and client acceptance.

## Local correctness fix validated with this review

Failed writes in `native/file_publication.cpp` and the audio-history sidecar
writer now mark the exact still-open file handle for deletion before closing it.
This removes the race where cleanup by pathname could delete a different file
created at the original path after the failed file was closed.
`tests/windows_history_test.cpp` renames the held file, creates a replacement at
the old path, then verifies handle-based cleanup removes only the held file and
preserves the replacement.

Validation on November 25, 2026: the Windows history regression passed, and the
complete test-native-windows.sh suite passed with MinGW x64 GCC 16.2.0/MSVCRT.
That run included the exact kernel32.dll/msvcrt.dll helper-import check,
PNG/JPEG decoding, MP3 parsing and integration, Ogg/FLAC metadata-refresh decode,
cache identity, playback-quality, and stream-reconstruction checks. Focused
import-policy and digest tests also passed. The import verifier accepts both
GNU and LLVM objdump PE metadata while continuing to reject UCRT/API-set DLLs.
An isolated full release build, package construction, PE/import checks, ZIP
manifest verification, and comparison of the packaged helper to the tested
helper also passed. The locally built ZIP SHA-256 is
2f34b35288caf0e178f65d845af2d0a0669f634339ec57e880e34ad1c4110cb1. The package
atomicity/determinism regression and git diff --check passed. CI and live Spotify
acceptance have not been run for these uncommitted source changes.
