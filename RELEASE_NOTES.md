# Soggfy v3.0.0-rc.55 — original file-based status indicators

RC55 removes the experimental persistence database introduced in RC54. The user prefers original Soggfy's behaviour, so downloaded status again comes directly from the saved audio files, without a separate per-track journal.

## Status indicators (restored original approach)

- Green checkmarks are calculated from **existing audio files** in the configured download directory. The folder is scanned recursively and the result is matched against each real playlist row's artist, album and title, including the configured path template, original Soggfy's `Artist/Album/Track number. Track.ext` layout, and old flat `Artist - Track.mp3` files.
- Blue downloading and red failure indicators are **session-only**, derived from Soggfy's active playback/download states. As in original Soggfy, error history is not restored from a separate database after restart. No red cross is shown for a merely missing file.
- RC55 **does not create, read or update** `SoggfyTrackStatus.tsv`. An old TSV created by RC54 is **left on disk untouched**; RC55 ignores it. Do not delete any user's files automatically.
- An audio file must match the filename/path conventions to yield a green check; custom renames or files stored outside the configured root may not match. This is intentional original-style behaviour.

The RC54-only persistence source files and journal test have been removed from the codebase and the Windows build. A regression test verifies recognition of the original nested file layout even when the selected save template has changed.

## Playback speed toggling

RC54's original Soggfy listening mode remains: with selected speed 50×, turning downloads off uses actual **1×**, and re-enabling restores **50×**. The stored speed preference is not modified.

## Unchanged

RC51 playlist indicator placement, RC52 stall-recovery watchdog, RC53 read-only decoder instrumentation, audio capture, MP3 conversion and playback backend are untouched. Real Spotify restart/toggle verification remains necessary.
