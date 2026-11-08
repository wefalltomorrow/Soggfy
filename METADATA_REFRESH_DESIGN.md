# Saved-recording metadata refresh

This is unfinished engineering work, separate from RC79 playback snapshots.
The current publication path skips equal/higher-quality native recordings before
constructing tags. It can retry conversion but does not refresh the saved tags.

## Implemented foundation, awaiting CI

`vorbis_comments.h` supplies one bounded reader/writer for native Ogg/FLAC
comments. Retagging and recording-identity parsing share the reader. The writer
rejects comment counts above 100,000 and oversized bodies before allocating
the serialized result; previously taggers could emit a count that their own
readers rejected. Raw vendor text, field order, duplicates, casing, Unicode
and unknown custom fields survive the common representation. Container limits
remain enforced by each tagger. This validates metadata structure only.

Local strict diagnostic builds and parser, Ogg, FLAC and identity regressions
pass. The new parser regression is registered in native, Windows and sanitizer
suites. This change has not yet received CI or package verification.

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
