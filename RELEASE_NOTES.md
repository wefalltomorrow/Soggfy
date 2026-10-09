# Soggfy v3.0.0-rc.41

RC41 fixes Spotify 1.3.1.234 dropping accelerated playback back to audible 1x after a track ends naturally.

Manual Next already created a fresh track player and picked up the configured Soggfy speed. Natural queue advancement could instead reuse a pre-created 1x player, so Soggfy's native constructor hook never got a chance to apply 10x-50x to that new track.

RC41 keeps the validated 1.3.1.234 native speed hook and adds a one-time current-track recreation whenever the active Spotify URI changes while accelerated playback is enabled. A short watchdog covers natural transitions that do not follow the same update path as a manual Next click, and a URI/speed guard prevents reset loops.

The RC40 Classic UI startup-race fix, capture engine, metadata, FFmpeg output, telemetry-only Soggfy filtering and SpotX ad/update handling remain unchanged.
