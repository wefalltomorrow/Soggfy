# Upstream sources

The active x64 implementation started from:

- Mainkill1/Floggfy tag `v1.1.0-rc.5`
- commit `a4fa629628a9756ca3db4b41f4d171fc6ed93a1e`

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

RC5's current-track footer fixes, cached-state refresh, CEF label formatting and stricter stale-state rejection are included together with its regression coverage.


## Compatibility references

Playback-speed compatibility was independently reimplemented against the current x64 Spotify track-player calling convention and validated with structural PE checks. Public reverse-engineering references were used only to confirm the modern calling convention and pattern family; no third-party implementation source was vendored.

The active hook requires a unique validated Spotify.dll target and fails closed when discovery is missing or ambiguous.
