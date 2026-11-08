# Saved-recording metadata refresh

This is unfinished engineering work, separate from RC79 playback snapshots.
The current publication path skips equal/higher-quality native recordings before
constructing tags. It can retry conversion but does not refresh the saved tags.

Current status (November 8, 2026): the isolated decoder helper has shipped in
verified MinGW packages since RC82. Inspection, planning, retag verification
and preparation have passed CI, but the saved-file refresh coordinator remains
disabled. Filesystem metadata preservation under concurrent mutation and full
artwork replacement policy remain open. See MILESTONES.md for current package
checkpoints; the sections below retain the implementation and failure evidence.

CI194 additionally verifies clean FFmpeg diagnostics and exact preservation of
the generated 1x1 red artwork pixel in all twelve real Ogg/FLAC fixtures. The
original real fixtures accidentally reused an opaque synthetic picture comment;
FFmpeg reported an error despite matching decoded audio. Synthetic opaque-art
preservation tests remain separate. This strengthens test evidence without
changing production artwork policy or enabling saved-file refresh.

A local Windows PNG decoder probe on November 8, 2026 shows why full pixel
decoding alone is insufficient. The explicit WIC PNG decoder successfully
returned pixels for bad IHDR/IDAT/IEND CRCs, missing IEND, duplicate IHDR,
unknown critical chunks, noncontiguous IDAT, extra or short scanlines, an invalid
filter, trailing compressed data and a bad zlib Adler checksum. It also accepted
27 of the 69 truncated prefixes of a generated 69-byte PNG; some returned
incorrect black pixels without failure. These are observations of this local
decoder, not promises about other Windows versions. Do not use successful WIC
CopyPixels as proof of complete, valid artwork. The fixture hashes, exit codes
and pixel outputs are preserved in outputs/Soggfy-WIC-PNG-probe.json; the local
diagnostic source and generator are work/wic_png_probe.cpp and
work/probe_wic_png.py. No local diagnostic executable ships.

The resulting offline PNG validator uses pinned libpng 1.6.59 and zlib 1.3.2
sources, with unmodified upstream files, licenses and SHA-256 manifests under
native/vendor. It first checks bounded chunk framing, every CRC, static-image
ordering and exact IEND termination; it then decodes every row/pass with strict
libpng CRC/error/warning handling, palette-index checks and exact input use.
APNG is unsupported. Limits are 4 MiB input, 8192 per dimension, 16 Mi pixels,
4096 chunks and 32 MiB of tracked decoder allocations, including allocation
headers. No full decoded image is retained. Rejected input clears the output.

Local diagnostic tests pass 90 generated layouts spanning legal PNG color/bit
depth combinations and Adam7/noninterlaced images, independently decoded by
FFmpeg to expected pixels. They also cover all five filters, ancillary chunks,
split IDAT, chunk-count boundaries, and 101 malformed inputs (including every
truncated prefix, excess/short pixel data, CRC/Adler damage, invalid palette
indexes and individual/aggregate compressed metadata expansion). All four jobs
passed in CI 42481391151, including both ASan/UBSan campaigns. Independent
package verification passed at source 790de51ab4662c57c751320ecf55af210c786415;
the DLL, launcher and deployed helper match the earlier CI190 binaries.
Full static PNG decoding is a foundation only: image-source provenance and
replacement eligibility remain open. RC87 adds supervised worker integration
as described below; the saved-file refresh coordinator remains disabled.

The offline JPEG validator uses pinned libjpeg-turbo 3.2.1 commit
7b4993786d856cbccabfc0ed2f923ba000e326cc. It supports single-frame 8-bit
Huffman sequential/progressive images. Marker framing, dimensions, explicit
Huffman tables at each scan and exact EOI termination are checked before
decoding every row with fatal errors/warnings. It does not use the decoder's
implicit MJPEG Huffman-table fallback. Arithmetic, lossless and higher-precision
JPEG are unsupported. APP/COM data stays opaque; EXIF/ICC validity is not proved.
Limits are 4 MiB input, 8192 per dimension, 16 Mi pixels, 4096 markers, 128 scans
and 32 MiB of tracked decoder allocations including allocation headers. A
per-decoder system allocator caps the initial control block and all decode pools;
the ordinary max_memory_to_use setting alone is not a total allocation cap.
No backing files or full decoded image are retained. Failure clears the result.
The fixed-size context and caller-owned encoded input are outside the reported
decoder allocation count. RC87 supplies CPU deadlines and cancellation through
the same supervised worker used for audio validation.

