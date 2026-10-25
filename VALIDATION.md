## RC69 candidate scope

RC69 includes RC63–RC68. The maintenance audit covers the Classic UI/queue/export
paths, native status/cache/publication/conversion, settings/logging/metadata, capture
and recovery boundaries, installer, manual conversion, packaging and CI gates.
It is a bounded source and automated-test audit, not proof that every line or live
Spotify behavior is defect free.

Local assertion-enabled Zig diagnostics pass the 32,768-file status suite, 640-query
result-cache reuse and 4,096-entry eviction; native conversion with controlled failures
and real FFmpeg; manual conversion; and Windows publication/settings checks. All
13 JavaScript suites pass. Real watch tests cover arrivals during encoding, locked
files, corrupt input, relative paths and invalid watch preflight. Packaging checks
verify deterministic bytes and preservation of old/same-version ZIPs on failure.

Both workflows now require the additional Windows publication, settings, logging,
metadata/cache and playback-quality suites. CI must pass for the final commit before
the candidate is accepted; local Zig diagnostics do not replace the MinGW build.

Live validation remains required on Spotify 1.3.1.234.g59d6bf59 Windows x64: 1x/30x,
Downloads-off speed, pause/recovery, Repeat One, Ogg/FLAC, long playlist/album export,
player replacement, Unicode/custom/podcast paths and extended playback.
Historical random pauses and decoder stalls are unresolved. Known remaining limits
include recurring watch scans, manual FFmpeg without a process deadline, a small
source-replacement window around optional deletion, nontransactional installer writes,
and uninstall ownership if another tool replaces the installed DLL. Settings flush now accepts newer coalesced writes, checks failures consistently and
handles generation wrap; the deterministic production-code regression passes. The remaining limits are
explicit follow-up risks, not established regressions or claimed fixes.

Preserve RC63–RC68. Do not merge, officially release or install into live Spotify
before the separate live-validation gate.

## RC68 candidate scope

RC68 includes all earlier candidate changes. Local validation uses Zig 0.13 with
assertions enabled (-UNDEBUG); shipped binaries still use MinGW CI. All twelve
JavaScript suites cover paged rows, obsolete page cancellation and stale queue
events, as well as existing pause/recovery and cached-player behavior.

The Windows status suite checks 32,768 files and 128 misses with zero cold regex
checks, 128 warm cache hits, unchanged-rescan reuse, added/deleted files,
locked/unlocked candidates, ambiguity, title-free/nested-token custom templates,
file/directory/depth limits, unavailable roots and cancellation during scan/match.
Local cold lookup measured 547 ms and warm lookup 0 ms at millisecond resolution.
The separate diagnostic measured 813 ms; RC67 measured about 46 seconds on that
synthetic miss workload. These are not live Spotify performance measurements.

Conversion tests use controlled failures and real FFmpeg: MP3 320 kbps full
decode, generated FLAC/Ogg failure-then-retry, corrupt/truncated native sources,
existing destinations, cancellation and original preservation. Manual-script
tests cover empty/corrupt output, failed conversion, raced files/directories,
320k arguments, explicit deletion only after validation and temporary cleanup.
CI installs FFmpeg and requires the real-codec cases through the shared Windows
maintenance runner. The fake converter tests control flow, not codec validity.

Live validation remains outstanding on Spotify 1.3.1.234.g59d6bf59 Windows x64:
check large playlists, changed save folders, skip policies/player replacement,
retained-native conversion retry and normal 1x/30x capture/playback. Retry occurs
only when capture reaches existing-file publication; it is not a background job.
Incomplete/reparse-limited libraries show uncertainty. The cooperative lookup
deadline cannot interrupt a blocked OS call or individual regex operation.
Preserve RC63–RC67. Do not merge, publish an official release or install into
live Spotify before the separate live-validation gate is satisfied.

## RC67 candidate scope (included in RC68)

