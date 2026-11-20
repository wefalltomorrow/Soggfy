# Floggfy update review — November 20, 2026

This is a source comparison plus a local cancellation-status correction,
not an upstream feature port or a claim of runtime compatibility.
Soggfy baseline: ee07d62421da01a792e41b2a5df540b4604d75c3.
GitHub releases were rechecked on November 20; both release branches were
inspected using pinned git objects without changing the upstream checkout.

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
| Busy-file publication recovery | 236eb99 orders retry deadlines; bc95202 preserves sources and retry artifacts; native/retry_schedule.h | SaveWorker in native/audio_history.cpp makes one Publish attempt and releases the capture afterward. Add bounded retained jobs, explicit transient errors, cancellation and memory accounting before enabling retries. Retrying the entire Publish operation blindly could repeat tagging/conversion or choose a different destination. |
| Recording occurrence and metadata | bba5a2b separates continuous occurrences from catalog identity; cf032ee merges worker enrichment by exact URI | Existing listen generations, client instance keys and RefreshListenMetadata already fence refreshes by exact instance and preserve missing fields. This is partial overlap, not proof of equivalent transition handling. Compare delayed observations and A-to-B-to-A regressions against our production path. |
| Native identity | dad2e97 corroborates field layout with dump replay | RC2 PlaybackPumpHook still passes PlayerPath{0x78,0x1e8,0x9b8}. It reads a 32-byte field and learns a 16-byte payload offset from matching observations. This does not resolve the pinned 1.3.1.234 object-layout and lifetime findings in NATIVE_IDENTITY_AUDIT.md. Do not copy this chain. |
| Native podcast capture | 8f33f9b handles reused MP3 decoder streams; 4529472/f009811/c918eb8/9f3cda1/e9edc31 separate media policy from codec; 6cf609e handles explicitly cleared context | Our offline MP3 inspector is not production podcast capture. Native identity/lifetime, payload validation, completeness, decoder reuse and codec-independent terminal handling remain unfinished. |
| Independent matching and unmatched recordings | b6f9597 finalizes validated streams independently of metadata; bf58a09 retains identified MP3 songs; v1.3 release/validation describes Unmatched output | Current capture/listen association has shared-candidate rejection, but does not provide the new general recording model or Unmatched workflow. Add only after completeness and memory ownership are established; unknown identity must not become a guessed song. |
| Forensic capture and replay | af76247 stream dumps; 59960a7 captured replay; native/stream_dump.h | The PR source at b53a2e414397f0814afbe909c6abd012acb880c2 adds a default-off bounded Ogg/FLAC evidence queue, per-session payload plus JSONL records, loss/incomplete markers, CRC/offset/schema validation and prefix-gated replay. CI run 216 passed all four jobs for that exact source; independent verification passed the packaged artifact (inner ZIP SHA-256 333713a8321410b21d10be15e2ac00da7949a511917af324ba6a40a001e773b6). Pinned-client evidence remains pending. The recorder stores compressed source audio and does not capture PCM or pointer contents. |
| Matcher diagnostics and memory | 5d222ae/c320b10/90e2618 bounded snapshots and rejection/ownership explanations; recording_memory.h and tag_memory_plan.h | Existing queue and capture limits are not evidence of equivalent accounting for the new retained-recording model. Port useful diagnostics alongside the lifecycle changes they describe. |
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

## Implementation order

1. Separate canceled/publication outcomes; add busy-file recovery with bounded
   ownership, coherent artifacts, exact destination policy and cancellation tests.
2. Add bounded stream evidence and replay to help validate lifecycle changes;
   continue the pinned native scheduler/identity audit.
3. Complete native recording identity, podcast/completeness and independent
   association before enabling native MP3 or unmatched publication.
4. Finish artwork/saved-metadata publication policy and integration.
5. Evaluate newer-client CEF and optional telemetry capabilities separately.
6. Run full CI and independent MinGW package verification for production changes;
   then use the existing separate live acceptance, merge and release gates.

This review changes no runtime defaults, installed files or staged live-acceptance
package. The cancellation correction is a source change requiring its own CI
checkpoint; earlier package evidence does not validate the new source.

Follow-up: source fed216eb6f04db77590cbf7a1fd74bea5be4f1f3 passed all four
jobs in CI 206 (44504382147) and independent MinGW package verification.
The cancellation correction is now CI/package verified; a live settings-change
race remains untested. See MILESTONES.md for hashes, the dependency-download
timeout/rerun and GCC fixture compatibility fixes. No automatic busy-file
retry, native MP3 capture or newer-client pointer chain was enabled.
