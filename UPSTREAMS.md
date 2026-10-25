# Upstream sources

The active x64 implementation started from:

- Mainkill1/Floggfy tag `v1.1.0`
- commit `dbf1ba819a7a7a52e9992ad5c786ac74b9a93cac`

The original Soggfy repository was at commit:

- Rafiuth/Soggfy `596bc4e3942a42f401e857c875ec456f9457b768`

The old source is preserved under `legacy/`.

Community work reviewed while building this fork:

- SuperSecretEyeball/Soggfy-Fixed — artist/path metadata changes and TypeScript cleanup
- MacKinnon7/Soggfy-Fixed — newer release-build workflow fixes
- AndyBogle1/Soggfy / upstream PR #143 — safer installer reuse behavior
- coleaderme/Soggfy / upstream PR #125 — TLS 1.2 installer compatibility
- MrSykenro/Soggfy / upstream PR #148 — current SpotX script URL
- upstream PR #102 — quoted/idempotent uninstall-path handling

Not every patch was copied literally. Fixes tied to the obsolete x86 installer/hook architecture were reimplemented only where they still apply to the modern x64 design.

Floggfy v1.1.0's optional explicit launcher and episode current-item identity fixes are included. The all-in-one ZIP includes the launcher in `Launcher/` alongside automatic `version.dll` mode. Use one mode at a time.

Soggfy's capture core intentionally remains ahead of Floggfy v1.1.0 in a few places: replayed Ogg pages are tolerated without duplicate writes, complete-listen validation is aware of accelerated playback, and the Classic UI/path/post-processing layers remain Soggfy-specific. A wholesale rebase would regress those fixes, so upstream changes are merged selectively.


## RC64 selective observer adaptation

Reviewed Mainkill1/Floggfy v1.2.4 (`cf72f18b65afc24ed3c5a4899c80d2ff68b75d74`)
and v1.2.5-RC1 (`15e4b764c7d78bd5ecfe67d6414def505a4410c5`). RC64 adapts
the dedicated media-reader and sequenced-snapshot boundary from commits
`4953c0a3874ec3178a6462183bb43739a22d132c` and
`72a4160c4cf6b26ae3fe759b76f673dd886af275`, with configuration revision fencing,
pre-artwork observation timestamps and Soggfy's existing accelerated clock.
The gated Windows test follows upstream's blocked-provider test approach.

Normal-speed timeline continuity rules, late-capture retention, repeat/delayed
identity handling and RC1's opt-in NetworkPatch defaults are not included in
this candidate. They require separate compatibility work. RC1 itself does not
claim that random pauses are solved.

## Compatibility references

RC65 selectively adapts late-capture retention from Mainkill1/Floggfy commit
`f80dfdd91f740afdce22869308b504308e072a61`. Soggfy keeps its existing association
window and accelerated listen tracker, binds retention to the original completed
listen and defers selection while a retained competing stream is incomplete.
The shared portable policy is tested with actual Ogg and FLAC validators.
The RC64 exclusions above describe RC64 specifically; RC65 did not include
upstream repeat/delayed-identity generation changes. Network defaults remain
outside RC66 as well.

## RC66 selective repeat and identity adaptation

Adapted from Mainkill1/Floggfy commits
`1bb4bf303cf09b303cd3c585266479d7b100cb6d` (generation-aware repeats),
`54f4ff416ab4280e33ccc0db5a62c83bf6c8abef` (external coverage invalidation), and
`f27deafebf14f6929d34aee26da6f5e2367a57b7` (delayed title reconciliation).
Soggfy retains its 1x–30x clock and adds raw-reset evidence, previous-rate
continuity, immutable completion timing and generation-bound capture retention.
The one-second confirmation and original two-second identity window are tested
against normal and accelerated playback. This is a selective adaptation, not
feature parity or a wholesale upstream timeline replacement. Live Spotify
Repeat One behavior still needs validation.

Playback-speed compatibility was independently reimplemented against the current x64 Spotify track-player calling convention and validated with structural PE checks. Public reverse-engineering references were used only to confirm the modern calling convention and pattern family; no third-party implementation source was vendored.

The active hook requires a unique validated Spotify.dll target and fails closed when discovery is missing or ambiguous.
