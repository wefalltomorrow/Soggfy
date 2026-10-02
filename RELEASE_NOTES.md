# Soggfy v3.0.0-rc.9

RC9 fixes Ogg captures that could fail with `stream discarded: page gap or integrity failure` even during normal uninterrupted playback.

## Fixed

- Spotify may internally revisit an Ogg page or the Vorbis identification/BOS page while buffering or reusing its decoder.
- Soggfy now recognizes already-consumed pages from the same Ogg logical stream as replays and ignores them.
- Replayed pages are never appended to the output twice.
- A replayed BOS page no longer resets and destroys an otherwise valid in-progress capture.
- Real forward gaps, invalid CRCs, serial changes, bad granule progression, skipped/seeked playback and incomplete listens still fail closed.

RC9 also contains the RC8 legacy-library detection and RC7 settings-button fix.
