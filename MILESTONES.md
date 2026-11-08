# Engineering milestones

Updated November 8, 2026. RC86 passed all four jobs in CI 42475271762 and
independent artifact verification at source 2d5e9f0cc9f90d47acdb7f7f1a8ff618324535ca.
It is a checkpoint, not completion of all work. RC87 adds supervised artwork
validation to the packaged helper; its CI/package verification is pending.

| Milestone | State | Completion evidence still required |
| --- | --- | --- |
| RC63-74 selective reliability work | CI/package checkpoints complete | Pinned-client acceptance remains separate |
| Converter error reporting | RC86 CI/package verified | Native and both PowerShell versions reject zero-exit stderr errors and pipe-filling diagnostics in encoding/validation. A real damaged MP3 is rejected even when its wrapper masks the decoder exit status. All four CI jobs and independent MinGW package verification passed; live acceptance remains |
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

## Offline artwork decoding checkpoint

The bounded PNG decoder passed all four jobs in CI 42481391151 at source
790de51ab4662c57c751320ecf55af210c786415. Independent package verification
matched source/merge trees, scripts, embedded Classic, PE imports, hashes and
helper binding. The DLL, launcher and helper remain identical to CI190. The
package SHA-256 is b5b4378b9ef28b66cbe96452dac1f4687d47a80f26d79fe9759785bbea5633c5.
Ninety PNG layouts and 101 malformed cases run in each job. The preceding run
failed on missing math-library linkage; this run includes the corrected link.

JPEG now has a bounded decoder and regression corpus. The standalone decoder
passed all four jobs in CI 42483537187 at source
4cb2f1dc09dd8965d741b017db26b61f7ba6438e. Each job covers 22 valid/scan-boundary
and 1722 malformed/resource-limit cases. Independent package verification passed;
the DLL, launcher and audio helper match CI190. Both image decoders still need
verified supervised-worker integration and source/replacement policy.
Progressive JPEG scan-boundary probes demonstrate that valid decoding alone
cannot prove original download completeness. No production
artwork replacement or saved-recording metadata refresh is enabled.

RC87 integrates both image decoders into the build-pinned helper with a distinct
32-byte image reply. The shared supervisor checks the complete reply, retains
source/worker locking and handle restrictions, and enforces cancellation and a
deadline. Artwork has a 4 MiB input cap and 128 MiB process limit in addition to
each decoder's 32 MiB allocation quota. Local Windows diagnostic tests cover
valid/malformed images and supervisor faults; existing audio supervisor tests
also pass. CI and independent MinGW package verification are pending.

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
All four jobs passed in CI 42474262462 at source
30281a92f15cb52166b754c98eaf2f1104e3cd97. Each job ran the 36 original
fixtures, 18 protected PCM-equivalence cases and 72 side-information mutations;
the two sanitizer campaigns also completed 40,000 seeded iterations in total.
Independent MinGW package verification matched the source/merge tree, scripts,
hash manifest, helper binding and PE dependencies. The DLL, launcher and helper
are byte-identical to the original RC85 binaries. Package SHA256:
3af36c5c4b03bb236f59c4fbc892c50dfad409bacfb013d60db6dbdbd095b8ad.

Primary implementation cross-check: FFmpeg commit
cc583787bbecd4914fee7c2bf7f09463113822fe, libavcodec/mpegaudiodec_template.c,
handle_crc and Layer III side-information parsing. Retained source SHA256:
ebc2a30146f84bb7bc8cb67ea28cc2af444cdfebd22e4013adbeab6ecf9a5430.
These additions remain offline; no native MP3 capture or production audio
validation was enabled.

## MP3 decoder candidate audit

On November 8, 2026, minimp3 commit
ea99364f61c14656440e8d77e9c233ccf3124633 was evaluated locally, outside the
shipping build. Header SHA256:
57e437c5c1f0e8b243885d3929c8973b5e6c778451e0100ab4251d19915cb3ad.
It is not sufficient as a strict validator through its unmodified public API.

A real MPEG-1 mono encoding was mutated at frame 10, granule zero: the declared
part2_3 length was set to zero while scalefac_compress was set to 15. Its long
block requires 74 scalefactor bits before Huffman data, so the declaration is
inconsistent. The RC86 structural inspector accepts it. minimp3
consumes all 59 frames and returns all 67,968 nominal samples without an empty
decode or skipped frame, just as for the original. Local FFmpeg 9.0.2 also
returns zero without error-level diagnostics, but produces different PCM.
This is a separate limitation from RC86's handling of reported decoder errors.

Source inspection confirms L3_decode has no error return for scalefactor
overrun; L3_huffman restores the declared granule bit position. Counting
decoded samples and forbidding resynchronization cannot establish strict
payload validity. Do not connect this candidate to production validation
without explicit semantic rejection and independent malformed-stream tests.
The reproducible local candidate audit and fixture hashes are preserved in
outputs/Soggfy-MP3-decoder-candidate-audit.json, with the harness under work/.
No decoder dependency, native MP3 publication, or new runtime hook was added.

## MP3 scalefactor-bound follow-up

The offline inspector now rejects granules whose declared part2_3 bit budget
cannot contain their scalefactors. It accounts for all MPEG-1 compression
values and second-granule reuse flags; short blocks do not use those flags.
MPEG-2/2.5 ordinary and intensity-stereo compression ranges use the appropriate
long, short or mixed partition sizes. Intensity coding applies only to the
right channel in joint stereo. No compressed audio is decoded by this check.

