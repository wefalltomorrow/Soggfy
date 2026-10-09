# Soggfy v3.0.0-rc.40

RC40 fixes the Classic Soggfy UI sometimes failing to appear on Spotify 1.3.1.234 during a normal launch.

## Classic UI startup fix

RC39 could load successfully but still miss the main browser if Spotify created it before the normal UI callback was observed.

RC40 keeps the existing callback path and adds a post-start fallback that looks up the already-running CEF browser after a short grace period, validates the browser objects, attaches the existing UI bridge, and loads Soggfy into the current main frame.

The fallback retries until a valid browser is available and uses the corrected CEF 151 client method slot identified during the earlier crash investigation.

Spotify 1.3.1.234 remains the supported baseline. Playback speed, capture, metadata, output handling, and the RC39 installer setup are otherwise unchanged.
