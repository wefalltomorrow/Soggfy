# Soggfy v3.0.0-rc.69 — conversion, queue and export reliability (test build)

RC69 includes RC63–RC68 and the broader maintenance audit. The target remains Spotify **1.3.1.234.g59d6bf59, Windows x64**. Live validation is still required; historical random pauses and decoder stalls are not established as fixed.

## RC69 changes

Native and manual encoding now fail on decoding errors rather than accepting a playable partial output. The built-in AAC fallback also recognizes the three unsuffixed M4A presets; custom commands remain explicit. MP3 stays at 320 kbps.

Manual conversion resolves relative paths and validates watch mode before changing files. Watch mode reconciles the directory after each pass, retries busy files with bounded backoff, and continues past individual failures. Files arriving during conversion are picked up on the next pass.

Queue reads and removals have bounded waits. Run ownership prevents old work from unlocking a replacement player's operation. Unsettled removals reserve their URI/UID so a retry cannot submit the same entry twice.

A separate 4,096-entry match-result cache avoids recompiling expressions when a working set exceeds the 512-expression cache. Repeating 640 unchanged queries produces 640 cache hits with zero new compilations in the regression fixture.

M3U export follows capped API pages and checks every track in batches of 128. Duplicate playlist entries retain their order. Incomplete responses, failed calls and limits produce an error instead of silently truncated exports. Saving stages a complete file before replacing an existing playlist. Visible-row fallback is identified in the message.

Installation checks the DLL before stopping Spotify or starting SpotX. Packaging preserves older archives and replaces a same-version ZIP only after the new archive finishes successfully. Windows publication/settings/logging/metadata/quality checks now run in both CI and release workflows; regression tests cover the new fixes.

Automated checks are not live Spotify validation. Watch mode performs recurring full scans and manual FFmpeg calls have no deadline. Concurrent external replacement of a source around optional deletion is not fully protected. Historical pauses/decoder stalls and live API variants remain unverified; preserve RC63–RC68 and do not install, merge or release before the separate live gate.

## RC68 changes

Large-library status lookup filters impossible filenames before running the existing matching expressions. An unchanged directory rescan preserves cached matches, while deleted/locked files and unreadable matching candidates are rechecked. A local assertion-enabled diagnostic benchmark of 32,768 files and 128 misses fell from about 46 seconds in RC67 to 0.55–0.81 seconds, with zero regex checks and a sub-millisecond warm lookup. This measures synthetic misses on local storage, not live Spotify or every naming layout; title-free custom templates retain full matching.

Directory traversal is bounded by 100,000 files, 10,000 scheduled directories and depth 12. Incomplete scans, unavailable roots and skipped reparse directories report uncertain status rather than downloaded/absent conclusions. New requests cancel obsolete native work; a seven-second cooperative deadline returns only completed queries. Blocking OS calls or a single regex call cannot be interrupted by that deadline. Positive status still means filename association and a readable nonempty file, not full audio verification.

Visible rows are queried in sequential pages of 128, deduplicated by URI and prioritizing the viewport. Navigation, a new refresh or configuration invalidation stops obsolete pages. Pending queue events are tied to their player, settings epoch, skip policy and age. Events received while skipping is disabled or no player exists are discarded; replacement players use a fresh snapshot.

The manual PostProcess script now requests MP3 320 kbps, validates nonempty output by full decoding, uses a unique temporary name and publishes without overwriting a raced file or directory. Failure preserves the original. Equal/higher-quality retained native files can retry conversion when a capture reaches the existing-file branch. Retry requires recognized Ogg/FLAC headers and full source decoding, preserves the existing original even when keep-original is off, and never overwrites a destination. This is not a background retry: Skip Downloaded can prevent that capture attempt; disable it for a replay or use the manual converter.

Windows maintenance checks share one runner, require real FFmpeg codec tests in CI, and cover failed/recovered FLAC and Ogg conversion, corrupt source/output, manual conversion conflicts and original retention. Checked procedure-pointer loading replaces incompatible casts. Classic UI, 1–30x behavior, Downloads-off effective 1x, cached PlayerAPI/per-entry UIDs, Unicode/legacy/podcast layouts, strict capture validation and file-based status remain intact.

## RC67 overview

RC67 addresses the RC66 maintenance review and includes RC63–RC66. PR #64 remains a test candidate pending live validation on Spotify **1.3.1.234.g59d6bf59, Windows x64**. It does **not** claim to eliminate decoder stalls, random pauses or historical missing-stream failures.

## RC67 changes

An existing conversion destination previously allowed the native capture to be deleted even when that destination was a directory or empty file. RC67 rejects invalid/unreadable destinations, checks actual audio decoding, and retains the native original for every pre-existing destination. A newly converted file must decode successfully with nonempty audio before it can be published and the original removed. No conflicting destination is overwritten. Full decoding adds conversion work; it is a preservation safeguard, not an encoder speed claim.