Local diagnostics accepted six valid encodings and rejected 1695 malformed or
truncated variants. An allocation probe passed 334 quota/persistent-failure
cases with no outstanding tracked allocations. A 98,940-byte progressive image
required 50,660,371 tracked bytes with a larger diagnostic quota, and was rejected
under 32 MiB; the equivalent sequential image fit in 323,638 bytes. These figures
are local probe observations, not peak total-process memory guarantees.

Fourteen progressive images ended after earlier complete scans were accepted by
both strict libjpeg and FFmpeg without diagnostics, yet decoded differently from
the originals. Thus successful JPEG decoding cannot establish original scan
coverage or source provenance, and must not authorize artwork replacement by
itself. Evidence is saved in outputs/Soggfy-JPEG-*-probe.json; diagnostic sources
remain under work. The committed corpus includes these valid shorter images,
malformed/truncated inputs, marker/input/dimension limits and a progressive image
that exceeds the allocation quota. The standalone JPEG implementation passed
all four jobs in CI 42483537187 at source
4cb2f1dc09dd8965d741b017db26b61f7ba6438e, with 22 valid/scan-boundary cases
and 1722 malformed/resource-limit cases in each job. Independent package
verification passed; its DLL, launcher and audio helper match CI190.
RC87 links JPEG and PNG into the packaged worker, with their licenses,
but not into the DLL. No replacement policy or saved-file refresh coordinator
has been enabled.

## Artwork source and replacement eligibility

The current source is the Windows media-session thumbnail in
native/media_session.cpp. Artwork() bounds its reported size to 4 MiB, requires
an exact-length read and recognizes PNG/JPEG magic before copying the bytes.
MediaReader caches that cover by Media::Key(), which combines title, artist
and album. That key is not a Spotify resource identity: distinct recordings
can have the same display fields. The thumbnail result has no stable source
identifier or evidence that an existing picture belongs to Soggfy.

Consequently, a successful worker decode only establishes the supported image
structure and decoded properties. Neither it, a larger resolution, nor a
matching display-field key authorizes saved-artwork replacement. A content hash
could identify equal bytes but would not establish source ownership or freshness.
The current refresh planner ignores incoming artwork, and output verification
requires exact preservation of existing picture comments and FLAC picture
blocks. Empty, opaque, custom and repeated picture entries retain that policy.

A future replacement policy needs a resource identity tied to the recording,
an identified artwork source and explicit eligibility for the old image, plus
validation of the proposed image. Missing provenance must preserve existing
artwork. Adding artwork to a file with no picture still needs trustworthy source
association; the title/artist/album cache alone cannot supply it. This is separate
from the existing new-recording tagger and does not enable saved-file mutation.

## Supervised artwork validation checkpoint

RC87 passed all four jobs in CI 42485859840 at source
4376d13fbf893b1fb252101e047195570592cd3c. Independent MinGW package verification
passed for SHA-256 5236268bc1cd3f2c478fb151506de95198c975faa8d220f26506e6eb7260201f.
This verifies decoder/supervisor deployment, not saved-file replacement policy.

RC87 uses the existing same-build SHA-256-bound worker and held trust object.
PNG/JPEG requests have a separate 32-byte version-2 reply containing format,
dimensions, depth, channels and progressive mode. Audio keeps its original
40-byte version-1 reply. Each caller rejects formats from the other domain;
the common supervisor checks exact reply length and all relevant field bounds.
No pixels, tags or paths cross the reply boundary. Decoding reads a read-only
mapping of the caller's held source file without changing its file position.

The worker rejects artwork above 4 MiB before mapping it. Its job permits one
process, caps process memory at 128 MiB, kills on closure, and is assigned while
the child is suspended. Only mapping and reply handles are inherited. The
existing cancellation/deadline loop applies to both audio and artwork. PNG and
JPEG retain their separate 32 MiB tracked decoder allocation limits. These are
resource and lifecycle controls, not a security sandbox or provenance proof.

