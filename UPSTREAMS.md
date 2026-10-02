# Upstream sources

The active x64 implementation started from:

- Mainkill1/Floggfy tag `v1.1.0-rc.3`
- commit `a8277d2791ea07a1144e03ccbc7b598da0066f8f`

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
