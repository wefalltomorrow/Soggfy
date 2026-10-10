# Soggfy v3.0.0-rc.54

RC54 restores two original Soggfy behaviours: completed/failed track indicators after restarting Spotify, and normal 1× music playback whenever downloading is disabled—even with a stored 50× playback-speed preference.

## Playlist indicators after restart

The RC53 Classic UI shows the correct icon on each playlist row, but previously kept terminal statuses only in a 64-record, two-minute in-memory cache. On restart, it tried to infer green checks from the output directory and current filename template; red crosses had no durable history.

RC54 stores only **DONE** and **ERROR** outcomes in `SoggfyTrackStatus.tsv` under the configured download directory. The append-only UTF-8 journal has hex-escaped fields, checksums and a 16 MiB compaction threshold; corrupt or partial lines are ignored during loading. Statuses are keyed by actual title, artist and album rather than the UI's temporary synthetic row IDs. An existing **DONE** path is verified to exist before restoring its green check. The normal filesystem scan also remains, and takes priority over a recorded error when an older matching MP3 or other audio file exists. Failed downloads preserve a red cross and failure tooltip across restarts; incomplete blue **IN_PROGRESS** and converting states never persist.

For older tracks that predate RC54, the filesystem scan now understands both the flat legacy `Artist - Track.mp3` convention and the original Soggfy `Artist/Album/Track number. Track.ext` template independently of the currently configured output layout. Previously displayed errors that never entered the new journal cannot be reconstructed retroactively.

## Original playback mode behaviour

The original Rafiuth/Soggfy `StateManager::GetPlaySpeed()` returns 1.0 if downloading is disabled, otherwise the saved playback speed.

RC54 applies this same policy in the x64 PCM decoder and the history/media clock. With playback-speed preference set to 50×, disabling downloads now means **effective speed 1×**, and re-enabling downloads restores **50×** without modifying the stored slider value. Toggling downloads also attempts the original-style restart of the current playback (preserving position on entering listening mode), to avoid a mismatched accelerated timeline.

The existing 50× PCM decoder calculation, RC53 passive debug probes, RC52 stall watchdog, RC51 UI positioning, capture, MP3 export and SpotX integration are otherwise unchanged.

## Validation

New regression coverage tests playback-speed gating through enable/disable/enable, corrupt/truncated journal records, Unicode/error text and terminal-only persistence. Windows compilation and actual Spotify restart behaviour still require in-app validation.
