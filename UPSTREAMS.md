## RC74 client playback boundaries — November 8, 2026

Continues the pinned v1.3-RC6 (7b825e3b942596c3e912fbbf7a0487abd7578e47)
review. Its cached PlayerAPI fields informed client-ID observation and unsupported
content fencing. The pinned Spotify binary lacks both instruction anchors used
by its private playback-ID hook; that hook and offsets were not copied.
This is not a fresh latest-release survey or full feature parity. Exact native
playback binding, native podcast MP3 capture and live acceptance remain open.

## RC73 decoder-event isolation — November 8, 2026

Continues the source review of Floggfy v1.3-RC6
(7b825e3b942596c3e912fbbf7a0487abd7578e47). RC73 uses Soggfy's existing
decoder hook to carry dynamic call context without adding private object offsets.
It fixes event-entry epoch races, rejects known decoder-owner conflicts, separates
codec context keys and fences queue loss. It does not port the upstream private
playback identity bridge or establish full upstream feature parity. Direct playback
identity/ad isolation, native podcast MP3 capture and live validation remain open.

# Upstream sources

The active x64 implementation started from:

- Mainkill1/Floggfy tag `v1.1.0`
- commit `dbf1ba819a7a7a52e9992ad5c786ac74b9a93cac`

The original Soggfy repository was at commit:

- Rafiuth/Soggfy `596bc4e3942a42f401e857c875ec456f9457b768`

The old source is preserved under `legacy/`.

Community work reviewed while building this fork:

- SuperSecretEyeball/Soggfy-Fixed — artist/path metadata changes and TypeScript cleanup
- MacKinnon7/Soggfy-Fixed — newer release-build workflow fixes
- AndyBogle1/Soggfy / upstream PR #143 — safer installer reuse behavior
- coleaderme/Soggfy / upstream PR #125 — TLS 1.2 installer compatibility
- MrSykenro/Soggfy / upstream PR #148 — current SpotX script URL
- upstream PR #102 — quoted/idempotent uninstall-path handling

Not every patch was copied literally. Fixes tied to the obsolete x86 installer/hook architecture were reimplemented only where they still apply to the modern x64 design.

Floggfy v1.1.0's optional explicit launcher and episode current-item identity fixes are included. The all-in-one ZIP includes the launcher in `Launcher/` alongside automatic `version.dll` mode. Use one mode at a time.

Soggfy's capture core intentionally remains ahead of Floggfy v1.1.0 in a few places: replayed Ogg pages are tolerated without duplicate writes, complete-listen validation is aware of accelerated playback, and the Classic UI/path/post-processing layers remain Soggfy-specific. A wholesale rebase would regress those fixes, so upstream changes are merged selectively.


## RC64 selective observer adaptation

Reviewed Mainkill1/Floggfy v1.2.4 (`cf72f18b65afc24ed3c5a4899c80d2ff68b75d74`)
and v1.2.5-RC1 (`15e4b764c7d78bd5ecfe67d6414def505a4410c5`). RC64 adapts
the dedicated media-reader and sequenced-snapshot boundary from commits
`4953c0a3874ec3178a6462183bb43739a22d132c` and
`72a4160c4cf6b26ae3fe759b76f673dd886af275`, with configuration revision fencing,
pre-artwork observation timestamps and Soggfy's existing accelerated clock.
The gated Windows test follows upstream's blocked-provider test approach.

Normal-speed timeline continuity rules, late-capture retention, repeat/delayed
identity handling and RC1's opt-in NetworkPatch defaults are not included in
this candidate. They require separate compatibility work. RC1 itself does not
claim that random pauses are solved.

## Compatibility references

RC65 selectively adapts late-capture retention from Mainkill1/Floggfy commit
`f80dfdd91f740afdce22869308b504308e072a61`. Soggfy keeps its existing association
window and accelerated listen tracker, binds retention to the original completed
listen and defers selection while a retained competing stream is incomplete.
The shared portable policy is tested with actual Ogg and FLAC validators.
The RC64 exclusions above describe RC64 specifically; RC65 did not include
upstream repeat/delayed-identity generation changes. Network defaults remain
outside RC66 as well.

## RC66 selective repeat and identity adaptation

Adapted from Mainkill1/Floggfy commits
`1bb4bf303cf09b303cd3c585266479d7b100cb6d` (generation-aware repeats),
`54f4ff416ab4280e33ccc0db5a62c83bf6c8abef` (external coverage invalidation), and
`f27deafebf14f6929d34aee26da6f5e2367a57b7` (delayed title reconciliation).
Soggfy retains its 1x–30x clock and adds raw-reset evidence, previous-rate
continuity, immutable completion timing and generation-bound capture retention.
The one-second confirmation and original two-second identity window are tested
against normal and accelerated playback. This is a selective adaptation, not
feature parity or a wholesale upstream timeline replacement. Live Spotify
Repeat One behavior still needs validation.

Playback-speed compatibility was independently reimplemented against the current x64 Spotify track-player calling convention and validated with structural PE checks. Public reverse-engineering references were used only to confirm the modern calling convention and pattern family; no third-party implementation source was vendored.

The active hook requires a unique validated Spotify.dll target and fails closed when discovery is missing or ambiguous.

## November 5, 2026 comparison and RC71 selective ports

