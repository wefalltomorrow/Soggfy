# Soggfy v3.0.0-rc.10

RC10 fixes downloads failing as `incomplete listen` when Soggfy's accelerated playback is enabled.

## Fixed

- The complete-listen validator previously measured media progress as if Spotify were always playing at 1x.
- At 10x-50x, perfectly normal position jumps between 500 ms polls therefore looked like seeks and canceled the download.
- Soggfy now uses the active validated playback rate for Windows media-session position extrapolation and complete-listen validation.
- Natural end-of-track transitions can be recognized even when high-speed playback moves through the final many media seconds between two polls.
- Delayed Spotify title/timeline updates remain handled safely at accelerated speeds.
- Real seeks, skips and incomplete captures still fail closed, and the compressed audio stream must still reach a validated EOS before publication.

RC10 includes the RC9 Ogg replay fix, RC8 legacy-library detection and RC7 settings-button fix.
