# Soggfy v3.0.0-rc.47

RC47 fixes the RC46 regression where Spotify tracks played at 50× but nothing was downloaded.

The decoder hook introduced in RC46 is still working and has not been changed. The failure was in the separate complete-listen tracker: each time Spotify refreshed its public playback timestamp at normal speed, Soggfy recalculated the accelerated position from that new timestamp. A track could jump from 190 seconds back to 13 seconds, get rejected as `position_rewind`, and never reach the file conversion/save queue even after a complete native Ogg capture.

RC47 keeps an independent monotonic position for each media identity during accelerated playback. Public Spotify metadata still establishes the initial position of each new track, normal 1× playback retains its previous behavior, and pausing or changing the speed does not reset the accelerated position.

Added regression tests reproducing the RC46 timeline refresh and verifying that a full 50× natural track transition is accepted. This is a targeted candidate fix; actual Spotify download publication still needs an in-app test.

SpotX installation handling, Classic UI, compressed Ogg/FLAC integrity checks, FFmpeg settings and the RC46 PCM decoder speed hook are unchanged.