FFmpeg now runs in a supervised Windows process job. The whole conversion/validation transaction has a size-based deadline (five minutes plus one second per 64 KiB, capped at six hours), and capture cancellation stops the converter and its child processes. Failure keeps the native audio and reports the reason. The pending-save queue is still bounded; sustained throughput overload can still fill it.

Status replies carry the UI session and request ID, preventing a timed-out reply from completing a later request. Identical work shares one promise; queued requests and status caches have limits. Publication and matching-setting changes invalidate results, queue results expire after three seconds, and failed removals require a fresh lookup. A newer queue event or changed player prevents an older lookup from removing obsolete entries.

Downloaded results are reconciled against readable, nonempty files under the applicable configuration. Ambiguity warnings, Unicode/multiple artists, legacy naming and podcast/custom extensions remain supported. The directory index refreshes on publication/root changes and after ten seconds; externally added files can take that interval plus the next UI/queue poll to appear. Queue decisions can be up to three seconds old. Status remains filename/file based, not a full audio integrity scan or persistent database.

Compiled expressions (up to 512) and matches are reused within each directory-index generation. A dedicated status worker keeps scans out of the metadata worker. The Windows benchmark checks 128 misses against 2,048 files: a cold lookup performs 786,432 regex checks; the repeated lookup performs zero and recompiles nothing. This is synthetic repeated-work evidence, not a guarantee for a different library or storage device.

New automated cases cover late/empty replies, bounded requests/maps, invalidation, queue/player changes, malformed headers, deleted/locked/ambiguous files, conversion conflicts, failed/empty/corrupt output, publication races, timeout/cancellation and child-process termination. A local real FFmpeg smoke test covers MP3 320 kbps encoding, full decoding, corrupt existing audio and header-only audio. Unknown or unreadable existing source files remain protected from replacement.

## RC66 changes

RC65 could wait indefinitely for a new title after the same song restarted. RC66 observes a natural end and backward timeline reset, then requires one second of continuous playback before confirming a same-title repeat. Each listen has a distinct generation. A delayed title can relabel the new instance within two seconds of the original reset, preserving its start and emitting no duplicate completion. An earlier title can resolve the transition before repeat confirmation.

The accelerated clock distinguishes newer raw backward timeline movement from ordinary SMTC timestamp refreshes. Actual resets reach the listen tracker; refreshed extrapolated positions retain monotonic progress. Seeks, missing coverage, invalid observations and interrupted confirmation fail closed. Progress across a speed change is judged using the preceding speed. Completed-listen times and capture deadlines refer to the original transition, not delayed confirmation. Retained captures also require the original generation.

The same minimal six-second repeat fixture fails on RC65 and passes on RC66. Portable tests cover combined clock/listen behavior at 1x, 2x, 10x and 30x, delayed titles and durations, multiple repeats, seeks, pauses, stale timestamps, invalidation, rate changes and retained-capture ownership. Live Spotify behavior remains unverified. Raw resets must be distinguishable; very short or sparsely reported accelerated repeats can remain unrecognized. These timeline heuristics cannot distinguish every manual seek near a natural end from a repeat. No stable track-ID source is added.

## RC65 changes

A track transition previously deleted an active capture even when its completed listen was waiting for a final Ogg EOS or FLAC frame. That candidate now survives until the listen's existing ten-second deadline. It remains subject to the shared memory budget, strict codec completeness and the unchanged -20/+3-second start window.

Retained audio belongs to the original completed listen. Later transitions cannot claim it or extend its deadline. Retained buffers are removed when their listen is queued, rejected or expires. If a second retained candidate is incomplete, selection waits; multiple complete matches or an unresolved competitor at the deadline fail closed. This can reject a complete candidate when association remains uncertain.

Portable tests exercise the shared production policy, real Ogg validation, FLAC frame coverage and a natural 30x listen transition. They do not exercise live Spotify callbacks or the full Windows worker. RC65 selectively adapts Floggfy commit f80dfdd. RC66 adds repeat/delayed-identity handling separately; network changes remain excluded.

## RC64 changes

The capture worker previously called Windows media APIs and optional artwork retrieval synchronously. A slow call could prevent it from draining captured audio during that wait. A dedicated observer now performs those reads, while capture polls bounded snapshots from memory. This is a concrete blocking path in the source; it has not been established as the cause of the historical Spotify stalls.

Each snapshot carries its original observation time, sequence and settings revision. Changes to speed, Downloads/capture epoch or settings reject outstanding results, including settings that change away and back before Windows responds. The accelerated position clock is retained, and an obsolete result cannot advance that clock.

