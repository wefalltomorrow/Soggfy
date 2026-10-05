# Soggfy v3.0.0-rc.27

RC27 hardens the Classic ad/telemetry filter using the URL-matching strategy from current BlockTheSpot.

## Why RC26 was still too narrow

RC26 expanded the old Soggfy filter from `spclient.wg.spotify.com` to regional `*-spclient.spotify.com` hosts. That is better than the old fixed-host rule, but it still assumes Spotify will keep using a known hostname pattern.

Current BlockTheSpot avoids that problem entirely: it extracts the request path and matches ad/telemetry endpoints independently of hostname.

## RC27 behavior

The CEF request filter now parses the URL path and blocks these prefixes on any host:

- `/ads/`
- `/ad-logic/`
- `/gabo-receiver-service/`
- `/dodo-receiver-service/`

Queries and fragments are stripped before matching.

This means Spotify can move those endpoints between `wg`, regional spclient hosts, or a future hostname without requiring another Soggfy update.

The rules remain deliberately path-scoped. Soggfy still allows metadata, audio-CDN, login, update and unrelated requests.

Regression coverage now includes:

- historic wg endpoints;
- current regional Spotify endpoints;
- future/unknown hosts using the same ad paths;
- query/fragment handling;
- negative cases proving metadata/audio/update traffic remains allowed.

## References reviewed

- Nuzair46/BlockTheSpot commit `6191f65aa02908f892cfdba33ac4612499c546d9` — host-independent CEF URL path blocking.
- SpotX-Official/SpotX commit `3f3fd30a95121a26ad723c963c2c0879cca4d664` — reviewed for UI-side ad state/container suppression ideas.

No BlockTheSpot or SpotX binary/code is bundled. The Soggfy implementation remains its own small CEF filter.

Playback-speed behavior is unchanged from RC26.
