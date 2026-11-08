# Engineering milestones

Updated November 8, 2026. RC85 passed all four jobs in CI 42461370789 and
independent artifact verification at source 89abeba4c351c21358184e741fe72ddc22955672.
It is a checkpoint, not completion of all work.

| Milestone | State | Completion evidence still required |
| --- | --- | --- |
| RC63Ã¢â‚¬â€œ74 selective reliability work | CI/package checkpoints complete | Pinned-client acceptance remains separate |
| Installation-directory transaction | RC75 CI/package verified | Live installation acceptance |
| Manual watch scan cost | RC75 CI/package verified | Live acceptance |
| Native playlist save result | RC75 CI/package verified | Live save dialog |
| Native playback/decoder identity | Static candidate traced through consumer, configured generator, recovery invocation and teardown | Null fallback places the upstream read outside its allocation; a traced non-null path spans separate byte fields and part of an ownership pointer. A separate candidate has playback-ID consumer, hex-encoding and configured 16-byte generator evidence. Audio-driver recovery has a conditional synchronous replacement-callback path; teardown replaces a shared byte flag and conditionally destroys the native object when its strong count reaches zero. One callback checks the token before using its stored pointers; its clone retains that guard and is built alongside a copy of the candidate ID. Later dispatch, other consumers, callback completion, actual runtime path, lifetime, synchronization, integration and transitions remain unverified; see NATIVE_IDENTITY_AUDIT.md |
| Ad/DJ/interstitial isolation | Open | Trustworthy identity plus transition/live evidence |
| Native podcast MP3 capture | Structural and reservoir-extent prototype | Bounded MPEG-1/2/2.5 layout, ID3 extents and reservoir bit-range accounting; 36 real mono/stereo CBR/VBR fixtures and missing-prefix rejection locally verified. CI pending for reservoir changes. Full payload/side-information semantics, CRC, gapless/completeness validation and lifecycle integration remain; no production capture enabled |
| Concurrent capture/cross-format association | RC84 shared-ready rejection and RC85 retention-conflict fix CI/package verified | Late first retention now rejects candidates shared by queued listens, including competing ready captures. Native identity-aware lifecycle, future-listen/preload and live transition evidence remain |
| Playback metadata refresh/instance-bound skip | Partial | RC78 queue fencing and RC79 exact-instance active/final/pending metadata refresh CI/packages verified; native current-track skip remains open |
| Saved-recording metadata refresh/preservation | Offline preparation and RC82 helper deployment verified | Inspection, isolated Ogg/FLAC decoding, minimal text/lyrics policy, output verification and preparation passed CI. RC82 DLL-bound helper packaging and installation passed CI/artifact verification. Local probes demonstrate concurrent ADS, attribute and DACL mutation despite the source lock. Filesystem metadata preservation, artwork replacement policy and production coordinator remain; see METADATA_REFRESH_DESIGN.md |
| Log levels 0/1/2/4 and bounded lanes | RC78 CI/package verified | RC77 test collision corrected in RC78; live acceptance remains separate; both lanes share one disk worker |
| Deterministic sanitizer/fuzz campaign | RC76 campaigns passed | 40,000 seeded iterations plus regressions; does not prove live callback safety |
| NetworkPatch policy | RC83 and RC84 CI/package verified | Explicit default-off startup opt-in, strict bounded INI parsing, preserved external edits and disabled IAT bypass. Upgrade behavior documented. Pinned-client acceptance remains |
| Pinned Spotify live acceptance/stalls | Separate gate | Authorized exact-artifact restart and walkthrough |
| Merge/official release | Separate gate | Review, acceptance and authorization |

Preserve Classic UI, file-based status, Unicode/multiple artists, custom/legacy/
podcast paths, cached PlayerAPI, per-entry UIDs, 1xÃ¢â‚¬â€œ30x, Downloads-off effective
1x with selected speed retained, MP3 320 kbps and strict Ogg/FLAC validation.
Only MinGW CI binaries ship. Preserve earlier packages and source archives.
