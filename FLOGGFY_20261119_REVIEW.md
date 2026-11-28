# Floggfy update review — November 20, 2026

This is a source comparison plus a local cancellation-status correction,
not an upstream feature port or a claim of runtime compatibility.
Initial Soggfy comparison baseline: ee07d62421da01a792e41b2a5df540b4604d75c3.
The Soggfy implementation snapshot and Floggfy main/releases were rechecked
on November 20. Floggfy main remains at 1e0201492afc72702337fa4980dda9314b9ab000
and the latest release remains v1.4-RC2; no newer upstream changes were found.
The Soggfy implementation snapshot reviewed is
b93d88d97c835a6c80f4b53b59aeb02a757be710.

## Pinned references

| Reference | Commit | Release publication (UTC) |
| --- | --- | --- |
| Previous comparison, v1.3-RC6 | 7b825e3b942596c3e912fbbf7a0487abd7578e47 | November 5, 2026 |
| Latest stable, v1.3 | fa26418728074e2823384b097eecfde6fde69ddf | November 14, 2026, 07:05:29 |
| Main branch inspected | 1e0201492afc72702337fa4980dda9314b9ab000 | Branch, not a release |
| Latest prerelease, v1.4-RC2 | 660ba79409629b8d572b12c420ada71ad1e55dde | November 15, 2026, 04:36:57 |

The annotated v1.3 tag object is a96d044dde4af42c637a8f219aa06cff2a34a226;
the table records its peeled commit. RC2 contains the telemetry work absent
from main. RC6 to RC2 changes 167 files (+16,034 / -3,012 lines); copying that
delta wholesale would replace substantial Soggfy-specific behavior.

## Confirmed differences and disposition

| Area | Upstream evidence | Soggfy state and next work |
| --- | --- | --- |
| Busy-file publication recovery | 236eb99 orders retry deadlines; bc95202 preserves sources and retry artifacts; native/retry_schedule.h | Implemented in source b53a2e4: retain complete captures for at most three retries (2/10/30 seconds), with a two-job/512 MiB cap and cancellation on settings-generation or capture-epoch change. All four CI jobs passed in run 216 and the package was independently verified. Live busy-file behavior remains untested. |
| Recording occurrence and metadata | bba5a2b separates continuous occurrences from catalog identity; cf032ee merges worker enrichment by exact URI | Existing listen generations, client instance keys and RefreshListenMetadata already fence refreshes by exact instance and preserve missing fields. This is partial overlap, not proof of equivalent transition handling. Compare delayed observations and A-to-B-to-A regressions against our production path. |
| Native identity | dad2e97 corroborates field layout with dump replay | RC2 PlaybackPumpHook still passes PlayerPath{0x78,0x1e8,0x9b8}. It reads a 32-byte field and learns a 16-byte payload offset from matching observations. This does not resolve the pinned 1.3.1.234 object-layout and lifetime findings in NATIVE_IDENTITY_AUDIT.md. Do not copy this chain. |
| Native podcast capture | 8f33f9b handles reused MP3 decoder streams; 4529472/f009811/c918eb8/9f3cda1/e9edc31 separate media policy from codec; 6cf609e handles explicitly cleared context | Our offline MP3 inspector is not production podcast capture. Native identity/lifetime, payload validation, completeness, decoder reuse and codec-independent terminal handling remain unfinished. |
| Independent matching and unmatched recordings | b6f9597 finalizes validated streams independently of metadata; bf58a09 retains identified MP3 songs; v1.3 release/validation describes Unmatched output | Current capture/listen association has shared-candidate rejection, but does not provide the new general recording model or Unmatched workflow. Add only after completeness and memory ownership are established; unknown identity must not become a guessed song. |
| Forensic capture and replay | af76247 stream dumps; 59960a7 captured replay; native/stream_dump.h | Current Soggfy source b93d88d includes the default-off bounded Ogg/FLAC evidence queue, per-session payload and JSONL records, loss/incomplete markers, CRC/offset/schema validation and prefix-gated replay. CI 218 and independent package verification passed. It records compressed source audio, not PCM or pointer contents. Pinned-client evidence remains pending. |
| Matcher diagnostics and memory | 5d222ae/c320b10/90e2618 bounded snapshots and rejection/ownership explanations; recording_memory.h and tag_memory_plan.h | Busy-publication retry ownership now has explicit two-job/512 MiB limits and cancellation. Broader matcher coverage/rejection snapshots and memory accounting for a future independent recording model remain unimplemented; add them with that lifecycle rather than guessing unknown track identity. |
| Artwork refresh | f680460 publishes freshness with cache content | Our supervised PNG/JPEG validation is complete at the recorded CI checkpoint; source/replacement policy and saved-recording coordinator remain disabled. Decoding an image does not settle freshness or publication policy. |
| CEF compatibility | b64bf0d adds CEF 156 observer ABI; 537fb4f attempts discovery on unknown versions | New-version support needs a separately pinned client and ABI evidence. Preserve existing fail-closed discovery; do not infer 1.3.1.234 compatibility from the newer client's tests. |
| Never Pause | v1.3 adds an optional, default-off control | No equivalent named control found in active Soggfy sources. Existing pause-safe recovery is not the same behavior. Do not replace it with unconditional resume behavior. |
| Telemetry | RC2 adds categories, broader suppression and optional readable gzip/protobuf dumps | Our existing block_telemetry setting defaults true and filters the gabo/dodo CEF request prefixes. That narrow filter is separate from the default-off NetworkPatch option. RC2's BlockTelemetry=0 policy, categories, renderer producers and dump schemas are not already implemented here. Evaluate separately without changing existing defaults in this review. |
| Speed and UI | v1.3 advertises speeds through 10x | Preserve our Classic UI, 1x–30x selection, Downloads-off effective 1x, selected-speed retention, configured paths and file-based status. Do not copy lower limits or replace the UI. |

