# Engineering milestones

Updated November 8, 2026. RC75 is a checkpoint, not completion of all work.

| Milestone | State | Completion evidence still required |
| --- | --- | --- |
| RC63–74 selective reliability work | CI/package checkpoints complete | Pinned-client acceptance remains separate |
| Installation-directory transaction | Implemented, local fault fixtures pass | RC75 MinGW CI/package verification |
| Manual watch scan cost | Implemented, queue and real-FFmpeg fixtures pass | RC75 CI/package verification |
| Native playlist save result | Implemented, JS/native fixtures pass | RC75 CI/package verification; live save dialog |
| Native playback/decoder identity | Static discovery compatible | Playback-ID ABI, lifetime, integration and transition fixtures |
| Ad/DJ/interstitial isolation | Open | Trustworthy identity plus transition/live evidence |
| Native podcast MP3 capture | Open | Parser, complete validation, lifecycle integration |
| Concurrent capture/cross-format association | Partial | Identity-aware lifecycle and conflicting capture tests |
| Playback metadata refresh/instance-bound skip | Open | Classic reservation/status integration and stale-result tests |
| Log levels 0/1/2/4 and bounded lanes | Open | Settings integration, concurrency and queue isolation tests |
| Deterministic sanitizer/fuzz campaign | Open | Reproducible bounded campaigns across capture/identity boundaries |
| NetworkPatch policy | Separate decision | Explicit opt-in design; no silent default change |
| Pinned Spotify live acceptance/stalls | Separate gate | Authorized exact-artifact restart and walkthrough |
| Merge/official release | Separate gate | Review, acceptance and authorization |

Preserve Classic UI, file-based status, Unicode/multiple artists, custom/legacy/
podcast paths, cached PlayerAPI, per-entry UIDs, 1x–30x, Downloads-off effective
1x with selected speed retained, MP3 320 kbps and strict Ogg/FLAC validation.
Only MinGW CI binaries ship. Preserve earlier packages and source archives.
