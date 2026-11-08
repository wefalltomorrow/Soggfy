# Saved-recording metadata refresh

This is unfinished engineering work, separate from RC79 playback snapshots.
The current publication path skips equal/higher-quality native recordings before
constructing tags. It can retry conversion but does not refresh the saved tags.

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
registered in native, Windows and sanitizer scripts; this development change
has not yet run in CI or been connected to saved-file publication.

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
within each of the twelve local cases. This test is required by native, Windows
and sanitizer CI scripts but has not yet run in CI. Local Zig builds remain
diagnostic; RC80 is still the latest verified MinGW package.

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
validator are linked into tests only. Before shipping, run the decoder in an
isolated helper with memory/time/cancellation limits; bounded compressed input
alone does not bound codec allocation or CPU. Stable-source supervision and
publication integration remain required.

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
source lock through a separately guarded transaction. Neither the helper nor
the supervision API is packaged or called from production yet.

The standalone transaction prototype opens the original with read/delete access
and read-only sharing, locks each resolved local ancestor against rename, then
rechecks the resolved path's file identity. It stages and flushes complete bytes,
moves the original by its held handle and publishes by the staged handle, always
without replacement. Cancellation before mutation leaves the original in place;
cancellation/failure after the original move attempts rollback. If a concurrent
creator occupies the destination, neither publication nor rollback overwrites it,
and the result reports the preserved original's backup path. Rename/recovery
storage is allocated before mutation. Local tests cover these paths, directory
and file locks, single-use transactions and hard-link rejection. CI is pending.
This prototype still needs filesystem security/stream/attribute preservation,
the actual settings-generation fence, validated-delta integration and production
packaging. Network/redirected roots are conservatively rejected. Its callback
checks do not by themselves make settings-generation publication atomic.

Reference sources inspected:
- https://xiph.org/vorbis/doc/Vorbis_I_spec.html (appendix A)
- https://github.com/xiph/vorbis/blob/c2aa86b05e981c96bf381fc6aa11cdd03eccc2fb/lib/block.c
- https://github.com/FFmpeg/FFmpeg/blob/n8.0/libavformat/oggparsevorbis.c

The text policy fills absent lyrics and preserves existing lyrics. Upgrading
plain lyrics to a verified synchronized version remains required. Full payload
validation, artwork comparison and the transaction below remain necessary
before any actual saved recording may be changed.

## Remaining implementation and acceptance requirements

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