Local diagnostic Windows tests cover PNG depth/channel variants, six pinned
JPEG variants, malformed/truncated input, empty and oversized input, the large
progressive allocation rejection, source locks/position, malformed replies,
handle isolation, memory limits, cancellation and timeout. Existing audio
supervisor fixtures pass against the enlarged helper. These checks also passed
in Windows CI. The independently verified CI helper subsequently passed the
same image corpus and Ogg/FLAC supervisor fixtures on the local Windows machine.
Only the diagnostic callers were built with Zig; the tested helper is the
unchanged MinGW artifact. Local Zig executables do not ship.

## Verified foundation

`vorbis_comments.h` supplies one bounded reader/writer for native Ogg/FLAC
comments. Retagging and recording-identity parsing share the reader. The writer
rejects comment counts above 100,000 and oversized bodies before allocating
the serialized result; previously taggers could emit a count that their own
readers rejected. Raw vendor text, field order, duplicates, casing, Unicode
and unknown custom fields survive the common representation. Container limits
remain enforced by each tagger. This validates metadata structure only.

RC80 passed native, Windows and both sanitizer jobs in CI run 42444451379,
followed by independent package verification. The source and CI merge share
tree fa7275cb67d840182c935a45cb20d89d1a29ca0f. The package/report is preserved.

## Text policy under development

`metadata_refresh_plan.h` proposes a minimal replacement set for the existing
taggers. It requires exact nonempty Spotify URI agreement and recognized
complete/native provenance markers, rejects conflicting album identities,
preserves populated/custom/repeated fields and all capture/quality evidence,
fills missing text, and refines compatible DATE precision with coherent YEAR.
Conflicting, invalid or ambiguous dates are retained without a date update.
Numeric track/disc values must be positive; candidate text must be valid UTF-8.
The planner checks input and prospective output limits, including the default
vendor inserted by the Ogg tagger. It returns no artwork replacement.

Local strict diagnostic policy and Ogg/FLAC round-trip tests pass. The latter
exercise the production taggers and check retained vendor/custom/repeated tags,
artwork, provenance, stream headers and audio bytes. Their synthetic payloads
test container preservation, not audio decoding. These regressions are now
registered in native, Windows and sanitizer scripts and passed the CI checks
recorded below. Saved-file publication is still not connected.

`metadata_refresh_inspection.h` now reads complete native-container metadata.
It checks strict quality headers, every Ogg page CRC, exact serial/sequence,
continuation, EOS and absence of trailing data, bounded header packets, unique
FLAC comment blocks, and exact identity/provenance agreement. Sample count,
rate and bit depth markers must agree with the container, including duplicates.
It clears stale output on failure. It does not decode compressed audio; an
apparently complete FLAC header with corrupt frames still requires rejection by
the future full-payload validator. No production publication caller uses it yet.

Local tests reject every truncated prefix of the synthetic Ogg fixture and
every incomplete FLAC metadata prefix, plus Ogg replay/gaps, wrong continuation,
serial changes, CRC errors, trailing data and conflicting provenance. The new
real-audio regression generates twelve FFmpeg fixtures (Ogg/FLAC, short and
longer durations, stereo/mono, tones/silence/noise, 22.05/44.1/48/96 kHz).
Original, initially tagged and refreshed files decode to byte-identical PCM
within each of the twelve cases. This test is required by native, Windows
and sanitizer CI scripts and passed the CI checks recorded below. Local Zig
builds remain diagnostic; only verified MinGW CI binaries ship.

The real Ogg fixtures exposed a coverage-validation issue with FFmpeg's
demux/decode path: the short 44.1 kHz fixtures decode to 128 fewer samples than
their final granules; the 22.05 kHz fixture also differs from its final granule.
Xiph's direct reference decoder returns the exact 44,100 samples for the same
one-second file. The Vorbis specification appendix A defines initial/final
trimming; libvorbis block.c explicitly treats a first-and-last audio page as
end trimming. Do not use an arbitrary tolerance around FFmpeg's decoded count.

`vorbis_validation.cpp` now prototypes full reference-codec synthesis for one
zero-origin native stream. It checks CRCs/sequence/continuation, bounded packet
sizes, all three codec headers, decoder return values, bitreader overrun, finite
PCM, decoded count at every known packet granule and final EOS coverage. Output
is cleared on failure/cancellation. The six real Ogg cases pass for original,
tagged and refreshed files. CRC-valid cases with missing/shortened audio packets
or inflated final granules are rejected, alongside truncation/gaps/cancellation.

