# Soggfy v3.0.0-rc.61 — optional cover artwork fix

RC61 builds on the 30x RC60 baseline. An earlier CI test archive contained a package still named RC60 because the version file was not bumped; this official release is correctly named **3.0.0-rc.61**.

## Fixed

- Vorbis/Ogg tagging no longer rejects an otherwise complete audio capture solely because cover artwork is missing, unsupported or larger than the permitted 4 MiB limit.
- Valid new artwork is embedded normally. If no usable new image exists, any previously embedded source artwork is preserved.
- Other text metadata remains available. Ogg integrity, sequence, CRC and end-of-stream checks remain strict; invalid or incomplete audio is still rejected.

## Preserved

- Accelerated playback limited to **1x–30x** for Spotify Windows x64 **1.3.1.234**; normal 1x listening when Downloads is disabled.
- Original Soggfy-style UI, green checkmarks, automatic queue skipping for downloaded songs, multi-artist filename recognition and MP3 320K conversion.
- The existing SpotX workflow for ads and Spotify update blocking.

## Verification and known limitation

GitHub Actions runs the native regression suite, including new coverless/invalid/oversized artwork tests, and creates one verified Windows x64 release ZIP.

Live re-testing is recommended for **7L – Murder-Death-Kill** and **Martin Solveig – Intoxicated (Radio Edit)**, which previously reported `invalid Vorbis comments or artwork`.

The occasional stalled playback/decoder issue at 30x (e.g., Echosmith – Cool Kids) is **not fixed by this update** and is being investigated separately.

## Install

Close Spotify fully, download the Windows x64 ZIP, extract it and run `Scripts\Install.ps1`. The application and package both identify as **3.0.0-rc.61**.
