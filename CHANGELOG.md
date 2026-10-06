# Changelog

## 3.0.0-rc.36

- Fixed the RC35 crash in post-start CEF browser discovery.
- The crash dump shows an access violation in Soggfy VERSION.dll at +0x6D3E2 on the CrBrowserMain thread while reading address 0x1.
- RC35 incorrectly used CEF 151 browser-host method slot 8 as get_client; slot 8 is has_view and returns an integer, so a true result (1) was misinterpreted as a Client pointer.
- Use the correct CEF 151 get_client slot (9).
- Added runtime structure-size checks and executable-pointer validation before calling browser host/client/frame methods.
- Keep RC35's hookless CEF startup mode intact so this build still performs no CEF MinHook operations.
- RC33 accelerated native-EOS Next behavior, RC29 capture/publication and RC30 status indicators remain unchanged.

## 3.0.0-rc.35

- Removed the remaining CEF MinHook entry detours from the Classic metadata/UI bridge.
- The Classic bridge now waits 8 seconds after CEF initialization, then discovers Spotify's existing browser on the CEF UI thread with `cef_browser_host_get_browser_by_identifier`.
- After discovery it obtains the main frame and injects the existing Soggfy scripts directly; no browser-creation hook is needed.
- The console bridge is attached by patching only the concrete display-handler callback slot, with no process-wide thread suspension.
- Disabled the CEF URL-request MinHook and optional native To Disk menu MinHooks in this startup-safe build so RC35 performs no CEF MinHook operations at all.
- Block Telemetry remains configurable/persisted, but native CEF request interception is temporarily inactive in RC35.
- Kept RC33 original-style accelerated completion/Next behavior and all Spotify.dll capture/playback-speed hooks unchanged.
- Added startup diagnostics identifying hookless CEF discovery mode.

## 3.0.0-rc.34

- Reworked the Classic CEF callback bridge to remove five callback-time MinHook installs that could suspend Chromium UI/compositor threads while Spotify's browser was being created.
- CEF client, display and load callback interception now patches only the function-pointer slot on the concrete CEF C object, preserving object identity without process-wide thread suspension.
- Added race-safe slot replacement and restoration handling plus diagnostics identifying each direct callback patch.
- Kept the three initial audited CEF browser-creation hooks queued into a single MinHook apply; no additional MinHook enable occurs from inside browser/client callbacks.
- This specifically targets the intermittent full-primary-monitor black rendered surface that survived RC31-RC33 startup delays.
- RC33 original-style accelerated transport behavior, RC31 startup staging, RC29 capture/publication and RC30 status indicators are otherwise unchanged.

## 3.0.0-rc.33

- Rechecked the preserved original Soggfy implementation and restored its transport model: playback speed thins decoded PCM, while Spotify's own scrubber/timeline is left alone.
- Removed the synthetic accelerated scrubber/elapsed-time override added in RC31/RC32.
- Current Spotify does not reliably auto-advance when the accelerated decoder reaches EOS, unlike the older Spotify build original Soggfy targeted.
- When the native capture path proves accelerated EOS, Soggfy now immediately presses Spotify's real Next control exactly once.
- Falls back to the internal `skipToNext()` API only if the visible Next control is unavailable.
- Removed the fragile playbackId/URI gating that could prevent the completion action from running.
- Restored original Soggfy's `playback_stuck` recovery behavior for very high playback speeds.
- Kept RC31 startup hardening, RC29 accelerated capture/publication and RC30 track-status indicators unchanged.

## 3.0.0-rc.32

