# Soggfy v3.0.0-rc.62 — pause-safe recovery and diagnostics

RC62 is a targeted pause/recovery safety update built on the RC61 baseline. This does **not** claim to eliminate all decoder stalls.

## Changes

- Logs `pause_observed` and `resume_observed` only when Spotify reports a state transition, and includes the source of the state (PlayerAPI flags or transport-button fallback). It never guesses whether a pause was manual or automatic.
- Before a stalled-track retry or fallback skip, rechecks the current track identity **and** playback mode. A track reported as paused or unknown is left alone.
- If an asynchronous failed retry leaves Spotify paused, the fallback skip is safely aborted.
- Clears stale retry counts on pause and restarts the full grace period on resume.
- Preserves RC61's optional artwork tagging fix and RC60's 30x maximum, original status/checkmarks, skip-downloaded queue and MP3 320K output.

## Test and limits

The native/JavaScript regression suite now includes a pause arriving between watchdog samples and recovery, a failed reset leaving Spotify paused, contradictory player flags, and proper timing after resume.

The RC61 log's **Simon Posford – Wish You Weren't Here** had `playing=0` after progressing to ~45 seconds. It is **not known** whether the pause was manual, remote or automatic; this version adds the missing diagnostic observations without overriding the pause.

A track can still stop decoding while Spotify reports `playing=1` at 30x (for example, **Echosmith – Cool Kids** in an older RC59 log). Do not interpret this RC62 update as a decoder/fetch fix; that remains separate.

## Install

Close Spotify fully, extract the Windows x64 ZIP and install over RC61. Enable Debug logging for a new stall reproduction and provide the resulting Soggfy.log.