Samples older than three seconds, invalid samples and skipped observations fail closed. Missing observations still invalidate listen coverage; keeping capture responsive does not waive complete-listen or Ogg/FLAC integrity checks. Optional artwork can delay the observer, but no longer the capture worker. If that delay exceeds coverage limits the listen remains ineligible.

The Windows regression uses a deliberately blocked provider to check continued polling/queue draining, stale or failed reads, settings changes and stop/restart behavior. Portable tests check 30x timing, pause/resume and variable delivery delays. These are automated boundary tests, not a live Spotify reproduction.

This selectively adapts Floggfy's observer boundary. Network defaults, late-capture retention, repeat handling and normal-speed timeline rewrites are not part of RC64.

## Included RC63 changes

RC62 checked playback state before starting a retry, but its real reset helper then waited for Spotify's queue lookup and insertion. A pause during those waits could still be followed by a skip. RC63 rechecks state after each queue operation and cancels pending recovery if playback is paused or unknown, Downloads is disabled, accelerated recovery is no longer enabled, the track changes, or the PlayerAPI instance changes.

The watchdog continues observing state transitions while a queue operation is pending. An observed pause/resume invalidates that operation, clears the retry budget and restarts the grace period. Clearing the monitor retains its operation lock until outstanding work settles. Pause/resume diagnostics record evidence without guessing who caused the pause.

The 30x maximum, Downloads-off effective 1x behavior, Classic interface, file-based status, cached PlayerAPI queue skipping, optional artwork tagging and strict capture-integrity validation are preserved.

The Windows DLL build explicitly links MinGW's supplied floating-point environment object to resolve a missing runtime symbol with the updated CI toolchain. The existing msvcrt target is preserved.
The optional launcher now statically links its MinGW runtime dependencies as well. Both binaries are checked for unwanted external MinGW DLL dependencies during CI and release packaging.

## Validation and limits

A new integration test loads both the real reset helper and recovery monitor, delaying queue promises to reproduce the RC62 failure. It covers pause/resume, setting changes, track/player changes, rejected operations, single-operation locking and normal successful recovery.

Spotify can already have accepted a queue insertion before cancellation is observed; a queued copy may remain even though RC63 cancels the pending skip. This is not an atomic queue transaction. A pause/resume entirely between observations cannot be detected. Live validation on Spotify 1.3.1.234.g59d6bf59 is still required.

Historical evidence remains limited: Simon Posford - Wish You Weren't Here reported playing=0 around 45.352 seconds, with no established cause. Echosmith - Cool Kids stopped decoder progress at 30x in the RC59 log. These are separate from the reproduced queue-operation race.

## Live test

1. Close Spotify fully, extract the RC67 Windows x64 ZIP and install using Scripts/Install.ps1. Confirm the package VERSION and version.dll file properties identify 3.0.0-rc.67. Enable Debug logging.
2. With Downloads enabled at 30x, check normal complete MP3 output and skipping of existing Unicode/multi-artist files.
3. Pause during a stalled track or recovery attempt. Leave it paused past the recovery deadline; it should stay paused. Resume and confirm there is a fresh grace period before any further recovery.
4. Disable Downloads during a stall/retry and check that playback returns to effective 1x without losing the selected speed. Re-enable Downloads to restore it.
5. Retest Cool Kids, Wish You Weren't Here, Murder-Death-Kill and Intoxicated (Radio Edit). Send the resulting Soggfy.log and note whether pauses were deliberate, whether any duplicate queued track remained, and whether the saved MP3s play through.
6. Compare a short 1x run and several natural transitions at 30x. Include tracks with artwork; change speed and toggle Downloads while playback is active. Watch for repeated `media snapshot unavailable or stale` entries, lost start detection or premature green checks. Existing valid files should still be recognized from disk.
7. Include natural transitions followed by a quick manual Next, several similar-duration tracks, and FLAC if available. Check that a late complete file receives the correct track's metadata and that skipped tracks are not saved. Compare RC64 separately; preserve both logs. Association failures now include `retained_active`, the number of unfinished retained candidates.

8. Run Repeat One for at least three plays at 1x and 30x, with skip-downloaded disabled for this test. Include a normal transition where the displayed title lags, then pause/resume and manual Next. Check `listen_instance` generations, one completion per full play, correct titles and playable MP3 320kbps output. Reusing an existing valid file is expected; a repeat should not create a duplicate filename. Preserve the RC65 package and log for comparison.

Relevant diagnostics include listen_instance (phase 0 tracking, 1 awaiting identity, 2 provisional repeat), pause_observed, resume_observed, recovery_aborted_state_changed, retry_1/retry_2, skip_stalled, decoder_probe, listen_complete, publish_done and failed. A publish_done entry alone does not prove a newly converted MP3, since existing files can follow a reuse path.