- Fixed RC31's accelerated scrubber override using the wrong CSS custom-property format.
- Current Spotify expects `--progress-bar-transform` to contain a full `translateX(...%)` transform; RC31 wrote a bare number, so the browser ignored it and left the visible scrubber on Spotify's raw 1x timeline.
- Apply the corrected transform to both the playback-progress root and progress-bar node for current Spotify layouts.
- Added per-playback accelerated UI diagnostics showing whether the root, bar and elapsed-time elements were found and the detected duration.
- Relaxed the accelerated next-track guard from volatile playbackId matching to the stable track URI.
- Added a guarded DOM Next-button fallback if `skipToNext()` returns without changing tracks.
- Added explicit accelerated handoff diagnostics for API attempt, DOM fallback, success and failure reasons.
- Kept RC31 startup hardening, RC29 accelerated capture/publication and RC30 track-status indicators unchanged.

## 3.0.0-rc.31

- Hardened intermittent Spotify startup rendering/blank-screen behavior by reducing hook churn during Chromium startup.
- The three initial Classic CEF browser hooks are now queued and enabled with one MinHook apply instead of three separate thread-suspension cycles.
- Deferred the optional CEF telemetry/ad request hook for 5 seconds after libcef loads so the Chromium compositor can settle first.
- Increased the Spotify.dll loader grace from 1.5 to 2.5 seconds and staggered playback-speed and capture hook families instead of enabling them back-to-back.
- Added startup-phase diagnostics for the CEF telemetry release and staggered Spotify native hook release.
- Added an accelerated-playback virtual transport clock so Spotify's visible elapsed time and scrubber follow Soggfy's effective 1-50x rate without seeking the real stream.
- Accelerated native EOS now sends a one-shot transport event to the Classic UI; if Spotify is still on that same playback, Soggfy advances to the next track automatically.
- The next-track handoff is identity-guarded so a delayed completion cannot accidentally skip a track Spotify already changed to.
- Preserved RC29's verified accelerated capture/publication path and RC30's per-track status indicators.

## 3.0.0-rc.30

- Restored old Soggfy-style per-track status indicators for current Spotify track rows.
- Normalized outer `role=row` wrappers before placing the indicator in the original final duration/actions cell.
- Added React-props fallbacks for track URI/title/artist/album when current Spotify omits those values from visible row links.
- Added a lightweight 1.5-second visible-row refresh so native IN_PROGRESS, CONVERTING, DONE and ERROR transitions appear without requiring a Spotify DOM mutation.
- Forced the compact status SVG container to remain visible under current Spotify row CSS.
- Added rate-limited-by-state `FLOGGFY_STATUS:classic rows=... results=... statuses=... rendered=...` diagnostics.
- Kept RC29 accelerated capture/download behavior unchanged.

## 3.0.0-rc.29

- Fixed accelerated tracks reaching complete Ogg EOS but never entering publication because Spotify's public SMTC timeline refreshes at a different rate from RC28's decoder-output thinning.
- During playback above 1x, a unique completed native stream that matches the current media duration and existing start-window association can now complete the listen directly from capture EOS.
- The completed stream must finish after the current listen begins; prefetched/ambiguous streams still do not qualify.
- Normal 1x seek/rewind/complete-listen validation is unchanged.
- Added `accelerated_complete` diagnostics and marks accelerated completions as `completion=capture_eos`.
- Kept strict Ogg sequence validation: real page gaps are still rejected rather than publishing incomplete audio.

## 3.0.0-rc.28

- Replaced RC26/RC27's unused Spotify 1.3.3.264 PCM filter-chain speed hook after live logging proved it never receives callbacks during active Ogg music playback.
- Re-analysed original Soggfy's x86 `DecodeAudioData` behavior and identified its x64 Spotify 1.3.3.264 equivalent at RVA `0x00d5a240`.
- Verified the decoder vtable at `0x01ae5b28`, dispatcher slot at `0x01ae5b30` and constructor assignment at `0x00d59a40`.
- Verified that the dispatcher calls the live Ogg decoder used by Soggfy's working native capture path and writes the produced PCM sample count through its R9 in/out parameter.
- Playback speed now lets Spotify consume/decode the full compressed packet and then reduces only the returned PCM sample count by the configured factor, matching old Soggfy's actual technique.
- Added exact byte validation for the decoder prologue, Ogg-decoder call and produced-count store before enabling the hook.
- Added `speed_decode_hook` diagnostics with PCM capacity, produced/kept samples, compressed input counts and thinning state.
- Retained RC27's host-independent ad/telemetry filtering.

