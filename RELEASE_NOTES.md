# Soggfy v3.0.0-rc.32

RC32 fixes the remaining accelerated transport problems found in RC31.

## Scrubber fix

The RC31 virtual clock itself was advancing, but the visible Spotify scrubber was not. Current Spotify stores its progress transform as a CSS transform value such as `translateX(-50%)`. RC31 incorrectly wrote a bare numeric percentage into `--progress-bar-transform`, so Chromium ignored it.

RC32 writes the correct `translateX(-100%..0%)` value to both relevant progress elements while accelerated playback is active. The elapsed-time text continues to use Soggfy's virtual accelerated clock, and no repeated Spotify seeks are used.

A new diagnostic line reports the detected UI elements and duration for each accelerated playback:

`FLOGGFY_STATUS:accelerated ui root=1 bar=1 time=1 dur=...`

## Automatic next-track fix

The RC31 log proved the native accelerated completion event reached the Classic UI, but there was no matching "advanced" record. The RC31 guard required Spotify's playbackId to remain identical between the completion callback and the delayed skip. Spotify can rotate that ID without changing the current track.

RC32 now guards against the stable track URI instead. It first calls Spotify's `skipToNext()`; if the track URI is still unchanged after a short wait, it falls back to clicking the current Spotify Next button. Each stage is logged so any remaining failure is immediately visible.

RC31's startup-race hardening, RC29's accelerated capture/publication logic and RC30's per-track status indicators are otherwise unchanged.
