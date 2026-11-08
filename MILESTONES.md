# Engineering milestones

Updated November 8, 2026. RC82 helper build/package/installer integration passed
all four jobs in CI 42456886343 and independent artifact verification.
It is a checkpoint, not completion of all work.

| Milestone | State | Completion evidence still required |
| --- | --- | --- |
| RC63–74 selective reliability work | CI/package checkpoints complete | Pinned-client acceptance remains separate |
| Installation-directory transaction | RC75 CI/package verified | Live installation acceptance |
| Manual watch scan cost | RC75 CI/package verified | Live acceptance |
| Native playlist save result | RC75 CI/package verified | Live save dialog |
| Native playback/decoder identity | Static discovery compatible | Playback-ID ABI, lifetime, integration and transition fixtures |
| Ad/DJ/interstitial isolation | Open | Trustworthy identity plus transition/live evidence |
| Native podcast MP3 capture | Structural prototype | Bounded MPEG-1/2/2.5 layout and ID3 extents; 36 real mono/stereo CBR/VBR fixtures locally verified. Payload/reservoir/gapless/completeness validation and lifecycle integration remain; no production capture enabled |
| Concurrent capture/cross-format association | RC84 shared-candidate rejection implemented; local regression passed | Batch check rejects Ogg/FLAC candidates shared by queued listens before either is consumed. Full CI/package checks, native identity-aware lifecycle, future-listen/preload and live transition evidence remain |
| Playback metadata refresh/instance-bound skip | Partial | RC78 queue fencing and RC79 exact-instance active/final/pending metadata refresh CI/packages verified; native current-track skip remains open |
| Saved-recording metadata refresh/preservation | Offline preparation and RC82 helper deployment verified | Inspection, isolated Ogg/FLAC decoding, minimal text/lyrics policy, output verification and preparation passed CI. RC82 DLL-bound helper packaging and installation passed CI/artifact verification. Local probes demonstrate concurrent ADS, attribute and DACL mutation despite the source lock. Filesystem metadata preservation, artwork replacement policy and production coordinator remain; see METADATA_REFRESH_DESIGN.md |
| Log levels 0/1/2/4 and bounded lanes | RC78 CI/package verified | RC77 test collision corrected in RC78; live acceptance remains separate; both lanes share one disk worker |
| Deterministic sanitizer/fuzz campaign | RC76 campaigns passed | 40,000 seeded iterations plus regressions; does not prove live callback safety |
| NetworkPatch policy | RC83 implemented; local Windows fixtures passed | Explicit default-off startup opt-in, strict bounded INI parsing, preserved external edits and disabled IAT bypass. Upgrade behavior documented. Full CI/package verification and pinned-client acceptance remain |
| Pinned Spotify live acceptance/stalls | Separate gate | Authorized exact-artifact restart and walkthrough |
| Merge/official release | Separate gate | Review, acceptance and authorization |

Preserve Classic UI, file-based status, Unicode/multiple artists, custom/legacy/
podcast paths, cached PlayerAPI, per-entry UIDs, 1x–30x, Downloads-off effective
1x with selected speed retained, MP3 320 kbps and strict Ogg/FLAC validation.
Only MinGW CI binaries ship. Preserve earlier packages and source archives.
