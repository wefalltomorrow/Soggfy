# Soggfy v3.0.0-rc.19

RC19 is a diagnostic build for the Spotify 1.3.3.264 playback-speed path.

RC18 correctly stopped caching an inactive startup ContextPlayer, but the latest live log still never found an active player and stayed at an effective 1x. RC19 makes the normal Soggfy log detailed enough to show exactly where discovery is failing.

## New speed diagnostics

While playback speed is configured above 1x and no usable player has been acquired, Soggfy now records a rate-limited scan report every five seconds:

- Spotify module base, image size, ContextPlayer vtable and wrapper addresses.
- Number of writable private regions/chunks scanned and total MiB examined.
- Number of raw ContextPlayer vtable hits.
- Counts of valid, inactive and invalid ContextPlayer layouts.
- Current-track and prepared-track candidate counts.
- Raw current/prepared/dispatcher pointers for up to six ContextPlayer candidates.
- Whether each current/prepared pointer can be read and passes the expected TrackPlayer shape.
- Nearby player-like object pointers inside each candidate, including their object offset and Spotify vtable/setter/getter RVAs.
- The final selected ContextPlayer address, if one is found.

The extra output is intentionally rate-limited and only runs while accelerated playback is requested, so the normal 5 MiB log cap still applies.

No capture or conversion behavior is changed in this build.
