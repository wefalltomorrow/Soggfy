# Native identity compatibility audit — November 8, 2026

The RC74 executable-only check was incorrect. Decoder discovery belongs in
Spotify.dll. Historical RC74 packages and reports are preserved unchanged; this
record supersedes their incompatibility conclusion.

Pinned DLL: version 1.3.1.234, 42,852,224 bytes, SHA256
`210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`.
Upstream: Mainkill1/Floggfy v1.3-RC6, commit
`7b825e3b942596c3e912fbbf7a0487abd7578e47`.

Method: map PE sections into SizeOfImage, apply DIR64 relocations, invoke
unmodified upstream discovery against the mapped bytes. No Spotify code was
executed and no live hooks were installed. The local Zig probe is diagnostic
only; shipped binaries still require MinGW CI.

| Static result | Evidence |
| --- | --- |
| Capture discovery | Found all six requested functions |
| Dispatcher | Unique anchor, RVA 0xe5c8d8 |
| Playback pump | RVA 0xe5d518, unwind end 0xe5dc07 |
| Stereo pump anchor | Unique at RVA 0xe5d9a4 |
| Decoder member | Pump loads member +0x70 before dispatcher call |
| Ogg capture ownership | Verified delta -0x88 |
| FLAC capture ownership | Verified delta 0 |
| AAC ownership | Not verified |

These results establish discovery compatibility, not runtime identity correctness.
The upstream playback-ID pointer path {0x78, 0x1e8, 0x9b8}, its encoding and
lifetime are not verified. The pump's +0x78 access uses a helper, requiring
further analysis. Pointer reuse, preloading, staged transitions and interstitial
attribution need dedicated fixtures and pinned-client acceptance. No stall fix
or comprehensive ad isolation is claimed.
