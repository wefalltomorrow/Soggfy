# Soggfy v3.0.0-rc.16

RC16 restores the 1-50x playback-speed control on Spotify for Windows x64 1.3.3.264 with a new implementation.

## Spotify 1.3.3.264 speed backend

The old Soggfy speed feature modified the speed argument while Spotify constructed a track player. That internal path changed and is no longer discoverable on 1.3.3.264.

Using the Spotify.dll extracted from the official x64 installer, RC16 instead targets Spotify's own current and prepared ContextPlayer playback-speed methods. The new backend:

- validates the exact 1.3.3.264 native layout before enabling itself;
- locates the live ContextPlayer object instead of guessing a constructor ABI;
- validates the nested player/vtable methods before use;
- applies speed directly to the current and prepared tracks;
- reapplies the configured value across track changes;
- changes speed immediately, so the current track no longer needs to be restarted.

Spotify 1.3.1.234 still uses the older validated constructor backend. Other builds remain disabled at 1x until explicitly validated.

## Capture

All RC15 capture/download fixes remain included. Spotify 1.3.3.264 remains the preferred capture baseline.

The release remains one all-in-one Windows x64 ZIP.
