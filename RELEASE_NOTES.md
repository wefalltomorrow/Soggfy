# Soggfy v3.0.0-rc.63 — asynchronous recovery safety (test build)

RC63 extends the RC62 pause-safety work on the RC61 baseline. It fixes a reproduced recovery race; it does **not** claim to eliminate decoder stalls or missing-stream failures. PR #64 remains a test candidate pending live Spotify validation.

## Changes

RC62 checked playback state before starting a retry, but its real reset helper then waited for Spotify's queue lookup and insertion. A pause during those waits could still be followed by a skip. RC63 rechecks state after each queue operation and cancels pending recovery if playback is paused or unknown, Downloads is disabled, accelerated recovery is no longer enabled, the track changes, or the PlayerAPI instance changes.

The watchdog continues observing state transitions while a queue operation is pending. An observed pause/resume invalidates that operation, clears the retry budget and restarts the grace period. Clearing the monitor retains its operation lock until outstanding work settles. Pause/resume diagnostics record evidence without guessing who caused the pause.

The 30x maximum, Downloads-off effective 1x behavior, Classic interface, file-based status, cached PlayerAPI queue skipping, optional artwork tagging and strict capture-integrity validation are preserved.

## Validation and limits

A new integration test loads both the real reset helper and recovery monitor, delaying queue promises to reproduce the RC62 failure. It covers pause/resume, setting changes, track/player changes, rejected operations, single-operation locking and normal successful recovery.

Spotify can already have accepted a queue insertion before cancellation is observed; a queued copy may remain even though RC63 cancels the pending skip. This is not an atomic queue transaction. A pause/resume entirely between observations cannot be detected. Live validation on Spotify 1.3.1.234.g59d6bf59 is still required.

Historical evidence remains limited: Simon Posford - Wish You Weren't Here reported playing=0 around 45.352 seconds, with no established cause. Echosmith - Cool Kids stopped decoder progress at 30x in the RC59 log. These are separate from the reproduced queue-operation race.

## Live test

1. Close Spotify fully, extract the RC63 Windows x64 ZIP and install using Scripts/Install.ps1. Enable Debug logging and confirm the startup log identifies 3.0.0-rc.63.
2. With Downloads enabled at 30x, check normal complete MP3 output and skipping of existing Unicode/multi-artist files.
3. Pause during a stalled track or recovery attempt. Leave it paused past the recovery deadline; it should stay paused. Resume and confirm there is a fresh grace period before any further recovery.
4. Disable Downloads during a stall/retry and check that playback returns to effective 1x without losing the selected speed. Re-enable Downloads to restore it.
5. Retest Cool Kids, Wish You Weren't Here, Murder-Death-Kill and Intoxicated (Radio Edit). Send the resulting Soggfy.log and note whether pauses were deliberate, whether any duplicate queued track remained, and whether the saved MP3s play through.

Relevant diagnostics include pause_observed, resume_observed, recovery_aborted_state_changed, retry_1/retry_2, skip_stalled, decoder_probe, listen_complete, publish_done and publish_failed. A publish_done entry alone does not prove a newly converted MP3, since existing files can follow a reuse path.
