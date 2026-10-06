# Soggfy v3.0.0-rc.30

RC30 restores the old Soggfy-style per-track status icons in Spotify's track rows.

## What the RC29 test proved

RC29 is now completing the full path successfully: native capture, accelerated completion validation, MP3 conversion, final publication and the backend DONE state all complete correctly. The test log records a finished MP3 and the saved counter incrementing to one.

The missing check/cross was therefore a Classic UI rendering problem rather than a downloader problem.

## What changed

- Restored the original Soggfy/Sprinkles placement model: the status icon is mounted in the track row's final duration/actions cell.
- Normalizes current Spotify's extra `role=row` wrapper before inserting the indicator.
- Adds a more robust React-props fallback for track URI, title, artists and album when Spotify's visible row links do not expose every field.
- Keeps DOM-link metadata as the first choice and uses React data only as a fallback.
- Refreshes visible status rows every 1.5 seconds so native transitions such as Downloading -> Converting -> Done/Error appear even when Spotify itself does not mutate the row.
- Keeps the original status set: blue downloading, processing, green check for DONE, red cross for ERROR, warning and ignored.
- Makes the status SVG explicitly visible so current Spotify row CSS cannot collapse or fade it accidentally.
- Adds a concise `FLOGGFY_STATUS:classic` diagnostic whenever the visible-row/status/render counts change.

RC29's working playback-speed and accelerated-download fixes are unchanged.
