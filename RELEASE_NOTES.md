# Soggfy v3.0.0-rc.50

RC50 corrects the RC48/49 Classic UI status-indicator regression that inserted red Xs into Spotify's home-page recommendation cards, sidebar, playlist artwork and non-track rows, breaking the track-list layout.

The previous implementation incorrectly selected every `div[role="row"]`, fabricated pseudo track URIs when none could be found, treated the absence of a local download as a failed track, and could prepend indicator nodes to the entire Spotify row rather than its duration cell.

This fix follows the original [Rafiuth/Soggfy Sprinkles/src/ui/status-indicator.ts](https://github.com/Rafiuth/Soggfy/blob/master/Sprinkles/src/ui/status-indicator.ts):
- Scan only `div[data-testid="tracklist-row"]` or `.main-trackList-trackListRow` inside Spotify's main view.
- Require an actual Spotify track or episode URI; do not fabricate one from album/card text.
- Render no icon for an absent download. Show a red X only for a real `ERROR`, green check for `DONE`, and the existing progress/warning/ignored icons for their actual statuses.
- Insert badges into a validated track row's trailing duration cell, never the row root, home page, sidebar or playlist artwork.
- Restore the compact original Soggfy icon styling rather than the fixed-width RC48 layout.
- Keep periodic refresh so new completed MP3s appear without reopening the playlist.
- Replace the inaccurate RC48 tests with DOM-scoping regressions for legitimate track rows, home cards, sidebar playlists, unknown URIs, empty status, completed status and errors.

No changes to the RC46 decoder, RC47 playback position, RC49 automatic-stall recovery, MP3 conversion, SpotX, or other installation behaviour.

Automated tests are not a substitute for validating the actual Spotify DOM on your installation.
