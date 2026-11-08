# Soggfy v3.0.0-rc.84 - reject shared capture ownership

Completed captures that match two queued listens are now rejected before either
listen can consume them. Previously, processing order could label a timing-only
candidate with the first matching listen. The check covers both formats together,
removes disputed candidates so they cannot be reused after a listen is removed,
and preserves unrelated candidates and existing completed-generation retention.
Diagnostics include the number of shared candidates for each affected listen.

Local regression fixtures pass for Ogg, FLAC, mixed-format conflicts, reversed
queue orders and retained ownership. These cases also run in both sanitizer
jobs. Full RC84 CI/package verification is pending at source commit. This is a
conservative ambiguity rejection, not verified native playback identity; future
listens, preloading and exact decoder lifetime remain separate requirements.

## RC83 background - explicit connectivity workaround

**Upgrade behavior:** the legacy Windows connectivity override is now off unless
SpotifyHistory.ini contains exactly NetworkPatch=1 under [To Disk]. Earlier Soggfy
builds applied it unconditionally. If you rely on that workaround, add the opt-in
before restarting Spotify with this candidate. Missing, duplicate, malformed or
unreadable settings leave the override off; INIs larger than 64 KiB also opt out.
Plain ANSI, BOM-marked UTF-8 and BOM-marked UTF-16LE files are supported.

The setting is startup-only. Classic settings saves preserve external INI edits;
they neither change the active override nor detach existing COM hooks. Disabled
startup skips connectivity import discovery, writes and retries. Enabled startup
retains Soggfy's loader grace and unresolved delay-import safeguards. Telemetry
filtering, SpotX, capture and playback-speed controls remain independent.

Local Windows fixtures exercise disabled discovery using an invalid module,
normal/delayed import mutation, strict INI parsing, restart-only state and settings
persistence. Full CI and independent RC83 package verification are pending at
source commit. No live installation, restart, merge or official release is implied.
The other unfinished milestones remain listed in MILESTONES.md.

## RC82 background - validation helper deployment foundation

The release build compiles the pinned Vorbis/FLAC helper, strips it, verifies
x64 PE imports, and embeds its finalized SHA-256 into the DLL trust entry point.
Packaging requires that binding and includes identical helpers beside both DLL
copies, their checksums, and the Vorbis/FLAC redistribution licenses. CI requires
the release helper to match the exercised offline helper byte-for-byte.

The installer copies the helper under a name containing its build hash before
publishing the DLL. Missing/tampered package helpers and same-name conflicts fail
before process control. Existing older helper builds are retained for rollback.
Uninstall checks helper ownership before mutation, removes the current helper
when no DLL backup exists, and retains it when restoring a backup that may depend
on that helper build. The descriptor is installation metadata, not runtime trust.

Saved-recording rewriting remains disabled. Filesystem security/stream/attribute
preservation, incoming artwork policy and the production coordinator are still
unfinished. Native playback identity and MP3 capture remain separate milestones.
Full RC82 CI and independent artifact verification are pending at source commit;
live installation, merge and official release require separate authorization.

## RC81 background - validated metadata preparation

The cached collector retains validated line-synchronized lyrics, including line
start times supplied by syllable-synchronized cache data. Matching plain text
does not discard that timing. Syllable-level timing is not represented.

Offline saved-recording work now includes owned-file inspection, bounded minimal
text changes, full reference Vorbis/FLAC decoding, isolated Windows validation,
exact decoded coverage agreement, independent output verification and private
staging. Transaction fixtures exercise held identities, rollback, competing
creators and a real settings-generation publication guard. The preparation path
passes twelve real-audio cases locally with byte-identical decoded audio.

These prototypes do not rewrite saved recordings in production. Filesystem
security/streams/attributes, trusted helper deployment and production coordination
remain unfinished; incoming artwork is currently ignored and existing artwork
retained. The validation worker is not included in this package. Native playback
identity, podcast MP3 capture and other open work remain in MILESTONES.md.

RC80 remains independently package-verified. CI 42449594271 passed all four jobs
for the earlier validator/lyrics/transaction work. Subsequent integration and
this exact RC81 package require their own full CI and independent verification.
Live installation, launch, merge and official release remain separate gates.

# Soggfy v3.0.0-rc.80 - bounded native metadata comments (test build)