The reference codec is pinned to Xiph commit
c2aa86b05e981c96bf381fc6aa11cdd03eccc2fb, copied unchanged with license and
per-file SHA256 hashes; libogg 1.3.5 bitwise.c supports it. This dependency and
validator were first linked into tests, then into the isolated helper deployed
in RC82. The helper enforces memory/time/cancellation limits; bounded compressed
input alone does not bound codec allocation or CPU. Production coordination
of stable-source supervision and publication remains unfinished.

`flac_validation.cpp` now prototypes full payload decoding with libFLAC 1.5.0,
pinned to Xiph commit 1507800de4b70e21be71f38caa0d9079d0bc6e45. It rejects every
decoder error, frame CRC failure, noncontiguous or mismatched sample coverage,
out-of-range PCM, trailing bytes and known MD5 mismatches. Zero MD5 is permitted
by the format; full frame CRC and coverage validation still apply. Tests cover
truncation, distributed byte mutations, declared coverage, CRC/MD5, appended
audio, cancellation and stale-output clearing. All twelve local real-audio
fixtures pass for original, tagged and refreshed files with identical PCM.
The codec C units use defined signed wrapping (`-fwrapv`) because corrupted
predictor residuals can overflow before CRC validation; our C++ code retains
ordinary signed-overflow checks. No sanitizer check is suppressed. Full CI
validation passed in run 42448323708 at dfd4131f3852856206a8630371e51c8823962ef0
(Linux, both ASan/UBSan seeds, Windows tests/build/package). Local diagnostic builds are not
shipping artifacts or a substitute for CI ASan/UBSan.

The preceding Vorbis CI run passed native tests and both sanitizer seeds after
fixing signed byte shifts in libogg's bit reader, verified by an independent
bit-by-bit oracle. Windows exposed a GCC array-bounds warning in a test fixture;
the fixture now allocates its final buffer size directly. The full Windows job
passed in run 42448323708, resolving that fixture failure.

The Windows validation-process prototype accepts a caller-held, locked source
handle and maps it read-only without moving its file position. Only the mapping
and result pipe are inherited. The child starts suspended and joins a job with
a 768 MiB committed-memory limit, one-process limit and kill-on-close before
running any decoder code. Input is limited to 256 MiB and wall time to 60 seconds;
larger files are conservatively ineligible for this prototype. Cancellation and
timeout terminate the job. A successful exit plus an exact 40-byte versioned
reply with nonempty coverage is required. Local tests cover real Ogg/FLAC files,
bad/short/extra replies, failed exits, hangs, memory exhaustion, excluded handles,
source position/locking and cancellation. This is resource isolation, not a
security sandbox. The caller must use a trusted same-build worker and retain the
source lock through a separately guarded transaction. RC82 packages the helper
and binds its digest to the DLL; the saved-file refresh caller is still disabled.

The standalone transaction prototype opens the original with read/delete access
and read-only sharing, locks each resolved local ancestor against rename, then
rechecks the resolved path's file identity. It stages and flushes complete bytes,
moves the original by its held handle and publishes by the staged handle, always
without replacement. Cancellation before mutation leaves the original in place;
cancellation/failure after the original move attempts rollback. If a concurrent
creator occupies the destination, neither publication nor rollback overwrites it,
and the result reports the preserved original's backup path. Rename/recovery
storage is allocated before mutation. Local tests cover these paths, directory
and file locks, single-use transactions and hard-link rejection. These passed
all four CI jobs in run 42449594271.
This prototype still needs filesystem security/stream/attribute preservation,
validated-delta integration and production
packaging. Network/redirected roots are conservatively rejected. Its callback
checks do not by themselves make settings-generation publication atomic.

Reference sources inspected:
- https://xiph.org/vorbis/doc/Vorbis_I_spec.html (appendix A)
- https://github.com/xiph/vorbis/blob/c2aa86b05e981c96bf381fc6aa11cdd03eccc2fb/lib/block.c
- https://github.com/FFmpeg/FFmpeg/blob/n8.0/libavformat/oggparsevorbis.c

### Filesystem preservation evidence (November 8, 2026)

A local Windows probe using the transaction's exact source access/share flags
confirmed that GENERIC_READ|DELETE plus FILE_SHARE_READ blocks default-stream
writes but permits another handle to modify an existing alternate data stream,
create a new alternate stream, and change file attributes. The audio bytes and
file identity remained unchanged. Source-handle stability therefore establishes
audio-byte stability, not a complete immutable filesystem-metadata snapshot.
The declaration in metadata_refresh_transaction.h states this distinction.