## Validation boundary

Upstream v1.3 VALIDATION.md explicitly targets Spotify 1.3.9.2055. It reports
35 saved FLAC recordings and nine complete Ogg sources with source/decoding
checks, plus incomplete/pending sources and an unmatched MP3 advertisement.
These are upstream-reported results; the underlying private captures were not
independently replayed here. The document distinguishes its live-tested DLL
from the final diagnostic-only candidate. RC2's telemetry evidence is likewise
new-client-specific and does not prove comprehensive native coverage.

Our pinned Spotify.dll remains version 1.3.1.234, SHA-256
210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f.
The existing native audit has 1,299 pinned instruction checks; concrete scheduler
slot +0x70 completion behavior and synchronized callback lifetime remain open.
Correlation with metadata cannot establish safe pointer lifetime.

## Additional local defect found during comparison

In native/audio_history.cpp, retry_existing returns Publication::Skipped with
a nonempty path when its pre-conversion cancellation check fires. SaveWorker
treats Skipped plus a nonempty path as DONE. A settings change at this point can
therefore acknowledge a canceled conversion as completed. The accompanying
change introduces Publication::Canceled at all three settings-cancellation
returns. It cannot enter the Saved/Skipped success branch, and the error status
retains the existing native path. It does not add automatic retries.

Local verification: the complete audio_history.cpp translation unit compiles
with the diagnostic Zig Windows target, and classic_status_protocol_test.js
passes. The initial Zig -fsyntax-only attempt failed with FileNotFound; an
actual object compilation succeeded. These checks do not exercise a live
settings-change race. Full MinGW CI/package and live acceptance remain separate.

## Current implementation order and gates

1. Canceled/publication outcomes, bounded busy-file retries and default-off
   compressed-stream evidence/replay are implemented and CI/package verified.
2. Continue pinned native identity, callback completion/lifetime, decoder
   association, ad/DJ isolation and capture completeness evidence. Until those
   are established, native MP3 capture and unmatched publication stay disabled.
3. Finish saved-recording artwork/source policy and filesystem preservation
   before enabling the refresh coordinator.
4. Evaluate newer-client CEF and optional telemetry independently against a
   separately pinned client; preserve current defaults and UI behavior.
5. Run full CI and independent package verification for each source update.
   Live acceptance, merge and official release remain separate gates.

This review changes no runtime defaults, installed files or staged live-acceptance
package. The cancellation correction is a source change requiring its own CI
checkpoint; earlier package evidence does not validate the new source.

Follow-up: the cancellation correction passed CI 206; bounded busy-file retry
passed CI 216; and current source b93d88d passed all four CI jobs in run 218
plus independent RC87 package verification. Floggfy's newer-client pointer
chain was not ported. Live settings-change and busy-file races, native capture
and pinned-client identity/lifetime remain unverified.
