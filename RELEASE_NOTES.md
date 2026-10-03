# Soggfy v3.0.0-rc.11

RC11 selectively syncs the useful changes from Floggfy v1.1.0 without replacing Soggfy's newer capture fixes.

## Floggfy 1.1.0 sync

- Adds a separate launcher package containing `Soggfy.exe` and `Soggfy.dll`.
- Launcher mode is an optional fallback for systems where normal adjacent `version.dll` loading is skipped.
- It stops Spotify processes from the same installation, starts a fresh client, explicitly loads Soggfy before application startup, waits for the Soggfy readiness signal, then resumes Spotify.
- Automatic `version.dll` mode remains the normal/default package. Install only one mode.

## Episode current-item fix

- Sparse podcast/episode titles can remain visible even when Spotify omits music-style artist or album fields.
- Episode playback identity now uses its Spotify episode URI, so two episodes with the same title cannot inherit one another's cached decoder/quality details.
- Target quality preferences are still never substituted for missing actual playback quality.

## Kept from Soggfy

Floggfy v1.1.0 still uses the older strict Ogg page sequence and 1x complete-listen timing code. RC11 deliberately keeps Soggfy's RC9 Ogg replay handling and RC10 accelerated-playback validation, plus the Classic UI, legacy-library matching and conversion features.