CopyFileEx succeeded while that source handle remained open and copied the
alternate-stream bytes. It did not preserve the source's protected DACL: the
copy inherited its destination-directory DACL. Comparing security descriptors
also reported a difference. The PowerShell Set-Acl attempt while locked failed;
this is evidence about that API path only, not proof that all security-descriptor
mutation is excluded by the lock. A direct DACL-only SetFileSecurityW call then
succeeded under the same held handle. Reading the DACL confirmed the newly added
probe ACE. Removing that ACE succeeded too; Windows normalized the descriptor
(ACE ordering/control bits), so restoration was not byte-for-byte identical.
The probe checks ACE removal and retained DACL protection separately. All probes
used isolated synthetic files. Both shells confirmed the native DACL mutation.

These results rule out treating CopyFileEx alone as the preservation step.
Reproduce with `tools/probe_refresh_file_preservation.ps1 -OutputDirectory <directory>`
under PowerShell 7 or Windows PowerShell 5.1. The diagnostic creates a unique
subdirectory containing synthetic files and result.json, and retains them for
inspection. Both shells produced the same stream/attribute/DACL results locally.
The implementation still needs an explicit metadata snapshot/copy/verification
contract, including concurrent stream creation and metadata changes. A check
only at Open or Stage cannot establish what exists at publication. BackupRead
also does not read SACLs unless the source handle has ACCESS_SYSTEM_SECURITY;
successful backup reads alone cannot prove complete security preservation.
Do not enable the production coordinator on the strength of audio/tag/PCM tests.
Default-stream oplocks alone are also insufficient: NTFS oplocks are per stream,
as are sharing modes. ReplaceFile documents preservation of DACLs and named
streams absent from the replacement, but it is not a drop-in for this transaction:
its pathname acquisition and failure states need a separate identity/rollback
contract, and its listed preservation guarantees do not cover all security data.

Relevant Windows API contracts:
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-copyfileexa
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-backupread
- https://learn.microsoft.com/en-us/windows/win32/fileio/file-streams
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilea

An additional isolated probe, `tools/probe_refresh_replace_file.ps1`, exercises
ReplaceFileW with each actual transaction handle independently held, and with
both handles absent. PowerShell 7.6.9 and Windows PowerShell 5.1 produced the same
four results on November 8, 2026. Holding either the source (GENERIC_READ|DELETE,
share READ) or staging (GENERIC_READ|GENERIC_WRITE|DELETE, share READ) causes error
32 without changing the source, consuming staging, or creating the backup.
Unlocked replacement preserves the tested protected DACL and original named
streams when staging has none. However, copying a named stream into staging,
then editing that stream on the original, publishes the stale staging stream;
only the backup retains the newer version. A newly added original stream that
does not exist on staging survives. The fixture checks audio, stream contents,
backup contents, staging consumption and DACL SDDL after every operation.

This provides concrete evidence against combining a metadata snapshot copied to
staging with ReplaceFile as a general concurrency solution. Omitting staged
streams avoids this tested conflict, but releasing the held handles still loses
the existing identity/namespace contract. These four synthetic cases do not
validate SACLs, EFS, extended attributes, crash recovery or adversarial pathname
replacement. The production coordinator remains disabled. Probe fixtures and
reports are retained under outputs/replace-file-probes, outside release packages.

A further isolated probe, `tools/probe_refresh_oplock.ps1`, tests whether an
exclusive read/write/handle-caching oplock can close the preservation gap. On
November 8, 2026, PowerShell 7.6.9 and Windows PowerShell 5.1 each passed ten
synthetic cases on the local Windows 10.0.19044 filesystem: five mutations with
share READ and the same five with share READ|WRITE|DELETE. Attribute changes,
existing-stream edits and new-stream creation completed while the oplock was
held without signaling a break. DACL changes and main-stream writes triggered
successful break notifications and completed only after handle release. Exact
audio/stream contents, Hidden state and DACL protection are checked afterward.
The pending asynchronous request is canceled/drained before freeing its buffers.

These are bounded local observations, not a guarantee for every filesystem or
security operation. They disprove using only a default-stream oplock to freeze
all metadata during publication. Separate locks for existing streams would
still leave the observed new-stream and attribute cases unresolved. Production
publication remains disabled. Reports are retained as
`outputs/Soggfy-oplock-preservation-PS7-20261108.json` and
`outputs/Soggfy-oplock-preservation-PS51-20261108.json`; synthetic fixtures remain
under `outputs/oplock-probes`. No recording or installed client was modified.

