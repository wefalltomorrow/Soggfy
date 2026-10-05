# Soggfy

A maintained Windows x64 continuation of Soggfy with the old Soggfy Spotify UI on top of the modern Floggfy capture engine.

The goal is to keep the interaction model people used in old Soggfy while replacing the obsolete x86 hooks, localhost control server and pinned 2024 Spotify build with the current x64 backend.

## What it does

- Uses the old Soggfy-style top-bar Downloads button and Soggfy settings modal.
- Shows per-track status icons for downloading, converting, completed, failed, warning and ignored states.
- Restores Skip Downloaded Tracks, Skip Ignored Tracks, Ignore/Unignore and Generate M3U.
- Restores the old 1–50x playback-speed control on explicitly validated Spotify builds. Spotify 1.3.1.234 uses the legacy constructor backend; 1.3.3.264 uses Spotify's exact validated SessionTrackPlayer speed setter/getter and verifies the live rate before reporting success. Unknown builds fail closed at 1x.
- Restores old-style output presets including MP3, M4A/AAC, Opus and custom FFmpeg output.
- Always captures Spotify's native Ogg or FLAC first, and keeps native FLAC lossless.
- Embeds artwork, lyrics and rich locally cached metadata when enabled.
- Can save cover art, synchronized lyrics as .lrc, plain lyrics as .txt, and Spotify Canvas video.
- Uses separate track and podcast path templates.
- Refuses incomplete, skipped, seeked or broken captures instead of publishing partial files.
- Skips equal or better existing files and atomically replaces lower-quality copies.
- Uses dynamic x64 hook discovery instead of fixed Spotify offsets.
- Keeps capture, UI, metadata, playback-speed and telemetry integrations independent.
- Preserves Floggfy RC5's current Song / Quality / Format / Sample-rate information inside the Classic settings UI.
- Uses a bounded, short-lived file index for downloaded-track ticks/Skip Downloaded instead of recursively rescanning the library on every UI mutation.
- Recognizes legacy Soggfy libraries, including flat `Artist - Track` / `All Artists - Track` files and existing audio in a different output format.
- Keeps Floggfy's native To Disk menu only as an optional troubleshooting fallback.

## Install

The Floggfy 1.1.0 capture/startup base was live-tested against Windows x64 Spotify 1.3.3.264. Its dynamic audio/connectivity resolver was also validated against signed Spotify DLLs from 1.3.0.277, 1.2.94.583 and 1.2.92.148. Soggfy has separate validated playback-speed backends for Spotify 1.3.1.234 and 1.3.3.264. The 1.3.3.264 backend validates Spotify's SessionTrackPlayer vtable, native speed setter/getter and constructor assignment before enabling it. It follows the live session and applies changes through Spotify's own playback dispatcher, then verifies the actual rate with Spotify's getter. The Classic Soggfy UI still needs normal real-client validation as Spotify UI internals change over time. Microsoft Store installs remain unvalidated.

Releases use a single all-in-one Windows x64 ZIP. It contains both startup modes; install **only one**.

**Automatic mode** is the normal choice:

1. Quit Spotify.
2. Download `Soggfy-v*-Windows-x64.zip` from Releases and extract it.
3. Run `Scripts\Install.ps1`, or manually copy the root `version.dll` beside `Spotify.exe` (normally `%APPDATA%\Spotify`).
4. Start Spotify normally.

The installer backs up a pre-existing `version.dll` instead of silently overwriting it. `Scripts\Uninstall.ps1` restores that backup.

**Launcher mode** is an optional fallback when Windows/Spotify skips the adjacent `version.dll`:

1. Quit Spotify and remove Soggfy's automatic `version.dll` from the Spotify folder.
2. From the same ZIP, copy `Launcher\Soggfy.exe`, `Launcher\Soggfy.dll` and `Launcher\SpotifyHistory.ini` beside `Spotify.exe`.
3. Start `Soggfy.exe`. It stops Spotify processes from that same installation, starts a fresh Spotify process and explicitly loads the adjacent `Soggfy.dll`.

After either startup mode, use the Soggfy Downloads button in Spotify's top bar, open settings with the sliders button, and play a track from start to finish without seeking or skipping.\n\nDisable Spotify **Automix** under **Edit -> Preferences -> Playback** while using complete-listen capture. Automix trims tracks and can prevent a download from satisfying Soggfy's full-listen validation.

## Classic Soggfy UI

The default interface follows the old Sprinkles workflow rather than Floggfy's To Disk menu.

The settings modal includes playback speed, output format, Skip Downloaded, Skip Ignored, cover-art and lyrics options, Canvas saving, Base/Track/Podcast/Canvas paths, invalid-character replacement, Block telemetry, Move Add to Queue to top, native FLAC/Ogg controls, cached metadata, logging, diagnostics and whether to retain the native original after conversion.

Track rows use the old status model:

- downloading / in progress
- converting
- completed
- failed
- warning
- ignored

Completed tracks can be opened in Explorer from the status indicator.

The context menu restores Ignore / Unignore and Generate M3U. Ignore rules can apply to track, episode, album, playlist or artist resource URIs, and Skip Ignored respects them.

## Output formats

Capture always starts from Spotify's native compressed Ogg or FLAC data. Optional FFmpeg conversion happens only after the complete native file has been validated and published.

Built-in presets include:

- Original OGG / FLAC
- MP3 320K
- MP3 256K
- MP3 192K
- M4A 256K (FDK AAC)
- M4A 224K VBR (FDK AAC)
- M4A 160K (FDK AAC)
- Opus 160K
- Custom FFmpeg arguments

If an FFmpeg build does not include libfdk_aac, the old FDK-labelled M4A presets automatically fall back to FFmpeg's native AAC encoder instead of simply failing.

By default the validated native Ogg/FLAC is retained after conversion. Scripts\PostProcess.ps1 is also included for manual/batch conversion workflows.

## Output paths

Fresh installs use the original Soggfy-style track template:

    Path Template={artist_name}\{album_name}{multi_disc_path}\{track_num}. {track_name}.{ext}

Podcast episodes use the separate Podcast template.

Available tokens include artist_name, all_artist_names, album_name, track_name, track_num, track_num_2, disc_num, release_year, release_date, multi_disc_path, multi_disc_paren and ext.

Cached metadata is used for album artist, contributing artists, disc information and release date/year when it passes the same identity checks used for tagging.

### Slash-name fix / upstream issue #150

Downloaded-track lookup and file creation now use the same canonical path escaping rules.

That specifically fixes the old Skip Downloaded Tracks bug with artists such as AC/DC and Gary Numan / Tubeway Army.

The regression suite tests both cases directly, including converted MP3 lookup, so saving and downloaded-file detection cannot silently drift apart again.

### Existing / legacy Soggfy libraries

Skip Downloaded Tracks checks the configured path template first, but accepts any supported existing audio extension rather than only the currently selected output format. It also has a conservative fallback for old flat libraries named `Artist - Track.ext` or `All Artists - Track.ext`. The fallback requires an exact artist/title filename match; title-only files are not guessed. If multiple files match the same track, the UI reports a warning instead of treating the result as safely downloaded.

## Fallback UI

The Floggfy-style native To Disk menu is suppressed while Classic UI is enabled.

For troubleshooting only, set Classic UI=0 and Native Menu=1 in the Soggfy section of SpotifyHistory.ini and restart Spotify.

If the injected UI is unavailable after a Spotify update, capture can still be enabled with Downloads=1 in SpotifyHistory.ini.

## Diagnostics

`Soggfy.log` is written in the configured save root when Log is enabled. Normal logging includes per-track start/failure/completion information, effective playback speed, Ogg BOS/EOS and rejection context, stream/listen association and publication/post-processing stages. Spotify 1.3.3.264 speed troubleshooting uses `speed_session_*` records showing discovered SessionTrackPlayer objects, underlying-player presence, native speed and active speed-automation entries. Memory-scan results are diagnostic-only; Soggfy calls speed methods only on a SessionTrackPlayer pointer that Spotify itself has exposed through the hooked setter/getter. `speed_session_hook` records show those genuine calls, while `speed_session_raw` records describe scan-only candidates. Older `speed_scan_*` ContextPlayer diagnostics remain only for historical troubleshooting.

Enable **Debug log** in Soggfy settings for the high-volume timeline trace: each media-session sample plus replayed Ogg-page details. This is intended for short troubleshooting runs because the log is capped and rotates by truncation when it reaches its size limit.

## Safety / implementation notes

The current UI does not restore the old localhost WebSocket server or old x86 decoder hook.

Instead:

- UI controls communicate through the in-process CEF bridge.
- Native capture remains bounded and memory-only until a complete listen is validated.
- FFmpeg starts only after native publication.
- Canvas downloads are bounded and published from a temporary file only after completion.
- Telemetry blocking is limited to the old Soggfy ad/telemetry receiver prefixes; metadata, audio CDN and client-update traffic are deliberately left alone.
- Playback speed uses separately validated Spotify.dll backends. Spotify 1.3.3.264 validates its SessionTrackPlayer vtable, setter, getter and constructor assignment and fails closed if they differ.

## Build and test

Release builds use Windows/MSYS2 MinGW64 and include exact compiler/linker details in BUILDINFO.txt. See BUILDING.md.

A Debian/Ubuntu/WSL reference build is also supported:

    sudo apt install build-essential python3 nodejs mingw-w64
    bash test-native.sh
    bash build-native.sh
    python3 package-release.py

CI covers native capture, FLAC/Ogg tagging, cache metadata, path templates, issue #150 path matching, playback-speed target discovery, telemetry URL filtering, JavaScript syntax, Windows x64 compilation, PE metadata and release hashes.

## Project history

The previous 2024 x86 Soggfy source is preserved under legacy/ for reference.

The active x64 base is synced selectively through Mainkill1/Floggfy v1.1.0. See UPSTREAMS.md for the exact source revision and community work reviewed while building this fork.

## Credits

- Rafiuth/Soggfy — original project, UI model and CC0 source
- Mainkill1/Floggfy — modern x64 capture, FLAC, dynamic discovery, cache metadata and validation base
- SuperSecretEyeball/Soggfy-Fixed and MacKinnon7/Soggfy-Fixed — community fixes and build ideas
- AndyBogle1, coleaderme and MrSykenro — installer/TLS/SpotX fixes reviewed while modernizing the fork
- MinHook, libogg and CEF — see bundled third-party notices

The modern Floggfy-derived code remains under the MIT license in LICENSE. Original Soggfy CC0 terms are retained at native/vendor/soggfy/LICENSE.txt and under legacy/.

Not affiliated with Spotify.