RC67 targets Spotify **1.3.1.234.g59d6bf59, Windows x64** and includes RC63–RC66.
The maintenance gate adds twelve total JavaScript suites and bounded existing-file
Vorbis/FLAC header tests to the portable suite. Windows CI and release workflows
also run production conversion and library-status tests with isolated fixtures.

The conversion harness checks directories, empty/corrupt/locked destinations,
valid pre-existing output with native retention, failed/empty/corrupt conversion,
atomic destination conflicts, timeout/cancellation, child-process termination,
successful publication and keep-original behavior. Optional SGF_REAL_FFMPEG runs
an installed FFmpeg against generated PCM for MP3 320 kbps encode/full decode,
valid/corrupt existing output and header-only audio. Fake-converter cases test
process/control flow; only the optional smoke exercises real codec behavior.

The status harness checks actual files, deletion, locking, empty files, directory
conflicts, ambiguity, publication/root/template invalidation, exact artist identity,
Unicode/multiple artists, legacy paths, podcasts/custom extensions and cache eviction.
Its deterministic 2,048-file/128-query repeated-miss benchmark requires zero warm
regex checks and compilations, versus 786,432 checks and 384 compilations cold.
Local diagnostic binaries use Zig 0.13 with assertions enabled (-UNDEBUG); shipped
binaries still require the established MinGW CI build and package verification.

Live validation must include MP3 320 kbps playback, conflicting destinations with
native retention, deletion/addition of library files, changed save folders/templates,
skip-downloaded retries and normal 1x/30x capture. External additions reconcile
after the ten-second directory-index lifetime plus polling; queue results expire
after three seconds. Full output decoding adds work and the save queue remains
bounded. Network/storage stalls and sustained overload are not eliminated.
Preserve RC66 for comparison. Do not merge, publish an official release or install
into live Spotify before the separate live-validation gate is satisfied.

## RC66 candidate scope (included in RC67)

RC66 targets Spotify **1.3.1.234.g59d6bf59, Windows x64**. Local portable
tests use workspace Zig 0.13 with assertions explicitly enabled (`-UNDEBUG`);
release binaries still use the established MinGW CI toolchain. The new
`repeat_identity_test.cpp` combines the production accelerated clock and listen
tracker at 1x, 2x, 10x and 30x. It covers two consecutive repeats, original start
and completion times, titles arriving before/after confirmation, changed
durations, timestamp refreshes, stale raw observations, seeks, missing coverage,
pauses, rate changes, short repeats and the original two-second deadline.
Capture tests also verify original-generation ownership and the unchanged
ten-second completion deadline through delayed relabeling.

A minimal full six-second same-title repeat fixture returns no completed listen
on RC65 and exactly one on RC66 after one second of confirmation. Existing
history/position tests, capture tests and all eleven JavaScript suites pass
locally. CI runs the full native suite, Windows observer boundary test, release
build and package checks. These are synthetic/source regressions, not live
Spotify evidence. Repeat One, delayed titles, manual seeks/Next, pause/resume,
Downloads toggles and correct MP3 metadata need live 1x/30x comparison before
merge or release. Very short/sparse accelerated resets may be unrecognized;
manual seeks near a natural end can be indistinguishable from a repeat.

## RC65 scope (included in RC66)

RC65 targets Spotify **1.3.1.234.g59d6bf59, Windows x64**, and includes RC64 below.
`bash test-native.sh` now compiles `tests/capture_association_test.cpp` against
the shared production transition cleanup, retention, selection and expiry policy.
Synthetic callbacks use the real Ogg CRC/sequence/EOS validator and FLAC decoded
frame coverage. Cases cover late completion, deadline boundaries, skipped/ignored
listens, rapid Next overlap, ownership across later transitions, competing
captures, invalid streams, duration/start mismatches and a natural 30x transition.
The test asserts that RC64's deletion predicate selects the late-callback fixture.
This is a portable policy/codec regression, not a live Spotify or full Windows
hook/worker integration test. CI must pass before distributing a candidate.
Preserve RC63/RC64/RC65 packages for comparison.

