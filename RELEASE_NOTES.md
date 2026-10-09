# Soggfy v3.0.0-rc.51

RC51 fixes two remaining playlist status problems in RC50: only the first track was receiving a status icon, and an old blue downloading icon could remain after completion or failure.

## Cause

RC50's fallback scanned an entire React Fiber subtree for a Spotify URI. It could retrieve the first playlist track's URI for unrelated rows; the per-URI deduplication then discarded those rows entirely. Rows without a visible Spotify URI could also be ignored, even when their song title and artist were available.

In addition, a playlist row with no album field was being assigned the *playlist title* as its album. The backend required an exact album match for recent live states, which could prevent an `IN_PROGRESS` state from being replaced by `DONE` or `ERROR` after a native capture.

## Changes

- Resolve each track using its own Spotify href or original upstream Soggfy-style row-scoped React menu props. Never recursively scan an unrelated Fiber tree.
- When a real track-list row does not expose its URI, create a stable filename/status lookup identity from its title, artist and album; never do this for home tiles or sidebar elements.
- Preserve duplicate occurrences of the same song as separate visible DOM rows.
- Read the album from the row's album cell; do not substitute the playlist name. Permit native recent-status matching when a row omits the album entirely.
- Refresh status every three seconds, and log a compact `classic status rows=... responses=... states=...` diagnostic every 20 seconds when debug logging is enabled.
- Add a multi-row regression test covering five rows, distinct identities, Spotify's React props, local fallback, duplicate songs, and updates from blue downloading to green downloaded.
- Preserve original Rafiuth/Soggfy indicator rules (green check for DONE, red X for actual ERROR, blue for IN_PROGRESS, no icon for unknown/missing statuses) and track-only placement from RC50.

The RC46 decoder, RC47 playback clock, RC49 stall recovery, MP3 export settings, SpotX, and capture pipeline are unchanged.

This is a test release; confirm visual status updates within Spotify, because CI cannot simulate every live DOM variant.
