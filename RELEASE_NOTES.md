# Soggfy v3.0.0-rc.48

RC48 restores the per-track red crosses and green checkmarks in the Classic Soggfy playlist interface.

- A red cross now appears for tracks without a matching local download, rather than leaving the status column blank.
- Green checkmarks indicate completed downloads, with a hover tooltip and Open Folder action.
- Failed tracks show the red error icon with the actual failure reason; in-progress/converting and ignored states retain their existing icons.
- Visible playlist rows are checked periodically for new files because MP3 conversion can finish without Spotify updating the playlist DOM.
- The status indicator is placed beside the track-duration cell and given a fixed visible size.
- Track-title extraction is more tolerant of Spotify's newer row markup. If a URI is unavailable, a local query identity allows filename-based status detection.
- Added regression tests for missing, downloaded, error and refreshed statuses.

RC47's functioning 50x decoder, audio capture, listen completion, MP3 conversion and SpotX support are unchanged.

The Classic UI indicators and their refresh logic still require a real Spotify UI test to verify appearance and native status matching.
