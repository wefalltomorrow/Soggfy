# Soggfy v3.0.0-rc.6

RC6 is the full Classic Soggfy UI/behaviour port.

The Spotify-facing experience now follows old Soggfy instead of Floggfy, while the backend remains the modern RC5-derived Windows x64 engine.

## Restored old Soggfy behaviour

- Top-bar Downloads toggle and Soggfy settings button.
- Old-style settings layout and controls.
- Per-track downloading, converting, completed, failed, warning and ignored status icons.
- Open Folder from completed-track status.
- Skip Downloaded Tracks and Skip Ignored Tracks.
- Resource-aware Ignore / Unignore.
- Generate M3U from downloaded files.
- Playback-speed control from 1x to 50x.
- MP3 320/256/192, M4A/AAC, Opus and Custom FFmpeg output presets.
- Embed / save cover art.
- Embed / save lyrics, using .lrc for synchronized lyrics and .txt for plain lyrics.
- Save Canvas.
- Track, Podcast and Canvas path templates.
- Invalid-character replacement modes.
- Block telemetry.
- Move Add to Queue to the top.
- RC5 current Song / Quality / Format / Sample-rate information, restyled inside Classic Soggfy settings.

## Modern backend improvements retained

Native compressed Ogg/FLAC capture, lossless FLAC preservation, complete-listen validation, seek/skip/truncation rejection, atomic publication, quality-aware existing-file replacement, dynamic x64 Spotify target discovery, identity-checked cached metadata, RC5 playback-quality safeguards, bounded queues/memory, safe FFmpeg post-processing, installer backup/restore, diagnostics, deterministic packaging and SHA-256 manifests all remain. Downloaded-file status now also uses a bounded 10-second shared index cache, while freshly completed tracks are surfaced immediately from live state.

## Issue #150

Upstream issue #150 is fixed.

Saving and Skip Downloaded Tracks now share the same canonical filename/path escaping. Regression tests cover AC/DC and Gary Numan / Tubeway Army, plus converted MP3 lookup.

## Compatibility note

The Floggfy RC5 capture base was live-tested against Spotify 1.3.3.264. The new Classic UI and modern playback-speed hook are designed to fail closed if Spotify changes the expected CEF/player layout; future Spotify updates still require ordinary live validation.
