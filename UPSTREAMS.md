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

Floggfy v1.1.0's optional explicit launcher and episode current-item identity fixes are included. The launcher is packaged separately from automatic `version.dll` mode, matching upstream's one-mode-at-a-time design.

Soggfy's capture core intentionally remains ahead of Floggfy v1.1.0 in a few places: replayed Ogg pages are tolerated without duplicate writes, complete-listen validation is aware of accelerated playback, and the Classic UI/path/post-processing layers remain Soggfy-specific. A wholesale rebase would regress those fixes, so upstream changes are merged selectively.


## Compatibility references

Playback-speed compatibility was independently reimplemented against the current x64 Spotify track-player calling convention and validated with structural PE checks. Public reverse-engineering references were used only to confirm the modern calling convention and pattern family; no third-party implementation source was vendored.

The active hook requires a unique validated Spotify.dll target and fails closed when discovery is missing or ambiguous.
