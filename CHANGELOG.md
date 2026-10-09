# Changelog

## 3.0.0-rc.42

- Fixed Spotify failing to launch when a persisted accelerated playback speed such as 50x was already configured at process startup.
- The native speed hook now installs in a safe disarmed state and passes Spotify's own native speed until the live player/UI explicitly arms acceleration.
- UI-requested speed changes and RC41 natural-transition recovery arm acceleration immediately before rebuilding the real current track.
- Hardened the Spotify connectivity compatibility hook so an unresolved delay-IAT entry is never overwritten; Soggfy now waits for Windows to resolve that import before patching it.
- This addresses the repeated startup crash signature that attempted to execute a low raw Spotify RVA before the normal Soggfy initialization logs completed.

## 3.0.0-rc.41

- Fixed accelerated playback dropping back to real 1x after Spotify naturally advances to the next track.
- Spotify 1.3.1.234 can reuse a pre-created next-track player on natural transitions, bypassing the constructor path where Soggfy applies the configured speed.
- Soggfy now recreates each newly-current track once when accelerated playback is enabled so the validated native speed hook is applied consistently.
- Added a URI guard and short retry/watchdog path so Soggfy's own recreation cannot loop and natural transitions are handled even when Spotify does not emit the same update event as a manual Next click.
- Manual Next remains supported and the configured speed persists across the queue.

## 3.0.0-rc.40

- Fixed the Classic Soggfy UI intermittently missing during normal Spotify 1.3.1.234 startup even though the native backend loaded successfully.
- Kept the existing CEF browser-creation callbacks and added post-start discovery of an already-created browser when no valid main frame was captured.
- Added validated browser, host, client and frame access in the fallback path.
- Uses the corrected CEF 151 BrowserHost get_client method slot identified during the earlier crash investigation.
- The fallback starts after a short grace period and retries on later CEF UI polls until a valid browser is found.
- Spotify 1.3.1.234 playback speed, capture, metadata, and installer behavior are otherwise unchanged.

## 3.0.0-rc.39

- Removed Soggfy's own ad-request blocking; the native CEF filter now blocks telemetry only.
- The native `Block Telemetry` option now targets only the Gabo and Dodo receiver-service endpoints.
- Ad paths such as `/ads/` and `/ad-logic/` are deliberately left to SpotX.
- Spotify client-update traffic remains untouched by Soggfy and is delegated to SpotX as well.
- The installer now offers SpotX by default, matching the original Soggfy workflow.
- SpotX is launched against the pinned Spotify 1.3.1.234.g59d6bf59 baseline with normal ad blocking and `-block_update_on`.
- Added `-SkipSpotX` for users who explicitly do not want SpotX and retained `-RunSpotX` for unattended installs.
- The installer validates Spotify 1.3.1.234 before patching so SpotX is not accidentally applied to the wrong client build.
- SpotX runs before Soggfy is copied into the Spotify directory, matching the safer original install order.
- RC38's clean Spotify 1.3.1.234 runtime rollback is otherwise unchanged.

## 3.0.0-rc.38

- Reverted the active runtime to the last known-good Spotify 1.3.1.234 codebase from RC14.
- Removed the Spotify 1.3.3.264 playback-speed experiments introduced after RC14, including ContextPlayer/SessionTrackPlayer probing, PCM helper hooks and the later DecodeAudioData-equivalent backend.
- Removed the RC29 accelerated-EOS completion override and RC31-RC37 scrubber, next-track, CEF startup-race and delayed-hook experiments.
- Restored the original validated 1-50x Spotify 1.3.1.234 track-player constructor hook unchanged.
- Restored the RC14 CEF metadata/UI, telemetry filter, native To Disk fallback and normal Spotify.dll startup behavior.
- Restored RC14 complete-listen validation and playback-rate-aware timing without the later 1.3.3-specific completion workarounds.
- Spotify 1.3.1.234 is the intended test baseline for this release. Other Spotify builds fail closed for native playback speed.
- This release is deliberately a clean rollback rather than a hybrid of RC14 and the later 1.3.3 experiments.

## 3.0.0-rc.14