### Same-file stream rename counterexample

The isolated `tools/probe_refresh_stream_swap.ps1` and
`tools/probe_refresh_stream_interference.ps1` probes investigate keeping the
same NTFS file record while renaming its default data stream to a retained
`:previous` stream and publishing a prepared `:candidate` stream as default.
All 14 cases produced matching observations under PowerShell 7.6.9 and Windows
PowerShell 5.1.19041.7802 on November 8, 2026. These are synthetic local fixtures,
not production publication or a general filesystem guarantee.

- Staging with share READ alone fails with sharing violation 32. Sharing READ
  and DELETE permits preparation while the original READ|DELETE handle stays
  open with share READ. The tested full-file rename fails with 32 both before
  and after moving the original default stream.
- Successful publication retains file identity, protected DACL, edits to an
  existing custom stream and creation of another stream after acquiring handles.
  Hidden is retained, but Archive is added: exact attribute preservation is
  not established. Timestamps, SACL, EFS, compression and extended attributes
  have not been validated.
- A competitor can rename staging or mark it for deletion through another
  handle. In these cases the held staging handle still publishes the tested
  bytes and they remain readable after all handles close. This observation
  does not establish general pending-delete or cleanup safety.
- Cancellation in the gap can restore the original when the new empty default
  stream is unused. A competing writer filling it causes both publication and
  rollback to fail with 87; the competing content and both retained streams
  survive.
- More critically, merely holding the empty default stream open for READ or
  WRITE (even sharing READ|WRITE|DELETE) also makes publication and rollback
  fail with 87. After handles close, the normal pathname is still empty and
  original audio exists only in `:previous`.

The last case disproves treating this two-rename sequence as atomic replacement
with guaranteed immediate rollback. Keeping recoverable bytes in an alternate
stream is insufficient for normal player access. Do not enable this approach
in the production coordinator. A future design must close the observable empty
stream gap or explicitly establish a recovery contract, durable crash journal,
identity-safe restart and cleanup, concurrent metadata policy, and unsupported
filesystem behavior. No journal or recovery coordinator is implemented here.

Reports are `outputs/Soggfy-stream-swap-PS7-20261108.json`,
`outputs/Soggfy-stream-swap-PS51-20261108.json`,
`outputs/Soggfy-stream-interference-PS7-20261108.json` and
`outputs/Soggfy-stream-interference-PS51-20261108.json`; each records retained
fixture paths under `outputs/stream-swap-probes`. Assertions check contents,
identity, protected permissions, retained Hidden state and return codes.

The text policy fills absent lyrics and preserves existing timed/custom lyrics.
It now permits a plain-to-line-timed upgrade only when normalized line endings
leave exactly the same words and every timestamp is monotonic and within the
source's declared sample coverage. Conflicting duplicates, LRC headers/offsets,
malformed timing and changed words preserve the original. The cached collector
retains validated LINE_SYNCED/SYLLABLE_SYNCED line start times without requests;
syllable-level timing is not represented. Matching plain text from another cache
does not downgrade collected timing. Collector and planner regression tests pass
locally and in all four jobs of CI 42449594271. Artwork comparison and completed
transaction integration remain necessary before saved files may change.

The helper's first Windows CI attempt (42448808402) stopped at an explicit-return
warning in the test program's wmain; an explicit success return fixes that
compiler portability issue. CI 42449594271 subsequently validated the helper and
transaction together, including the MinGW build and package checks.

## Artwork envelope inspection checkpoint

The offline inspector now inventories native FLAC PICTURE blocks and
METADATA_BLOCK_PICTURE comments (including repeated and mixed-case tags).
`picture_envelope.h` reads bounded big-endian length fields, checks the defined
picture-type range, printable media-type bytes and strict UTF-8 descriptions,
and distinguishes embedded payloads from the `-->` external-link marker.
Its Ogg base64 decoder requires canonical padding and zero unused bits and
rejects whitespace, alternate alphabets, trailing data and oversized blocks.
Failed parsing clears stale output. Inspection results own their summaries;
borrowed payload views never escape into a recording inspection result.