No historical missing-stream event is proven to have this cause. A stalled
decoder that never supplies the final callback still times out. Unresolved
competing retained captures fail closed at ten seconds, which may reject an
otherwise complete candidate. No wider association window is introduced.

## RC64 observer scope (included in RC65)

RC64 targets Spotify **1.3.1.234.g59d6bf59, Windows x64**. Older live-test
accounts below are historical evidence, not validation of RC64 or a change of
the chosen Spotify baseline. This candidate needs live testing before merge or release.

`bash test-media-observer-windows.sh` runs a focused Windows regression against
the production snapshot reader with an injected provider. The provider is held
for more than three seconds while a consumer continues polling and draining a
bounded queue. Tests cover the original sample timestamp, failures/exceptions,
invalid/future samples, oversized artwork, duplicate/skipped sequences,
generation/rate/epoch/artwork changes (including A-B-A), nonblocking stop and
single-worker restart with monotonic sequences. This is a boundary test, not
a live Spotify or full capture-worker integration test. CI and release workflows
run it explicitly on Windows before packaging; the broader historical Windows
test script is not implicitly covered by that step.

The portable position-clock suite adds 30x coverage, variable delivery latency,
pause/resume, effective-1x clock transitions and rejection of real observation
gaps. Existing strict stream-integrity and eleven JavaScript suites remain gates.
No test here establishes the cause or elimination of Spotify's random pauses,
decoder stalls or late stream-association failures.

## Automated checks (historical baseline and continuing suites)

`bash test-native.sh` covers Ogg integrity/complete listens, native artwork/tags,
FLAC metadata and contiguous frame coverage, fixed RAM block budgets, requested
layout and conservative quality rules, bounded UTF-8 metadata identity, and a
publication queue whose consumer is deliberately blocked. The collector test
uses simulated existing client caches for date, genre and Unicode lyric transport.
Request functions and property getters are traps: zero requests, service
resolutions or accessor invocations are allowed. Unrelated track caches are
ignored, and later cache updates are collected without fetching anything.

Windows tests exercise real file APIs: equal/higher-quality skips, failed-upgrade
preservation, atomic upgrades, downgrade prevention, asynchronous INI persistence,
no premature save-folder creation and the 5 MiB log reset/oversize/unwritable cases.
Debugger startup checks survive and detach. Actual menu clicks toggle Downloads,
FLAC and Ogg and persist their values; Menu=0 startup and the folder picker also
have been checked.

## Final release checks

The cache-only collector was loaded in the real client and located its existing
PlayerAPI without constructing services. A track was saved after a complete
listen through the background publication worker; independent full decoding
verified its source MD5 and embedded artwork. Its cached Spotify URI, disc number,
disc total and track total were embedded. Release date, genre and lyrics were
absent from the observed real playback cache and were omitted. Simulated cached
field tests establish transport correctness; they do not establish availability
of those fields in every live client. No Spotify endpoint requests are made.

The release candidate was separately started with Menu=0, Metadata=0 and Log=0;
Log=0 left the existing log size and modification time unchanged. Review covered the bounded
publication/log/settings workers, reference forwarding and the descriptor-only
cache collector correction. Eight one-second rapid restart runs exited normally,
and a debugger startup/detach check left the process running.

With all optional integrations enabled, the installed candidate saved a complete
native FLAC from Spotify 1.3.3.264. The official Xiph `flac` decoder reported zero
errors, verified the original STREAMINFO MD5, decoded 6,313,591 samples at 44.1 kHz
stereo, and found embedded front-cover art plus the required tags. The activity
log recorded one decoder initialization, one complete stream, one complete listen
and one saved file, with no decoded-frame coverage gap.

The production audio resolver was run against Windows-mapped Spotify DLLs from
1.3.3.264, 1.3.0.277, 1.2.94.583 and 1.2.92.148. All six targets were found at
the independently established function RVAs in every sample. The production
connectivity resolver found `CoCreateInstance` in every sample by import name,
without a Spotify hash, RVA or provider DLL assumption.