- Fixed Spotify 1.3.3.264 crashing during startup when Soggfy's native playback-speed hook was installed.
- Crash-dump analysis showed repeatable execute access violations at a raw low Spotify RVA before the UI loaded, while the upstream Floggfy startup path remained stable.
- Playback-speed injection now fails closed by Spotify file version instead of trusting the pattern match alone.
- The native 1-50x speed hook remains enabled only on the runtime-validated Spotify 1.3.1.234 ABI for now.
- Spotify 1.3.3.264 and other unvalidated builds run at normal 1x speed, leaving capture, Classic UI, metadata and conversion functionality available.
- The Classic settings slider now displays effective 1x and is disabled when the native speed hook is unavailable.
- Added regression coverage for the playback-speed compatibility policy.

## 3.0.0-rc.13

- Expanded Soggfy.log so normal logging records enough state to diagnose capture failures without requiring a debug build.
- Logs capture configuration changes including configured/effective playback speed, speed-hook availability, codec toggles, output preset, FFmpeg path and save root.
- Logs Ogg BOS/EOS details, page sequence/serial/granule state on rejection, compressed byte/page counts and capture-vs-media duration.
- Logs raw Windows media position separately from Soggfy's extrapolated position, timeline snapshot age, playing state and playback rate.
- Incomplete-listen failures now state the exact reason: start too late, media-clock discontinuity, rewind, position ahead of expected progress or duration change.
- Logs full-listen completion, stream association candidates, publication destination, native/output format and post-processing result.
- Debug Log additionally records every sampled media timeline and replayed Ogg-page details.
- Added regression coverage for the new listen rejection diagnostics.

## 3.0.0-rc.12

- Changed GitHub Releases to publish one all-in-one Windows x64 ZIP instead of multiple build assets.
- The root of the ZIP contains automatic `version.dll` mode, settings, scripts, build information, checksums and documentation.
- Optional launcher mode now lives under `Launcher/` in the same ZIP.
- Release CI verifies that both automatic and launcher payloads are present in the single archive.
- GitHub's automatically generated source-code ZIP and tarball remain visible separately.

## 3.0.0-rc.11

- Synced selected upstream improvements through Mainkill1/Floggfy v1.1.0.
- Added Floggfy 1.1.0's optional explicit launcher as a separate `Soggfy.exe` + `Soggfy.dll` startup mode while keeping normal `version.dll` automatic mode unchanged.
- Launcher mode stops running Spotify processes from the same installation, starts a fresh process, explicitly loads the adjacent DLL before application startup and waits for Soggfy readiness.
- Kept automatic and launcher packages separate so the two injection modes cannot accidentally be installed together.
- Ported upstream episode current-item handling so sparse episode titles remain visible even when music artist/album fields are absent.
- Episode playback quality identity now uses the Spotify episode URI, preventing same-title episodes from borrowing stale decoder details.
- Retained Soggfy's newer Ogg replay tolerance, accelerated complete-listen validation, Classic UI, legacy-library matching and post-processing instead of rebasing onto the older upstream capture core.
- Updated release packaging, checksums, CI verification, README and upstream tracking for the new launcher artifact.

## 3.0.0-rc.10

- Fixed complete-listen validation incorrectly assuming 1x playback while Soggfy's native speed control was running Spotify faster.
- Media-session position extrapolation now advances using the active Soggfy playback rate when the validated speed hook is available.
- Listen start-time, expected-position, natural-end and delayed-title calculations are now playback-rate aware.
- High-speed polling tolerances scale with playback rate so normal 10x-50x progress is not mistaken for a seek.
- Real seeks/skips still invalidate the listen, and publication still requires a complete validated Ogg/FLAC stream.
- Added regression coverage for 50x playback, natural transitions, delayed title updates and real forward seeks.

## 3.0.0-rc.9

- Fixed valid Ogg captures being discarded when Spotify internally replays already-consumed Ogg pages during buffering/decoder reuse.
- Replayed pages from the same logical stream are ignored instead of being appended twice or treated as a missing-page failure.
- Replayed Vorbis BOS/identification pages no longer replace an in-progress capture.
- Genuine forward page gaps, CRC failures, serial mismatches, corrupt granules, seeks and incomplete listens are still rejected.
- Added regression coverage proving a capture can survive duplicate data and BOS pages and still complete normally.