Ogg/FLAC taggers now use the same bounded comment reader as native recording
identity checks. Their writers reject oversized comment bodies and counts
above 100,000 instead of producing metadata their readers cannot accept.
Tests cover framing, every truncated prefix, stale failure output, raw custom
fields, Unicode, duplicate artists and writer/reader limit agreement.

This supplies a common metadata representation for future saved-recording
refresh. It does not yet update tags in existing files; complete owned-file
inspection, artwork policy and guarded replacement remain unfinished. See
METADATA_REFRESH_DESIGN.md for the implementation requirements.

RC79 passed all four jobs in CI run 42443510851 and independent package
verification, including source/merge tree agreement and preservation of earlier
packages. RC80 has passed local diagnostic tests; CI/package verification is
pending. Live installation, launch, merge and release remain separate gates.

# Soggfy v3.0.0-rc.79 - same-instance metadata refresh (test build)

When a listen completes by pausing at its natural end, its final corroborated
metadata now updates the completed snapshot. Further observations may refresh
a completed candidate while it is still waiting for audio association. The
client URI/playback ID and listen generation must both match; legacy labels,
repeats and transitions cannot refresh another listen. Completion coverage and
duration stay fixed, and already queued publication jobs remain immutable.

Missing optional fields no longer erase previously observed artwork, track,
genre or album artist for the same exact instance. Active snapshots move the
incoming artwork without another copy. The portable Media value type supports
regressions in native, Windows and ASan/UBSan suites.

RC78 passed all four CI jobs and independent MinGW package verification at
commit e7ec7bbbb5c2df8ccf59629e8d5f933607050fdb, run 42442391256. RC77 and the
initial RC78 attempt failed on a test-only C signal name collision; the fixture
was qualified and the corrected run passed. No RC77 package was produced.
RC79 CI/package verification is pending. Native identity/capture milestones
and separately authorized live acceptance, merge and release remain open.

# Soggfy v3.0.0-rc.78 - playback-bound Classic queue work (test build)

Classic queue lookups and retained queue events now belong to the cached player's
observed playback ID and current URI as well as its queue revision, settings and
status epoch. A repeat, track change, missing snapshot or changed skip policy
invalidates a delayed lookup before queue removal. Ordinary timeline updates do
not invalidate it. Per-entry UIDs and outstanding mutation reservations remain.
When playback identity is absent, the legacy queue safeguards still apply; this
is not native decoder binding or permission to skip the currently playing track.

Regression tests reproduce the stale repeat lookup before the fix and cover
track changes, missing state, policy changes, retained event replacement and
same-instance position updates. Added collector tests validate seconds/milliseconds,
pause/buffering, 1x/2x/10x/30x, end clamps and invalid clocks. The existing position
conversion was correct and has not changed.

RC78 requires full CI and independent MinGW package verification. Native playback
ownership, podcast MP3 capture, metadata refresh and live acceptance remain open.

# Soggfy v3.0.0-rc.77 - bounded logging and Classic log levels (test build)

Classic Log detail selects 0 (off), 1 (completions/problems), 2 (activity) or
4 (diagnostics). Fresh installs default to 1. Existing INIs without LogLevel
preserve their Log/DebugLog behavior; explicit invalid levels normalize to 1.

Activity and diagnostics use separate fixed 128-record queues and separate
5 MiB capped files. Producers try the queue lock without waiting, sanitize control
characters and avoid splitting UTF-8. Overflow counts belong to each lane. The
disk worker alternates lanes and rechecks log level before writing. It is still
one worker: a slow disk can delay both lanes. Diagnose includes both file tails.

Local regression coverage checks saturation, eight concurrent producers, Unicode
truncation, settings/legacy setters, actual file separation and lowering verbosity
before drain. RC77 requires the full MinGW and sanitizer CI/package checks.
RC76's two 20,000-iteration ASan/UBSan campaigns passed. Unicode punctuation
corrupted during documentation editing is restored in this candidate.

Native playback ownership, podcast capture and the remaining milestones are open.
Live installation, merge and official release retain their separate gates.

# Soggfy v3.0.0-rc.76 - sanitizer coverage and optional FLAC artwork (test build)

FLAC publication no longer fails when artwork is missing, disabled, oversized or
unsupported. Existing pictures survive when no usable replacement is available;
a valid replacement is inserted once. Compressed audio remains byte-identical.

CI and release builds now require two deterministic Clang ASan/UBSan campaigns
(20,000 mutations each, seeds 4043931417 and 1592594996), plus instrumented
regressions. Valid baseline Ogg, FLAC, ID3 and playback observations are asserted
before truncation/mutation. Models exercise buffers, coverage, decoder scope,
owner conflicts and listening at 1x-30x. This is bounded portable coverage, not
proof of Windows callback races, native playback ownership or live compatibility.