Malformed or unsupported blocks remain opaque entries and their original bytes
are still preserved by text-only refresh. Duplicate pictures retain their order
within native blocks and comments. No link is fetched. A valid envelope does not
establish valid image data, trustworthy dimensions, provenance or replacement
eligibility: a deliberately non-image payload is accepted in the envelope-only
regression to enforce this distinction. RFC 9639 section 8.8 explicitly treats
its dimension fields as informational; future decoders must establish actual
dimensions independently. Full bounded image decoding and trusted source/content
comparison remain required before replacement can be enabled.

Local diagnostic tests cover every truncated fixture prefix, UINT32_MAX length
fields, UTF-8 edge cases, canonical base64 vectors, bounds, stale-result clearing,
10,000 deterministic envelope mutations and mixed opaque/parsed Ogg/FLAC entries.
All four jobs passed in CI 42471578020 at source
ec4b62c7abc0387ed74fd18e3fad23c236a26ab7. Each job exercised the envelope regression
and 12 real Ogg/FLAC PCM-preservation fixtures; the two sanitizer campaigns each
completed 20,000 seeded iterations. Independent MinGW package verification passed
source/merge-tree, archive/manifest, scripts, PE imports and helper binding checks.
Package SHA256: bf75271f7d40e783b19777a66947c74fb5741e0c2166e81c7b4bb3b14d101865.
Evidence is isolated in outputs/CI-run185; the original RC85 acceptance package
is preserved. Production refresh remains disconnected; shipped image-writing
behavior is unchanged.

A follow-up caps the combined native-block/comment picture inventory at the
existing 100,000 comment-field limit. Without a count cap, tiny malformed FLAC
picture blocks could expand a bounded metadata region into millions of summary
objects. Files exceeding the cap fail inspection with cleared output; they are
never rewritten. Local diagnostic boundary tests cover exactly the combined
limit, one excess comment picture and excess native blocks, plus all 12 PCM
fixtures. All four jobs passed in CI 42472714776 at source
495c42024cbac3c6977aeedcd8f6f6dd51cb3c4c, including these boundary regressions
and all 12 PCM fixtures. Both sanitizer campaigns completed 20,000 iterations.
Independent archive, manifest, source/merge-tree, scripts, PE imports and helper
binding verification passed. Package SHA256:
d30a12c230dc45c309c768c127487065630f11be5ef75c7846a583be9ebc8803.
Evidence and package are preserved separately in outputs/CI-run186. DLL, launcher
and helper are byte-identical to the original RC85 acceptance package; this
offline inspection fix does not enable production refresh.

Primary format contract: https://www.rfc-editor.org/rfc/rfc9639.html#section-8.8

## Remaining implementation and acceptance requirements

The independent output verifier re-plans the exact permitted text delta, checks
the resulting ordered fields/vendor, and compares codec headers and compressed
audio byte-for-byte. Ogg comparison walks packet-by-packet, including audio
granules/EOS/serial; FLAC comparison retains every non-padding/non-comment block
and exact frame bytes. CRC-correct audio mutations and unauthorized custom-tag
changes are rejected. All twelve real-audio cases pass this additional check.
This transfers validity only from an already fully decoded, stable source; it
does not independently establish the validity of synthetic container fixtures.

A settings publication guard now holds the existing shared settings lock and
requires the exact generation with downloads and metadata enabled. Its lifetime
excludes setters; stale/disabled guards release the lock immediately. A Windows
regression checks exclusion from another thread and reacquisition after scope
exit. It must be acquired after staging and held only through final mutation.
The transaction now splits Stage from CommitStaged. The former writes/flushes
the private staging file; the latter performs the bounded namespace operations.
A Windows integration test stages, changes real settings, rejects the stale
guard, and confirms the original survives and staging is cleaned up. The valid
generation publishes successfully under the guard. These additions pass local
tests and all four jobs in CI 42450109385. Filesystem metadata preservation and
the complete production coordinator remain unfinished.

The Windows preparation prototype now joins held-source reading, owned-file
inspection, minimal text planning, isolated full decoding, exact decoded/header
coverage agreement, retagging, independent output verification and private
staging. It never publishes by itself. A no-op skips decoding and does not claim
audio validity. Incoming artwork is ignored and existing artwork remains exact.
The caller must retain the transaction and acquire its settings fence after
staging, rechecking cancellation before publication. Twelve real Ogg/FLAC cases
pass locally, each exercising staging cleanup, fixture publication, no-op,
conflicting identity, missing helper, plausible-but-wrong decoded coverage,
immediate cancellation and throwing cancellation. All four jobs in CI
42450663860 passed, including the MinGW preparation path. The preparation
coordinator is not production-connected; RC82 subsequently deployed the helper.