## 3.0.0-rc.27

- Reworked Classic CEF ad/telemetry matching to follow BlockTheSpot's host-independent URL-path strategy.
- `/ads/`, `/ad-logic/`, `/gabo-receiver-service/` and `/dodo-receiver-service/` are now blocked regardless of Spotify hostname.
- URL queries/fragments are stripped before matching.
- Removed dependence on `spclient.wg.spotify.com` and `*-spclient.spotify.com` hostname patterns.
- Added regression coverage for unknown/future hosts while keeping metadata, audio CDN, login/update-style and unrelated paths allowed.
- Reviewed current SpotX ad-state/UI patches for future cosmetic cleanup, but RC27 does not patch Spotify's SPA.

## 3.0.0-rc.26

- Replaced the non-functional Spotify 1.3.3.264 SessionTrackPlayer/ContextPlayer speed experiments with the original Soggfy-style decoded-PCM thinning strategy.
- RC25 logging proved the configured rate changed while effective speed stayed at 1x and both native SessionTrackPlayer hook call counters remained zero.
- Added an exact, version-validated PCM filter-chain hook at RVA `0x00463954` with matching vtable slot `0x019c7c68`.
- The new hook lets Spotify process each live PCM block normally, then exposes only `produced / configured_speed` samples to the sink, preserving the complete compressed capture while accelerating audible playback.
- Added `speed_pcm_hook` diagnostics with input/produced/kept sample counts and conservative effective-speed reporting.
- Added regression tests for classic PCM thinning math.
- Expanded the CEF ad filter to current regional `*-spclient.spotify.com/ads/` and `/ad-logic/` endpoints while keeping normal Spotify metadata/audio traffic allowed.
- Added regional ad-filter regression tests.
- Retained RC25's loader-readiness gate and read-only IAT handling.

## 3.0.0-rc.25

- Fixed RC24 crashing while checking Spotify's loader readiness.
- Both RC24 minidumps point to the same `lock cmpxchg` write fault in Soggfy at the read-only `GetCommandLineW` IAT slot.
- Replaced `InterlockedCompareExchangePointer(..., nullptr, nullptr)` with a true read-only IAT load.
- Retained RC24's resolved-import checks and 1500 ms loader grace period.

## 3.0.0-rc.24

- Fixed the RC23 startup crash by deferring Spotify.dll-native hook installation until the Windows loader has resolved critical normal imports.
- Parsed the RC23 minidump and confirmed the exact same execute AV signature as RC22/RC13.
- Identified the failing call as Spotify.dll's normal `GetCommandLineW` IAT call; the crash target `0x01f7e69c` is the raw unresolved PE hint/name RVA for that import.
- Added a loader-readiness gate checking `GetCommandLineW`, `GetCurrentProcessId`, `GetModuleHandleW`, `GetProcAddress`, and `VirtualProtect` resolve to committed executable targets.
- Added a 1500 ms quiet grace period after those imports are resolved before MinHook or Spotify IAT patching begins.
- Connectivity, playback-speed and native audio-history hooks now all wait behind the same Spotify.dll loader gate.
- Retained RC23's rule that memory-scanned SessionTrackPlayer candidates are diagnostic-only and never invoked directly.

## 3.0.0-rc.23

- Fixed the RC22 startup crash caused by invoking Spotify methods on a SessionTrackPlayer-shaped object found only by process-memory scanning.
- Parsed two RC22 minidumps; both show the same `0xc0000005` execute AV at an address outside all loaded modules, with the immediate caller inside Spotify.dll.
- Memory-scanned SessionTrackPlayer candidates are now diagnostic-only and are never used as callable objects.
- Spotify 1.3.3.264 speed methods are invoked only after Spotify itself has supplied the exact SessionTrackPlayer `this` pointer through the hooked setter/getter.
- Added first-call diagnostics for genuine SessionTrackPlayer setter/getter invocations, including native/requested speed and mode.
- Maintenance may reapply and verify speed only on a hook-observed live SessionTrackPlayer.
- Added explicit setter/getter call counters when Spotify never exposes a live speed object.