Previous v1.1.0-rc.1 `version.dll` SHA-256:
`854249b471baeea8d7072d4403018fde56e933a6a7ddc5e372a4b6e19fd6cd67`.

An installed-client restart of this exact DLL produced two connectivity patch
events in the same startup, showing that the monitor restored the delay-IAT slot
after Windows resolved and replaced it. The Network List Manager hook then
installed, the compatibility override ran, Spotify kept established connections,
and the dynamic audio, metadata and menu hooks initialized. No connectivity-hook
failure was logged.

The menu integration was then changed from an exact CEF identity allowlist to
runtime capability discovery. Synthetic tests accept the audited structure size
and larger append-compatible structures, reject truncated structures or missing
methods, and recognize a localized top-level menu by item types rather than text.
The installed Spotify 1.3.3.264 client exposed a 488-byte model; the release DLL
validated its required executable methods, installed both menu hooks and inserted
Downloads, Save Location, FLAC and Ogg without consulting the CEF version.

The archived Spotify 1.2.92.148, 1.2.94.583 and 1.3.0.277 packages were also
checked with their actual UI binaries. The production PE scanner found the
`cef_menu_model_create` delay import in every `Spotify.dll`, and every paired
`libcef.dll` exported that factory and loaded as CEF 146.0.10 commit 3504. The
installed Spotify 1.3.3.264 client uses CEF 151.3.18 commit 3578. Official headers
for both revisions have the same required 41-method menu prefix and the same seven
delegate callbacks. This establishes static discovery and ABI compatibility for
the archived packages; they were not executed end to end.

Recording names, per-file hashes and personal listening data are not published.

## CEF client identity crash repair

On Spotify 1.3.3.264, opening the mini player with metadata enabled reproduced
CEF's fatal `UnwrapDerived called with unexpected class type 0` check. Disabling
only metadata prevented that crash. The bridge now preserves the original CEF
client and handler objects and intercepts their callback function addresses.

The Windows identity regression failed before the repair and passed afterward.
It covers all three browser factories, unchanged client/handler structures,
original callback arguments and transferred browser references. The Linux
native/JavaScript suite and Windows history/log/metadata suite all passed.

With metadata enabled, Settings → Connected apps → View succeeded twice and
the mini player opened without a fatal debugger exception. The cached metadata
collector still received fields. These are checks of the reported UI paths;
they do not resolve earlier intermittent freeze reports.

Repaired `version.dll` SHA-256:
`59503908963397442c8daf4ae95407c98aacc7a0aac714c2866ee8346af82098`.

## Cached catalogue metadata enrichment

The renderer collector now exports every cached contributing artist and album
artist, retains full release dates, and separates label/organization tags from
explicit publisher credits. The metadata worker also reads existing Spotify
LevelDB tables and complete WAL records through shared read-only file handles;
it does not open/recover the database, take its lock, query endpoints or construct
client services. Exact cache URI, embedded protobuf GID, title, album and duration
are verified before merge. Cached artist lists cannot remove renderer artists.

Synthetic Linux and Windows tests passed for separate album records, signed
protobuf dates, artist lists, label/copyright tags, exact identity and wrong-GID
rejection, Snappy compression, WAL fragments/truncated tails and tombstones.
Checksum-valid repeated block handles and excessive aggregate expansion are
rejected. AddressSanitizer and UndefinedBehaviorSanitizer passed the cache suite.
The Windows suite also verified concurrent shared reads, an exclusively held
WAL, and rejection of readable corruption instead of silently using older data.
Existing history, Ogg/FLAC tagging, log and CEF object-identity regressions passed.

The installed Windows Spotify 1.3.3.264 client received enriched fields through
the production metadata worker, stayed responsive and initialized connectivity
repair. A metadata-only update to an existing FLAC preserved its compressed audio,
STREAMINFO and embedded artwork byte for byte. Private recordings and cache
records are not included in the repository or release.

