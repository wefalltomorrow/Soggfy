# Engineering milestones

Updated November 8, 2026. RC79 is a checkpoint, not completion of all work.

| Milestone | State | Completion evidence still required |
| --- | --- | --- |
| RC63–74 selective reliability work | CI/package checkpoints complete | Pinned-client acceptance remains separate |
| Installation-directory transaction | RC75 CI/package verified | Live installation acceptance |
| Manual watch scan cost | RC75 CI/package verified | Live acceptance |
| Native playlist save result | RC75 CI/package verified | Live save dialog |
| Native playback/decoder identity | Static discovery compatible | Playback-ID ABI, lifetime, integration and transition fixtures |
| Ad/DJ/interstitial isolation | Open | Trustworthy identity plus transition/live evidence |
| Native podcast MP3 capture | Open | Parser, complete validation, lifecycle integration |
| Concurrent capture/cross-format association | Partial | Identity-aware lifecycle and conflicting capture tests |
| Playback metadata refresh/instance-bound skip | Partial | RC78 queue fencing verified; RC79 exact-instance active/final/pending metadata refresh implemented, CI/package pending; native current-track skip remains open |
| Log levels 0/1/2/4 and bounded lanes | RC78 CI/package verified | RC77 test collision corrected in RC78; live acceptance remains separate; both lanes share one disk worker |
| Deterministic sanitizer/fuzz campaign | RC76 campaigns passed | 40,000 seeded iterations plus regressions; does not prove live callback safety |
| NetworkPatch policy | Separate decision | Explicit opt-in design; no silent default change |
| Pinned Spotify live acceptance/stalls | Separate gate | Authorized exact-artifact restart and walkthrough |
| Merge/official release | Separate gate | Review, acceptance and authorization |

Preserve Classic UI, file-based status, Unicode/multiple artists, custom/legacy/
podcast paths, cached PlayerAPI, per-entry UIDs, 1x–30x, Downloads-off effective
1x with selected speed retained, MP3 320 kbps and strict Ogg/FLAC validation.
Only MinGW CI binaries ship. Preserve earlier packages and source archives.