## 3.0.0-rc.22

- Fixed RC21 rejecting the exact Spotify 1.3.3.264 SessionTrackPlayer object because its dispatcher/player interface methods were required to reside inside Spotify.dll.
- Live RC21 logging consistently showed `raw_hits=1 valid=0` while tracks were playing, proving discovery found the exact vtable object but validation discarded it.
- SessionTrackPlayer validation now accepts virtual methods from any committed executable module while retaining the exact Spotify vtable match and readable object-layout checks.
- The native setter is now attempted even when Spotify reports playback-speed automation entries; the native getter decides whether the request actually took effect.
- Added `speed_session_raw` diagnostics for exact-vtable objects that still fail validation, including dispatcher/player pointers, method executability, vector size and cached speed.
- Effective speed remains verified against Spotify's live underlying-player getter before Soggfy reports accelerated playback.

## 3.0.0-rc.21

- Replaced RC20's Spotify 1.3.3.264 AudioSessionImpl track-creation speed hook after live logging proved that exact routine never ran for the active music session, even across new tracks.
- Identified the live SessionTrackPlayer vtable at RVA `0x01a07308`, native speed setter at `0x005a8d18` (slot `+0xc0`) and native speed getter at `0x005a17d8` (slot `+0xc8`).
- Added exact validation of the SessionTrackPlayer setter/getter byte sequences, vtable slots and constructor vtable assignment before enabling the backend.
- Hooks Spotify's own SessionTrackPlayer speed setter/getter and tracks the live object directly.
- Falls back to a bounded process-memory scan for the exact SessionTrackPlayer vtable when Spotify has not exposed a live object through the hooked methods yet.
- Applies configured speed through Spotify's own SessionTrackPlayer dispatcher and verifies the result using Spotify's native getter.
- `speed_effective` is updated only after a live underlying player reports the requested rate.
- Added `speed_session_candidate` and `speed_session_scan` diagnostics, including live-player presence, native speed and playback-speed automation count.
- Reports pending rather than success when Spotify's own playback-speed automation blocks a manual non-1x rate.
- Added the upstream Floggfy recommendation to disable Spotify Automix because trimmed playback breaks complete-listen capture.

## 3.0.0-rc.20

- Replaced the Spotify 1.3.3.264 ContextPlayer speed backend after RC19 showed that the only matching AudioSessionImpl instances stayed inactive during real music playback.
- Revalidated the official 1.3.3.264 Spotify.dll and identified the actual track-player creation routine at RVA `0x0057968c`.
- The identified routine receives playback speed as its fourth Windows x64 argument in XMM3 and logs that value as `speed: %f`; its stack arguments match Soggfy's existing 13-argument track-player hook ABI.
- Added exact byte validation for the target prologue, XMM3 speed-copy sequence, stack-argument layout and speed-log store before any hook is installed.
- The 1.3.3 backend no longer relies on RC13's fuzzy target discovery or runtime ContextPlayer memory scanning.
- Speed changes on 1.3.3.264 take effect on the next TrackPlayer creation, so changing speed mid-song requires restarting the song or starting another track.
- `speed_effective` is updated only when the 1.3.3 track-create hook actually runs.
- Added a concise `Spotify 1.3.3 track-player create` diagnostic containing native/requested/applied speed.

## 3.0.0-rc.19