Lookup work uses background I/O priority, bounded files/bytes, block expansion
limits and a 24-entry memo cache. Its two-second cutoff is cooperative between
file operations, not cancellation of a stalled disk read. Spotify can exclusively
hold its active WAL; in that specific sharing-lock case the reader uses matching
readable SST catalogue records, which may lag WAL updates or evictions. Other
read/parse failures retain ordinary tags. Missing fields remain absent. This
change does not automatically retag the entire existing library, and earlier
cross-version/CEF support limits still apply.

Enriched `version.dll` SHA-256:
`153b7d52f4f49973efaedceee79e17348c186158f7d1ce3dd27524448c2a66e0`.

## RC4 playback quality menu (historical layout)

The bottom of To Disk has five disabled informational rows: song, quality,
format, bitrate and sample rate (with FLAC bit depth). A worker publishes a fixed
RAM snapshot; the menu uses a nonblocking read and rejects snapshots older than
three seconds. Opening this section performs no file, artwork or endpoint reads.
Downloads-off observation uses the existing bounded queue and retains only small
headers and counters, without creating whole-song capture buffers.

The existing renderer collector reads actual current playbackQuality/
playback_quality from its local player snapshot, never targetBitrateLevel.
Known string levels and protobuf enum values 1–6 are normalized; new values
remain unavailable. The numeric enum mapping was checked in the installed
1.3.3.264 DLL and the archived 1.3.0.277, 1.2.94.583 and 1.2.92.148 DLLs.
Native use requires an exact title, artist, album and duration cache match,
candidate uniqueness and receipt age under twenty seconds. Playback quality is
not embedded into permanent song tags. Metadata=0 disables this optional source;
validated decoder headers still identify lossless quality and FLAC bit depth.

Synthetic Linux and Windows tests cover current-vs-target quality, quality
changes, missing/unknown fields, zero network/service/accessor calls, stale/wrong/
ambiguous cache identities, Ogg duration corroboration, read-ahead rejection,
same-song replay, separate title/timeline transitions, seek invalidation,
codec/quality conflicts, menu placement,
refresh, disabled-command ownership, snapshot contention and expiry. Observer
tests verify that showing quality cannot enable saving and preserves the original
Ogg parser return.

Numeric bitrate is the average encoded bitrate once the complete stream is
known, rather than read-ahead bytes divided by partially decoded time. Ogg decoder
association waits for EOS to corroborate duration. Decoder matching uses timing
and duration heuristics, not a native track ID; unknown or ambiguous sources
remain unavailable. Labels refresh on opening the submenu, rather than changing
continuously while it stays open. No estimated file size is displayed.

On installed Windows Spotify 1.3.3.264, all five disabled rows appeared in the
actual To Disk submenu. Cached Lossless quality was displayed with Downloads
off, and the audio library remained unchanged. A live observed FLAC stream
also displayed 44,100 Hz during development. Final native and Windows suites
passed, along with the playback tracker under AddressSanitizer and
UndefinedBehaviorSanitizer. Numeric bitrate was verified with synthetic complete
streams; this release does not claim an additional full real-track bitrate test.

## Current-track footer correction

The current-track heartbeat reads the existing local player snapshot once per
second. Its short message bypasses artwork reads and metadata enrichment.
CEF’s public FormatLabel callback reads a fixed-size RAM snapshot while
constructing visible labels, before MenuWillShow; it no longer includes
Bitrate. A fresh client identity takes precedence over a lagging Windows media
session. Decoder format and sample rate are retained only when identities and
quality agree. Missing or stale client state clears the footer instead of
restoring the previous song. No request endpoints are called.

Regression checks cover a client track change ahead of Windows, repeated old
Windows publications, matching decoder information, expired heartbeats,
clearing the current song, contradictory FLAC bit depth, pre-show label
formatting, and four disabled menu rows without duplicates.

Live Windows Spotify 1.3.3.264 validation confirmed that the first submenu
opening displays the visible current track. Consecutive skips were checked
after allowing two seconds for the heartbeat: the footer matched the new
visible track each time, contained four disabled rows, and had no Bitrate row.
The current observed FLAC stream displayed its validated sample rate and
bit depth. Older-client runtime behavior remains unvalidated.