Local cross-checking against actual scalefactor consumption in the pinned
minimp3 source passed all 20,480 version/compression/reuse/block/intensity
combinations. Synthetic frame tests exercise exact and one-bit-short budgets,
CRC/non-CRC layouts, every granule/channel and intensity mode isolation.
The real-corpus regression adds 72 impossible scalefactor declarations across
all MPEG versions, mono/stereo and CBR/VBR, while preserving acceptance of the
36 original files and 18 protected PCM-equivalence fixtures. All four jobs
passed in CI 42476996697 at source 2e5e81fd7e6e2b1125b8f2ee16032e4110cd9c53.
Independent MinGW package verification matched the source/merge tree, scripts,
manifest, helper binding and PE dependencies. DLL, launcher and helper are
byte-identical to CI 42475271762. Package SHA256:
03103325ee0a094bc627d6e59bd43773419ced1c5b0f7822bc2eed29286549fb.
Full Huffman semantics, gapless
timing, completeness and capture lifetime remain open; production MP3 capture
is still disabled.

## MP3 Xing/Info extent follow-up

The offline inspector now bounds first-frame Xing/Info optional fields and
compares declared audio-frame and stream-byte counts with observed extents.
Counts exclude ID3 and include the seek frame only in the byte extent. The
seek frame is excluded from reservoir history; the first audio frame cannot
borrow its metadata bytes. Protected seek frames retain the fixed Xing offset
used by LAME and still require a valid MPEG header/side-information CRC.

Local synthetic cases cover all 16 flag combinations, optional-field bounds,
all MPEG versions/channel layouts, protected frames and fixed-block input.
Another 36 real tagged encodings cover mono/stereo and CBR/VBR at all nine
supported sample rates. They pass independent packet-count comparisons, 360
count/truncation/reservoir rejection cases, ID3 exclusion and protected seek
header PCM-equivalence checks. All four jobs passed in CI 42477717796 at source
9b45a527956813bb9b5dbe704396a444691af8bd. Independent package verification
matched the source/merge tree, scripts, manifest, helper binding and PE
dependencies. Package SHA256:
c18a876a3fc7e37f58173f308b9b92b1a6d46ef23e79973a809210e3b9151db2.

These are consistency checks on untrusted declarations. A stripped or rewritten
tag cannot establish completeness; absent counts remain unknown. Nominal sample
counts still include physical metadata frames and are not presentation timing.
Encoder delay/padding, LAME extension CRC, VBRI, iTunSMPB, full Huffman semantics,
provenance and capture lifecycle remain separate work. No native capture enabled.
Primary cross-checks: FFmpeg cc583787bbecd4914fee7c2bf7f09463113822fe,
libavformat/mp3dec.c and mp3enc.c; LAME libmp3lame/VbrTag.c, lame_get_lametag_frame.

## Gapless tag compatibility audit

The local FFmpeg 9.0.2 audit generated 36 real Xing/Info fixtures across all
nine sample rates, mono/stereo and CBR/VBR. In every fixture, declared audio
frames times samples-per-frame minus encoder delay and padding matches the
independently decoded PCM sample count. This does not establish trustworthy
delay/padding for native capture or files with changed metadata.

Only six fixtures match a LAME tag CRC calculated from the MPEG header through
the byte immediately before the stored tag CRC. All 36 match the fixed
190-byte calculation in pinned FFmpeg mp3enc.c (CRC placeholder zeroed when
inside that extent, and zero padding beyond a short seek frame). The six
matching both calculations are MPEG-1 stereo; the other layouts place the
CRC earlier. Requiring just the LAME field-boundary checksum would reject
these 30 otherwise decodable local FFmpeg fixtures. No permissive production
fallback has been introduced; extension compatibility remains explicit work.

Evidence and fixture hashes: outputs/Soggfy-MP3-gapless-tag-audit.json.
Harness: work/probe_mp3_gapless.py; CRC oracle includes the CRC-16/ARC standard
check vector. Pinned mp3enc.c SHA256:
8dec50d701aafec3b1d81fb47ad1a9e3144938cbcf776a5f5633d0b091883f33.

## Independent decode fixture correction

Positive FFmpeg fixture generation, probing and decoding must now produce no
error-level diagnostics, even with a zero exit code. Both MP3 and metadata
refresh harnesses exercise a zero-exit error process to check this requirement.
Native test programs retain their own pass/fail assertions; debug libFLAC may
print normal rewind messages while they exercise expected failure cases.

This exposed malformed artwork in the real metadata fixtures: the synthetic
"opaque" picture comment produced an FFmpeg error even though decoded audio
matched. Real fixtures now contain a generated valid 1x1 PNG, independently
decoded to the expected RGB pixel before and after refresh. Malformed artwork
preservation remains covered by synthetic container tests. All 12 real Ogg/FLAC
cases pass locally through isolated validation and preparation, with identical
PCM, preserved artwork pixels and clean FFmpeg diagnostics. The complete MP3
corpus also passes the stronger harness. CI run 42479107718 passed native,
Windows and both sanitizer jobs. Source 4bd96eccaf1dd3840cf602e1b068062c410fccec
and CI merge e8e0e63a209a9a144f40dcf14823ab76c1082277 share tree
1bc28a14d7f0e18cc7a9dae15ed7da1209322609. Independent package verification
passed for SHA256
74c1bee5a5640646e0558e4ddef8dea7f342db5fb79b2e6790a57bbf821030ec.
DLL, launcher and helper are byte-identical to CI190. These are fixture/evidence
corrections; runtime validation and the disabled metadata refresh coordinator
have not changed. CI193 exposed a harness mistake: requiring clean stderr from
native debug programs rejected normal libFLAC rewind messages. CI194 fixes that
by applying the clean-diagnostic requirement to every FFmpeg operation only.
