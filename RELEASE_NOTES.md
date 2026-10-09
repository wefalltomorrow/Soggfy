# Soggfy v3.0.0-rc.44

RC44 fixes RC43 launching to an empty Spotify window.

RC43 waited for libcef.dll before touching Spotify.dll. That avoided the earlier loader crash, but it also delayed the connectivity compatibility hook until after Spotify had already made its initial network-state decision, which can leave the XPUI shell blank.

RC44 no longer uses CEF as the readiness signal. It directly checks several loader-critical Spotify.dll normal imports and requires them to resolve to executable addresses, then waits an additional 1500 ms before installing the connectivity, playback-speed and capture hooks.

This is the same loader-readiness strategy previously used to avoid raw-RVA startup crashes, while keeping the connectivity hook early enough to affect Spotify's first connectivity check.

RC42's delay-IAT safety and startup-disarmed acceleration, RC41's natural-transition speed recovery, and RC40's Classic UI fallback remain in place.