The helper trust prototype now checks SHA-256 against a caller-supplied trusted
build digest, retains a read-shared executable handle and locks resolved local
ancestor names until validation finishes. It rechecks file identity after
locking the ancestors and exposes no path on a rejected open. Local Windows
tests cover launching while locked, Unicode paths, attempted mutation/rename,
digest mismatch, malformed requests and hard links. The supervisor accepts the
held trust object. RC82 binds the expected digest to the DLL build; it is never
read from an adjacent mutable manifest. Production caller integration remains open.

The offline build now generates the helper SHA-256 header after linking the
worker, then compiles that value into its supervisor and preparation test
callers. OpenBuildPinned accepts no runtime expected digest. The harness checks
acceptance of the exact build and rejection of a substituted executable even
with an updated adjacent checksum file. Failed generation removes stale headers;
missing, empty and oversized helpers fail the build. Both helper/source locks
remain held during preparation. This closes the runtime-selected-digest gap in
the offline harness. RC82 also compiles that digest and trust entry point into
the release DLL and packages the finalized helper.

The reusable offline worker builder now strips the linked executable, checks
GNU objdump output for x64 PE format and exactly KERNEL32/MSVCRT imports with
an empty delay-import directory, then generates the digest. Unexpected dynamic
codec, GCC or UCRT dependencies fail the build. It removes an older digest
header before linking so a failed rebuild cannot retain a prior binding. Windows
CI runs the decode/preparation tests against this finalized worker. This build
policy is not a signature check or a security sandbox. RC82 adds automatic and
launcher package copies plus codec licenses, and requires the release helper
to equal the exercised offline helper byte-for-byte. Packaging checks the DLL's
embedded digest. The installer uses SoggfyValidation-<sha256>.exe, validates
existing same-name files, copies with CreateNew inside its installation-directory
transaction, and retains older builds for DLL rollback. Uninstall checks both
DLL and present helper ownership before mutation; a DLL backup retains the helper
because the restored DLL may use the same build. These changes deploy the helper
foundation without enabling the unfinished metadata-refresh coordinator.

1. Read metadata from complete native recordings. Require exact Spotify URI
   agreement and recognized complete-listen/capture provenance. Legacy or
   uncertain identity cannot authorize a rewrite. Metadata/header checks and
   provenance tags alone do not establish complete, valid audio.
2. Validate the whole source payload while retaining a stable source handle.
   Existing FFmpeg validation is private to conversion and only runs for
   certain conversion paths; metadata-only refresh cannot rely on that path
   having run. Preserve strict Ogg/FLAC completeness requirements.
3. Plan a minimal delta: preserve populated/custom fields and repeated artists;
   fill missing fields; keep DATE/YEAR coherent and synchronized lyrics.
   Preserve all capture, duration and quality evidence from the old recording.
4. Validate artwork structurally and compare source, dimensions and content
   before replacing it. Current BuildPicture only bounds size and MIME; it is
   not an image validator. Preserve opaque/custom art unless replacement is
   specifically justified. No artwork should be synthesized from missing data.
5. Verify retagged output: identical audio payload/coverage and quality, exact
   identity, complete owned provenance, retained custom fields and artwork.
   Quality upgrades must preserve eligible old metadata as a separate case.
6. Publish with a transaction tied to the inspected file identity and settings
   generation. Recheck cancellation before mutation. Concurrent edits, deletion,
   replacement, locks and failed rollback must retain recoverable originals.
   Do not use the unconditional export ReplaceCompleteFile API.
7. Cover no-op, missing metadata, duplicates, conflicting identities, legacy
   files, corrupt/truncated payloads, custom artwork, repeats, cancellation,
   concurrent path replacement and publication failures. Run full CI and verify
   the final MinGW artifact independently before live acceptance.

The upstream comparison remains pinned to Floggfy v1.3-RC6 commit
7b825e3b942596c3e912fbbf7a0487abd7578e47. Its metadata-update policy is useful
reference material; its structural inspection is not a substitute for payload
validation. Native MP3 capture, native playback ownership and current-instance
skip are separate unfinished milestones.
