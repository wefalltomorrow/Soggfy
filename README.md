# Soggfy

A maintained Windows x64 continuation of Soggfy, using the modern Floggfy capture engine as its base.

This branch combines the parts that still make sense from the original Soggfy ecosystem with the safer current-client work in Floggfy. It does not use the old x86 offsets or the pinned 2024 Spotify installer.

## What it does

- Saves fully played tracks from Spotify's own compressed Ogg or FLAC input.
- Keeps native FLAC lossless. The capture core does not transcode.
- Embeds artwork and metadata, including extra locally cached metadata when available.
- Refuses incomplete/skipped/broken captures instead of publishing partial files.
- Skips equal or better existing files and atomically replaces lower-quality copies.
- Uses dynamic x64 hook discovery instead of fixed Spotify offsets.
- Keeps the capture, menu and metadata integrations separated so one optional integration can fail without blindly hooking another.
- Uses the original Soggfy-style Spotify integration by default: a top-bar Downloads toggle and a Soggfy settings button/modal.
- Keeps Floggfy's native **To Disk** menu only as an optional fallback when Classic UI is disabled.
- Supports the original Soggfy idea of configurable output paths.

## Install

Live compatibility inherited from Floggfy v1.1.0-rc.5 was tested end-to-end with Windows x64 Spotify 1.3.3.264. Its dynamic audio/connectivity resolver was also checked against signed Spotify DLLs from 1.3.0.277, 1.2.94.583 and 1.2.92.148. Older builds were not all run end-to-end. Microsoft Store installs remain unvalidated.

1. Quit Spotify.
2. Download the Windows x64 ZIP from Releases or the CI artifact.
3. Extract it.
4. Run `Scripts\Install.ps1`, or manually copy `version.dll` beside `Spotify.exe` (normally `%APPDATA%\Spotify`).
5. Start Spotify.
6. Use the **Soggfy download button** in Spotify's top bar to enable Downloads. The sliders button beside it opens **Soggfy settings**.
7. Play a track from start to finish without seeking or skipping.

The installer backs up a pre-existing `version.dll` instead of silently overwriting it. `Scripts\Uninstall.ps1` restores that backup.

## Output paths

Leave `Path Template=` empty in `SpotifyHistory.ini` for the smart Floggfy-style media layout.

For a Soggfy-style custom layout, for example:

```ini
Path Template={artist_name}\{release_year} - {album_name}{multi_disc_path}\{track_num_2} - {track_name}.{ext}
```

Available tokens:

- `{artist_name}`
- `{all_artist_names}`
- `{album_name}`
- `{track_name}`
- `{track_num}`
- `{track_num_2}`
- `{disc_num}`
- `{release_year}`
- `{release_date}`
- `{multi_disc_path}`
- `{multi_disc_paren}`
- `{ext}`

Cached metadata is used for album artist, contributing artists, disc information and release year when it passes the same identity checks used for tagging.

`Normalize Artist Separators=1` only changes rendered paths. It turns a separator such as `Artist A / Artist B` into `Artist A, Artist B`, but deliberately leaves `AC/DC` alone. Embedded tags are not rewritten by this option.

## Optional conversion

The native capture is always kept in its original Ogg/FLAC form unless you explicitly remove it.

`Scripts\PostProcess.ps1` can use an installed FFmpeg to create MP3, AAC/M4A, Opus or FLAC copies after capture. This is intentionally outside the injected DLL so FFmpeg failures can never affect Spotify or the native capture.

## Settings

`SpotifyHistory.ini` contains the full settings list. With `Classic UI=1` (the default), Spotify gets the old Soggfy-style top-bar controls and settings modal. The modal controls Downloads, native FLAC/Ogg capture, save location, path template, metadata enrichment and logging.

The Floggfy-style native **To Disk** menu is suppressed while Classic UI is enabled. For troubleshooting only, set `Classic UI=0` and `Native Menu=1`, then restart Spotify to use that fallback menu instead.

If the injected UI is missing after a Spotify update, capture can still be enabled with `Downloads=1` directly in the INI.

## Build and test

Release builds use a newer Windows/MSYS2 MinGW64 toolchain and include exact compiler/linker details in `BUILDINFO.txt`. See [BUILDING.md](BUILDING.md).

A Debian/Ubuntu/WSL reference build is still supported:

```bash
sudo apt install build-essential python3 nodejs mingw-w64
bash test-native.sh
bash build-native.sh
python3 package-release.py
```

GitHub Actions runs the native regression suite, builds the release DLL on Windows with a GCC 14+/binutils 2.44+ floor, verifies the Windows version resource and checksums, then publishes the ZIP, raw DLL, checksum manifest and build-toolchain record.

## Project history

The previous 2024 x86 Soggfy source is preserved under `legacy/` for reference. It is not part of the default build.

The current UI is a clean modern reimplementation of that legacy Sprinkles interaction model. It does **not** bring back the old WebSocket control server or x86 hook engine.

The modern base is synced through Mainkill1/Floggfy v1.1.0-rc.5. See [UPSTREAMS.md](UPSTREAMS.md) for the exact source revision and community fixes reviewed for this fork.

## Credits

- Rafiuth/Soggfy — original project and CC0 source
- Mainkill1/Floggfy — modern x64 capture, FLAC, dynamic discovery, cache metadata and validation base
- SuperSecretEyeball/Soggfy-Fixed and MacKinnon7/Soggfy-Fixed — community fixes and build ideas
- AndyBogle1, coleaderme and MrSykenro — upstream installer/TLS/SpotX fixes reviewed while modernizing the installer
- MinHook, libogg and CEF — see bundled third-party notices

The modern Floggfy-derived code remains under the MIT license in [LICENSE](LICENSE). Original Soggfy CC0 terms are retained at [native/vendor/soggfy/LICENSE.txt](native/vendor/soggfy/LICENSE.txt) and under `legacy/`.

Not affiliated with Spotify.