RC75 passed MinGW CI and independent artifact verification. RC76 sanitizer/CI
results remain pending until its workflow finishes. Native identity, podcast
capture, concurrent association and the other MILESTONES.md entries remain open.
Live installation, merge and official release retain separate approval gates.

# Soggfy v3.0.0-rc.75 - maintenance reliability (test build)

RC75 snapshots and hashes the selected installation directory before xpui repair,
SpotX or Soggfy publication. Failure restores originals and removes new files.
Failed rollback retains recovery files and a manifest. Reparse points are rejected.
The transaction covers installation files, not external changes or crash recovery.

Manual watch conversion coalesces up to 4,096 paths while encoding, reconciles
on startup/overflow/directory changes, and scans every 60 seconds as a fallback.
Classic Generate M3U waits for a correlated native saved/canceled/failed result;
only successful publication reports Saved. Malformed rows reject the whole export.

Local validation includes PowerShell 5.1/7 fault injection and queue tests, real
FFmpeg watch/rename recovery, JavaScript acknowledgements and a Windows native
metadata fixture. MinGW CI and independent package verification remain required.

The RC74 compatibility check searched the wrong module. Upstream discovery works
in the pinned Spotify.dll; playback-ID layout and lifetime remain unverified.
See NATIVE_IDENTITY_AUDIT.md and MILESTONES.md. Live installation, merge and
official release remain separately gated.

# Soggfy v3.0.0-rc.74 — client playback boundaries (test build)

RC74 adds cached client playback IDs, position, pause/buffering and active-device
flags independently of optional metadata enrichment. Client identity is accepted
only when SMTC labels, duration and position agree. Delayed disagreement waits
up to the existing three-second observation timeout. Explicit IDs distinguish
repeats from same-instance rewinds; missing identity after it was seen cannot
silently fall back to title-only identity.

Unknown/unsupported content, an explicitly inactive device and stale heartbeats
fence capture events on entry and exit. The worker discards pending captures and
listening coverage, including A -> interstitial -> A. Decoder scopes retain their
entry fence; audio callbacks read a lock-free atomic. Downloads-off quality
observation remains bounded. Already-selected publication jobs may finish.

This is client observation, not exact decoder-to-playback binding. Capture/listen
selection still uses timing/duration and known decoder ownership. One-second
polling can miss brief interstitials; preloads, pointer reuse and unscoped callbacks
remain gaps. Missing/inconsistent timeline fields may discard valid recordings.
No claim is made that all ads are isolated or historical pauses/stalls are fixed.

Correction (November 8, 2026): the original compatibility check searched
Spotify.exe, but the decoder implementation is in Spotify.dll. Both upstream
anchors occur uniquely in the pinned DLL, and the upstream dispatcher and
Ogg/FLAC ownership discovery succeeds on a relocated static image. The earlier
incompatibility conclusion was incorrect. RC74 itself adds no private-memory
reads; playback-ID layout, lifetime and live integration still require validation.

Includes RC63–RC73, Classic UI, file-based status, Unicode/custom/legacy/podcast
paths, strict Ogg/FLAC validation, MP3 320 kbps and 1x–30x. Downloads off stays
at effective 1x and preserves the selection. Full CI and package verification are
required. This candidate does not authorize installation, merge or release.

# Soggfy v3.0.0-rc.73 — decoder-event isolation (test build)

RC73 rejects Ogg/FLAC callback events whose capture settings changed while the
original decoder call was running. A locked policy snapshot pairs codec enablement
with its epoch. A thread-local scope carries decoder pointer and entry epoch through
nested calls and restores the outer scope on return. Known conflicting owners
discard the active capture. Ogg and FLAC use separate capture namespaces.
Queue-loss generations reject stale callbacks and clear queued backlog after overflow.

This is decoder-event isolation, not a Spotify playback-instance identity bridge.
Unscoped callbacks retain legacy association; pointer reuse, advertisement attribution
and completed-capture/listen association still require separate work and live evidence.
The shared policy lock does no disk I/O, but live decoder latency remains unmeasured.
Historical stalls are not claimed fixed.