Compared RC70 (4327835b65c0e8005faaf38054ecb7ef6a7db0eb) with Floggfy
v1.3-RC6 (7b825e3b942596c3e912fbbf7a0487abd7578e47), published November 5,
2026 at 01:16:30 UTC. The release tag is ahead of main (6c7218475f33867a44f732dad89437508d73bfed).
Sources: https://github.com/Mainkill1/Floggfy/releases/tag/v1.3-RC6 and its tagged
source tree. Release notes describe a prerelease still awaiting an exact-artifact
restarted live walkthrough. Earlier upstream live checks used their own client and
do not validate Soggfy's pinned Spotify 1.3.1.234.g59d6bf59.

RC71 adapts compressed_buffer.cpp allocation ownership and spare-capacity arithmetic,
plus the allocation-failure regression approach. It preserves uninitialized block
allocation to avoid unnecessary zero filling. It also ports launcher_architecture.h
and invokes RequireX64 before StopExisting, with Soggfy names and local regression
fixtures. These are small independent safety changes, not a capture-core rebase.

| Upstream change | Soggfy status after RC71 | Follow-up |
| --- | --- | --- |
| Buffer allocation failure recovery | Adapted and fault-injection tested | Included |
| Launcher PE architecture preflight | Ported before process control | Included |
| Dedicated observer, late retention and repeat generations | Earlier selective RC64–66 adaptations | Preserve accelerated-clock behavior |
| Direct Spotify playback source and decoder ownership | Not ported | Integrate private ABI discovery, producer sequences and capture ownership together |
| Playback identity bridge and staged natural transitions | Not ported | Test bounded history, repeats, preloads, pause/resume, skips and seeks across 1x–30x |
| Ad/DJ/interstitial audio isolation | Not ported | Requires trustworthy playback and decoder identity; SpotX remains separate |
| Native podcast MP3 parser/capture | Not ported | Podcast path templates and MP3 transcoding are not native MP3 capture |
| Concurrent capture validation and lifecycle/cross-format refinements | Partial overlap with existing strict validators/quality replacement | Compare association and ownership semantics before integrating |
| Playback-time metadata refresh and instance-bound auto skip | Existing cached PlayerAPI/UID skip is different | Adapt to Classic queue reservations and file-status rules |
| Verified different-track duplicate routing | Not ported | Integrate URI-based canonical lookup, suffix expansion and quality upgrade destinations |
| Album URI in final provenance/tags | Cached album_uri exists; final rich tagging not ported | Carry through merge/tag/publication without losing existing metadata |
| LogLevel 0/1/2/4 and independent bounded diagnostic lanes | Not ported | Reconcile with existing Soggfy diagnostics/settings and privacy behavior |
| Deterministic sanitizer fuzz campaign | Not ported as an upstream campaign | Add after choosing the capture/identity boundaries to integrate |
| CaptureSpeedLimit menu semantics and 1/2/4/8/10x choices | Intentionally not copied | Keep Classic 1x–30x and Downloads-off effective 1x with selection retained |
| NetworkPatch default-off opt-in | Not ported | Keep separate from the native telemetry filter and SpotX policy |

Priority for deeper work: playback/decoder identity and ad isolation first; native
podcast MP3 second; identity-aware duplicate routing/album provenance next; then
metadata refresh, diagnostics and broader fuzz coverage. Capture changes must retain
strict complete-listen validation, Unicode/multiple artists, legacy/custom/podcast
paths, file-based statuses and the pinned client. Upstream's FLAC overlap incident
on Spotify 1.3.8.614 does not establish a fix for Soggfy's historical random pauses.

Separate maintenance remains: transactional installer/SpotX rollback, watch-scan
cost, and native playlist save acknowledgement. RC71 closes the known blind DLL
deletion in uninstall; it does not make the entire installation process transactional.

## RC72 identity milestone — November 5, 2026

Adapted the v1.3-RC6 duplicate-routing concepts and album-URI provenance to Soggfy's
custom path templates and Classic file status implementation. This is an independent
implementation, not the upstream RecordingFile/song_destination architecture.

| Area | RC72 result | Limits |
| --- | --- | --- |
| Album URI | Validated live/cached identity, canonical SPOTIFY_ALBUM_URI tags, conflicting release rejection | Requires available matching local metadata |
| Different-track duplicate routing | Explicit full track-ID conflict routes through 10/14/22-character suffixes under Duplicates | Canonical unknown/legacy data never authorizes routing; mixed conflicting families fail |
| Destination reuse | Matching native or MP3 converted duplicate preferred over vacant shorter suffix | Other converted formats do not establish identity |
| Publication | Recheck under destination lock; move old identity by handle; publish without replacing a concurrent creator | Interrupted rollback can retain .previous recovery file; no automatic crash recovery |
| Classic downloaded state | Filter foreign embedded IDs; require exact tags for suffixed duplicates | Legacy identity-less lookup retained; no full playback proof from tags |
| Metadata through conversion | Copy global and audio-stream tags; real FLAC/Ogg-to-MP3 regression coverage | Container support still determines which tags survive |

Identity inspection is bounded to 2 MiB and recognizes Vorbis/FLAC comments and
MP3 ID3v2.3/v2.4 text encodings. It validates native headers and Ogg metadata CRCs,
without replacing strict complete-capture or FFmpeg decode validation.
The direct decoder/playback identity, ad isolation, native podcast MP3, metadata
refresh/instance-bound skip, diagnostic lanes and sanitizer campaigns listed above
remain open. Classic 1x–30x and the pinned client are retained. RC72 is a candidate
milestone, not upstream feature parity or proof of a live-stall fix.

## RC75 maintenance checkpoint

Installation-directory rollback, bounded watch events and correlated playlist
save results have local regression coverage; CI/package verification is pending.
See MILESTONES.md for outstanding work and NATIVE_IDENTITY_AUDIT.md for the
correction to RC74 native compatibility findings.
