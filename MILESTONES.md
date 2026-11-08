# Engineering milestones

Updated November 8, 2026. RC85 passed all four jobs in CI 42461370789 and
independent artifact verification at source 89abeba4c351c21358184e741fe72ddc22955672.
It is a checkpoint, not completion of all work.

| Milestone | State | Completion evidence still required |
| --- | --- | --- |
| RC63-74 selective reliability work | CI/package checkpoints complete | Pinned-client acceptance remains separate |
| Installation-directory transaction | RC75 CI/package verified | Live installation acceptance |
| Manual watch scan cost | RC75 CI/package verified | Live acceptance |
| Native playlist save result | RC75 CI/package verified | Live save dialog |
| Native playback/decoder identity | Static candidate traced through consumer, configured generator, recovery invocation and teardown | Null fallback places the upstream read outside its allocation; a traced non-null path spans separate byte fields and part of an ownership pointer. A separate candidate has playback-ID consumer, hex-encoding and configured 16-byte generator evidence. Audio-driver recovery has a conditional synchronous replacement-callback path; teardown replaces a shared byte flag and conditionally destroys the native object when its strong count reaches zero. One callback checks the token before using its stored pointers; its clone retains that guard and is built alongside a copy of the candidate ID. The dispatch-provider pointer is traced to its service-table entry. Configuration transfer moves the same guarded wrapper and cleans it after dispatch unless the receiver consumes it; 752 pinned instruction checks pass. Concrete receiver consumption, other consumers, callback completion, actual runtime path, lifetime, synchronization, integration and transitions remain unverified; see NATIVE_IDENTITY_AUDIT.md |
| Ad/DJ/interstitial isolation | Open | Trustworthy identity plus transition/live evidence |
| Native podcast MP3 capture | Structural, CRC and reservoir-extent prototype | Bounded MPEG-1/2/2.5 layout, ID3 extents and reservoir bit-range accounting; 36 real mono/stereo CBR/VBR fixtures and missing-prefix rejection locally verified. All four jobs passed in CI 42469354367 after using the runner MSYS2 installation (the preceding run hit HTTP 504 during setup). Native and both sanitizer jobs exercised the 36 real fixtures; each sanitizer campaign completed 20,000 seeded iterations. The MinGW package was independently verified at source f34da7d7b94c43dea22463a3302db9babc3e4d09; its DLL and validation helper match the original RC85 binaries. Protected-frame CRC and selected side-information constraints now pass local independent-decoder checks (follow-up below). Full compressed-payload and remaining side-information semantics, gapless/completeness validation and lifecycle integration remain; no production capture enabled |
| Concurrent capture/cross-format association | RC84 shared-ready rejection and RC85 retention-conflict fix CI/package verified | Late first retention now rejects candidates shared by queued listens, including competing ready captures. Native identity-aware lifecycle, future-listen/preload and live transition evidence remain |
| Playback metadata refresh/instance-bound skip | Partial | RC78 queue fencing and RC79 exact-instance active/final/pending metadata refresh CI/packages verified; native current-track skip remains open |
| Saved-recording metadata refresh/preservation | Offline preparation and RC82 helper deployment verified | Inspection, isolated Ogg/FLAC decoding, minimal text/lyrics policy, output verification and preparation passed CI. RC82 DLL-bound helper packaging and installation passed CI/artifact verification. Local probes demonstrate concurrent ADS, attribute and DACL mutation despite the source lock. Same-file stream swaps preserve tested metadata but expose an empty default stream; even a reader can block publication and rollback. That approach remains disabled. Artwork envelope inventory is implemented offline; filesystem metadata preservation, full artwork decoding/replacement policy and production coordinator remain; see METADATA_REFRESH_DESIGN.md |
| Log levels 0/1/2/4 and bounded lanes | RC78 CI/package verified | RC77 test collision corrected in RC78; live acceptance remains separate; both lanes share one disk worker |
| Deterministic sanitizer/fuzz campaign | RC76 campaigns passed | 40,000 seeded iterations plus regressions; does not prove live callback safety |
| NetworkPatch policy | RC83 and RC84 CI/package verified | Explicit default-off startup opt-in, strict bounded INI parsing, preserved external edits and disabled IAT bypass. Upgrade behavior documented. Pinned-client acceptance remains |
| Pinned Spotify live acceptance/stalls | Separate gate | Authorized exact-artifact restart and walkthrough |
| Merge/official release | Separate gate | Review, acceptance and authorization |

Preserve Classic UI, file-based status, Unicode/multiple artists, custom/legacy/
podcast paths, cached PlayerAPI, per-entry UIDs, 1x-30x, Downloads-off effective
1x with selected speed retained, MP3 320 kbps and strict Ogg/FLAC validation.
Only MinGW CI binaries ship. Preserve earlier packages and source archives.

## MP3 protected-frame follow-up

The offline Layer III inspector now validates protected-frame CRC over the
last 16 header bits and complete side information. It rejects coefficient-pair
counts above 288 and switched blocks with type zero. Main audio bytes remain
outside the CRC coverage; passing this inspection does not establish valid
Huffman data, complete playback coverage, gapless timing or native ownership.

Local diagnostic C++ boundary tests and the 36 existing MPEG-1/2/2.5 real
mono/stereo CBR/VBR fixtures pass. Another 18 reservoir-disabled real encodings
are reframed at a larger bitrate to insert CRC and stuffing without removing
compressed bytes. FFmpeg with crccheck+explode independently accepts them and
decodes identical PCM. Header, stored CRC and side-info corruption in middle
frames are rejected by both implementations. Another 72 real-stream mutations
exercise the two side-information constraints with matching rejection causes.
Synthetic tests cover every granule/channel record and every protected bit.
CI and independent MinGW package verification for this follow-up remain pending.

Primary implementation cross-check: FFmpeg commit
cc583787bbecd4914fee7c2bf7f09463113822fe, libavcodec/mpegaudiodec_template.c,
handle_crc and Layer III side-information parsing. Retained source SHA256:
ebc2a30146f84bb7bc8cb67ea28cc2af444cdfebd22e4013adbeab6ecf9a5430.
These additions remain offline; no native MP3 capture or production audio
validation was enabled.