Includes RC63–RC72, Classic UI, Unicode/custom/legacy/podcast paths, file-based
status, cached PlayerAPI metadata, per-entry UIDs, strict Ogg/FLAC validation,
MP3 320 kbps and 1x–30x controls. Downloads off remains effective 1x without
changing the selected speed. Full portable and Windows CI/package verification
are required. Spotify 1.3.1.234.g59d6bf59 live validation is separate; this candidate
does not authorize installation, merge or official release.

# Soggfy v3.0.0-rc.72 — track identity and album provenance (test build)

RC72 completes a selective duplicate-routing and album-tagging milestone against
Floggfy v1.3-RC6. Different full Spotify track IDs at the same configured filename
route into Duplicates with 10-, 14-, then 22-character suffixes. Matching existing
duplicate files take priority over vacant shorter suffixes. Custom hierarchy and
Unicode names are retained. Legacy identity-less files remain in their established
namespace; an explicit valid foreign ID is required to create a duplicate route.

Album URI is validated in the bridge, carried through cached enrichment, and
embedded as SPOTIFY_ALBUM_URI. Conflicting IDs reject enrichment. Both native and
manual conversion preserve container and audio-stream tags, fixing metadata loss
on Ogg-to-MP3. Classic filters explicit foreign IDs and requires an exact embedded
track ID to recognize a suffixed duplicate; filename suffixes alone are insufficient.

Identity inspection supports native Vorbis/FLAC and MP3 ID3v2.3/v2.4 text tags,
including UTF-16. It reads at most 2 MiB, validates metadata structure and Ogg CRCs,
and does not claim full audio validation. Other formats retain legacy lookup behavior;
they cannot establish a duplicate identity. Mixed foreign/unknown destination
families and exhausted suffixes fail without replacing existing files.

Quality upgrades lock the previous recording, move it by handle, then publish the
complete staged file without replacement. A concurrent creator is preserved. If
interruption or a concurrent creator prevents rollback, the complete previous file
remains at a .complete-<process>-<sequence>.previous recovery path. This is not a
crash-atomic two-file transaction or automatic recovery implementation.

This candidate includes RC63–RC71. Full portable/Windows CI and package verification
are required. No live installation, merge or official release precedes separate
Spotify 1.3.1.234.g59d6bf59 validation. Decoder ownership/ad isolation, native podcast
MP3 capture and historical playback stalls remain outstanding; see UPSTREAMS.md.

## RC71 (historical)

RC71 includes RC70 and selectively adapts allocation and launcher safety from
Floggfy v1.3-RC6. Buffer growth now owns each allocation before vector insertion,
recovers spare capacity after allocation failure, and avoids empty-buffer copying.
The launcher reads bounded PE headers and rejects incompatible or malformed inputs
before stopping Spotify. It does not load binaries to inspect their architecture.

Uninstall requires version.dll from the matching complete Soggfy package. It hashes
the installed DLL under a read lock, refuses mismatches even with -Force, and
rechecks identity before deleting by handle. A missing target with an existing
backup is retained for manual recovery. Matching installations restore the backup
without overwriting a new destination; settings removal remains optional.
Scripts/ConverterProcess.cs must remain beside Uninstall.ps1 and PostProcess.ps1.

New regression gates cover allocation failures, rejected launcher inputs and
uninstall ownership under PowerShell 5.1 and 7. UPSTREAMS.md records the larger
playback-identity, ad-isolation, native podcast MP3 and duplicate-routing work still
missing. This candidate does not establish resolution of historical playback stalls.
Preserve RC63–RC70. Installer transactions and live validation remain outstanding.
No live installation, merge or official release is part of RC71.

## RC70 scope (historical)

RC70 includes RC63–RC69 and follows up the conversion limits from the maintenance audit. The target remains Spotify **1.3.1.234.g59d6bf59, Windows x64**. Live validation is still required; historical random pauses and decoder stalls are not established as fixed.

## RC70 changes

Manual encoding and validation now share a configurable deadline. The default is
five minutes plus one second per 64 KiB of input, capped at six hours; use
-TimeoutSeconds to override it. Executables and PowerShell wrappers start suspended
and join a supervised Windows job before running, so their descendants are stopped
on timeout or when the parent PowerShell process ends. The watcher can continue
after a hung converter. Normal timeout cleanup removes partial output and retains
the original; forcibly killing PowerShell can leave a temporary output.

Fresh native conversion now holds the same read protection as retries, preventing
writers or replacements throughout encoding and validation. Both conversion paths
reopen sources for deletion, compare file identity, size and timestamps, and delete
through that handle. A replacement or changed source is retained.

