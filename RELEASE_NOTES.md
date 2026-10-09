# Soggfy v3.0.0-rc.42

RC42 fixes the startup crash seen after leaving playback speed set to 50x and then restarting Spotify 1.3.1.234.

Three crash dumps showed the same execute-access violation at a low raw address before normal Soggfy initialization completed. Two of those dumps were still RC40 and the third was RC41, so the RC41 natural-transition fix itself was not the cause.

RC42 makes two startup-sensitive paths fail safe:

- The validated Spotify 1.3.1.234 speed hook still installs, but persisted acceleration is disarmed during process startup. It passes Spotify's own native speed until the live Soggfy UI/player path explicitly arms acceleration immediately before rebuilding the actual current track.
- The connectivity compatibility hook no longer patches an unresolved delay-IAT slot. It waits until Windows has resolved the target to executable code outside Spotify.dll, preventing a raw image RVA from being used as a callable address.

RC41's natural-transition speed recovery and RC40's Classic UI startup fallback remain in place.