- Added detailed, rate-limited diagnostics for Spotify 1.3.3.264 playback-speed discovery after RC18 still failed to locate an active TrackPlayer.
- Logs the Spotify module/vtable/wrapper addresses used by the backend.
- Logs writable-region scan coverage, raw ContextPlayer vtable hits, valid/inactive/invalid layout counts, current/prepared candidate counts and the selected context.
- Logs raw current/prepared/dispatcher fields for candidate ContextPlayer objects and whether those player pointers match the expected TrackPlayer shape.
- Scans the nearby ContextPlayer object for player-like pointers and records their offsets plus vtable/setter/getter RVAs, making changed 1.3.3 object offsets much easier to identify from one user log.
- Diagnostics run only while a speed above 1x is requested and are limited to one scan report every five seconds.

## 3.0.0-rc.18

- Fixed Spotify 1.3.3.264 playback speed remaining at 1x because RC17 could cache an inactive startup ContextPlayer whose current and prepared TrackPlayer pointers were both null.
- Context discovery now ignores inactive ContextPlayer objects and keeps rescanning until a real current or prepared TrackPlayer exists.
- A unique current-track ContextPlayer is preferred over prepared-only candidates.
- Cached ContextPlayer state is discarded and reacquired when it becomes inactive across startup or track transitions.
- Full ContextPlayer memory scans stay on the maintenance path rather than the Classic UI/settings caller.
- The log now records the acquired active ContextPlayer plus current/prepared TrackPlayer addresses.
- RC17's native speed getter verification and direct TrackPlayer setter fallback remain in place.

## 3.0.0-rc.17

- Fixed RC16 reporting configured playback speed as effective even when Spotify 1.3.3.264 was still playing at 1x.
- Added real TrackPlayer speed verification through Spotify's own getter instead of trusting the ContextPlayer wrapper return value.
- If Spotify's 1.3.3 ContextPlayer wrapper is a no-op, Soggfy now falls back to the underlying TrackPlayer vtable speed setter used by Spotify itself and verifies the result immediately.
- Current and prepared players are handled separately with the mode values observed in Spotify 1.3.3.264's native wrapper code.
- Capture/listen timing now uses the verified effective playback rate instead of the configured slider value, preventing false position-rewind failures when a speed request was not actually applied.
- Soggfy.log now records `speed_verified` or `speed_failed` with requested/effective rates, ContextPlayer/TrackPlayer addresses, wrapper results and before/after native speed readings.

## 3.0.0-rc.16

- Restored native playback-speed support on Spotify for Windows x64 1.3.3.264 with a new version-specific backend.
- Reverse-engineered the official 1.3.3.264 Spotify.dll from the current x64 installer and identified Spotify's own ContextPlayer current/prepared-track playback-speed methods.
- The 1.3.3 backend no longer hooks the obsolete track-player constructor used by older Spotify builds.
- Startup validates the exact 1.3.3.264 method prologues, ContextPlayer vtable assignment and speed-method vtable slots before enabling the feature.
- Soggfy locates the live ContextPlayer instance conservatively and re-validates its current/prepared player objects before applying speed.
- Speed changes on 1.3.3.264 apply directly without recreating the current track, and the configured speed is maintained across track transitions.
- Spotify 1.3.1.234 keeps the older validated constructor-hook backend; unknown builds still fail closed at 1x.
- Added backend-selection regression coverage for both supported Spotify versions.

## 3.0.0-rc.15

- Improved the Modern Capture panel so Ogg/Vorbis format and sample rate can appear as soon as a plausible native decoder stream is observed near the start of the current track.
- Added a separate Capture association row with Pending, Matched, Complete, Ambiguous or Unavailable state.
- Kept strict identity rules: a detected Ogg header is shown as detected/pending, not promoted to a validated current-track format until duration/identity corroboration succeeds.
- Mid-track read-ahead remains hidden to avoid labeling the next song's decoder as the current song.
- Ogg is now labeled Ogg/Vorbis in the UI.
- On Spotify builds where accelerated playback is disabled for ABI safety, the settings page now shows a clear read-only 1x status instead of an inert slider.
- Added regression coverage for pending native-stream detection, sample-rate display and association labels.

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