Tests cover encoding and validation hangs, descendant cleanup, the shared deadline,
Unicode/metacharacter wrapper paths, watcher recovery, locked sources and replacement
at deletion. Both Windows PowerShell 5.1 and PowerShell 7 run the manual suite.
Scripts/ConverterProcess.cs is included and required beside PostProcess.ps1.

Preserve RC63–RC69. Recurring full watch scans, nontransactional installer writes,
uninstaller ownership after external DLL replacement, and live playback/API behavior
remain limits. No live installation, merge or official release is part of RC70.

## RC69 changes

Native and manual encoding now fail on decoding errors rather than accepting a playable partial output. The built-in AAC fallback also recognizes the three unsuffixed M4A presets; custom commands remain explicit. MP3 stays at 320 kbps.

Manual conversion resolves relative paths and validates watch mode before changing files. Watch mode reconciles the directory after each pass, retries busy files with bounded backoff, and continues past individual failures. Files arriving during conversion are picked up on the next pass.

Queue reads and removals have bounded waits. Run ownership prevents old work from unlocking a replacement player's operation. Unsettled removals reserve their URI/UID so a retry cannot submit the same entry twice.

A separate 4,096-entry match-result cache avoids recompiling expressions when a working set exceeds the 512-expression cache. Repeating 640 unchanged queries produces 640 cache hits with zero new compilations in the regression fixture.

M3U export follows capped API pages and checks every track in batches of 128. Duplicate playlist entries retain their order. Incomplete responses, failed calls and limits produce an error instead of silently truncated exports. Saving stages a complete file before replacing an existing playlist. Visible-row fallback is identified in the message.

Settings flush accepts a newer coalesced save instead of waiting forever for the superseded version. The persisted generation and failure result are read together.

Installation checks the DLL before stopping Spotify or starting SpotX. Packaging preserves older archives and replaces a same-version ZIP only after the new archive finishes successfully. Windows publication/settings/logging/metadata/quality checks now run in both CI and release workflows; regression tests cover the new fixes.

At RC69, manual FFmpeg had no deadline and source deletion had a replacement window;
RC70 addresses those limits. Automated checks are not live Spotify validation.
Historical pauses/decoder stalls and live API variants remain unverified.

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

1. When the separate live test is authorized, close Spotify fully, extract the RC70 Windows x64 ZIP and install using Scripts/Install.ps1. Confirm the package VERSION and version.dll file properties identify 3.0.0-rc.70. Enable Debug logging.
2. With Downloads enabled at 30x, check normal complete MP3 output and skipping of existing Unicode/multi-artist files.
3. Pause during a stalled track or recovery attempt. Leave it paused past the recovery deadline; it should stay paused. Resume and confirm there is a fresh grace period before any further recovery.
4. Disable Downloads during a stall/retry and check that playback returns to effective 1x without losing the selected speed. Re-enable Downloads to restore it.
5. Retest Cool Kids, Wish You Weren't Here, Murder-Death-Kill and Intoxicated (Radio Edit). Send the resulting Soggfy.log and note whether pauses were deliberate, whether any duplicate queued track remained, and whether the saved MP3s play through.
6. Compare a short 1x run and several natural transitions at 30x. Include tracks with artwork; change speed and toggle Downloads while playback is active. Watch for repeated `media snapshot unavailable or stale` entries, lost start detection or premature green checks. Existing valid files should still be recognized from disk.
7. Include natural transitions followed by a quick manual Next, several similar-duration tracks, and FLAC if available. Check that a late complete file receives the correct track's metadata and that skipped tracks are not saved. Compare RC64 separately; preserve both logs. Association failures now include `retained_active`, the number of unfinished retained candidates.

8. Run Repeat One for at least three plays at 1x and 30x, with skip-downloaded disabled for this test. Include a normal transition where the displayed title lags, then pause/resume and manual Next. Check `listen_instance` generations, one completion per full play, correct titles and playable MP3 320kbps output. Reusing an existing valid file is expected; a repeat should not create a duplicate filename. Preserve the RC65 package and log for comparison.

Relevant diagnostics include listen_instance (phase 0 tracking, 1 awaiting identity, 2 provisional repeat), pause_observed, resume_observed, recovery_aborted_state_changed, retry_1/retry_2, skip_stalled, decoder_probe, listen_complete, publish_done and failed. A publish_done entry alone does not prove a newly converted MP3, since existing files can follow a reuse path.