## 3.0.0-rc.8

- Made Skip Downloaded Tracks recognize existing Soggfy audio regardless of the currently selected output format.
- Added conservative legacy flat-library detection for exact `Artist - Track.ext` and `All Artists - Track.ext` filenames.
- Legacy matching supports MP3, M4A/MP4, Ogg, Opus, FLAC, AAC and WAV files.
- Preserved old Soggfy filename escaping modes and the historical all-artists slash-to-comma behavior.
- Legacy matches are combined with normal template matches and only count as downloaded when exactly one file matches, avoiding ambiguous skips.
- Added regression coverage based on flat MP3 libraries, multi-artist files, AC/DC escaping and format changes.

## 3.0.0-rc.7

- Fixed the Classic Soggfy settings/sliders button not opening reliably in Spotify's live top bar.
- Stopped borrowing Spotify's navigation-button CSS class for injected Soggfy controls.
- Made the Downloads and Settings controls explicit standalone `type="button"` elements with isolated click handling.
- Kept the retractable top-bar layout stable while expanding to expose the settings button.
- Mounted the settings overlay directly instead of through an unstyled wrapper and raised it above Spotify UI layers.
- Added modal focus and Escape-to-close handling.

## 3.0.0-rc.6

- Ported the full old Soggfy-style UI workflow onto the modern x64 backend.
- Restored per-track IN_PROGRESS, CONVERTING, DONE, ERROR, WARN and IGNORED status indicators.
- Restored Skip Downloaded Tracks and Skip Ignored Tracks.
- Fixed upstream issue #150 by using the same canonical path escaping for file creation and downloaded-track lookup; AC/DC and Gary Numan / Tubeway Army are covered by regression tests.
- Restored resource-aware Ignore / Unignore behavior for tracks, episodes, albums, playlists and artists.
- Restored Generate M3U using the real downloaded-file status backend.
- Restored integrated MP3, M4A/AAC, Opus and custom FFmpeg output presets after validated native capture.
- Added automatic native-AAC fallback for old FDK AAC presets when libfdk_aac is unavailable.
- Restored cover-art embedding/saving and lyrics embedding/saving; synchronized lyrics use .lrc and plain lyrics use .txt.
- Restored Canvas saving with bounded native downloads and atomic publication.
- Restored the Block telemetry toggle with a scoped CEF request filter.
- Restored separate podcast/episode metadata and Podcast template handling.
- Restored the 1–50x playback-speed setting using a validated modern x64 Spotify.dll track-player hook that fails closed on unsupported layouts.
- Preserved Floggfy RC5's current Song / Quality / Format / Sample-rate readout inside Classic settings.
- Added a bounded shared downloaded-file index cache so status ticks and Skip Downloaded do not rescan the output tree for every UI mutation.
- Retained the Floggfy RC5-derived native Ogg/FLAC engine, cached metadata, dynamic hook discovery, diagnostics and hardened release pipeline.

## 3.0.0-rc.5

- Replaced the Floggfy-style To Disk menu as the default interface with a modern reimplementation of old Soggfy's Spotify integration.
- Added the old-style top-bar Downloads toggle and Soggfy settings button/modal.
- Wired Classic UI controls directly to the modern x64 capture settings: Downloads, Ogg, FLAC, save location, path templates, artist normalization, metadata and logging.
- Kept the native To Disk menu as an opt-in fallback only (`Classic UI=0`, `Native Menu=1`).
- Kept the CEF bridge alive when metadata is disabled so the Soggfy UI itself still works.
- Added JavaScript syntax validation for the injected Classic UI.
- Did not restore the obsolete localhost WebSocket server, x86 hooks, playback-speed downloader or other legacy transport code.

## 3.0.0-rc.4

- Hardened the release build without changing the RC5-derived capture runtime.
- Moved release DLL production to a newer Windows/MSYS2 MinGW64 path with GCC 14+ and binutils 2.44+ minimum checks.
- Added Windows VERSIONINFO metadata generated from the repository VERSION file.
- Added exact build-toolchain recording in BUILDINFO.txt.
- Added raw DLL release assets and external SHA-256 coverage for both the DLL and final ZIP.
- Added CI verification for the version resource, MSVCRT target and release checksum manifest.

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
