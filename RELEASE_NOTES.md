# Soggfy v3.0.0-rc.57 — original downloaded-file detection on Windows

This build targets the RC56 regression where existing MP3s are visible in `C:\Users\Shimon\Music\Spotify`, but the original-style green checkmarks are missing and **Skip downloaded tracks** cannot find confirmed `DONE` statuses.

## What was observed

The supplied screenshots show a configured `{all_artist_names} - {track_name}.{ext}` template and existing files including `Shpongle - The Magumba State.mp3`, `Shpongle - Empty Branes.mp3` and `Oasis - Wonderwall.mp3`. The user-supplied RC56 log showed `classic status rows=28 responses=28 local_ids=28 states={"IN_PROGRESS":1,"NONE":27}`, meaning the status backend returned no completed-file matches for those rows. The visible playlist itself is working. The exact filenames pass `ClassicPathMatches` C++ regression tests; this makes a filesystem traversal problem plausible, though not proven until tested in Spotify.

## Changes

- Use Windows' Unicode `FindFirstFileW`/`FindNextFileW` directory enumeration to build the same read-only recursive file index used by Soggfy's original file-based status detection. This avoids an unreported `std::filesystem::recursive_directory_iterator` failure on Windows/MinGW and works with the same Win32 extended-length path normalization used by Soggfy elsewhere.
- Continue to scan the **configured base folder** (no status journal or database), recurse to depth 12, cap the index at 100,000 files, ignore directory reparse points, and preserve full file paths for the original Open Folder indicator.
- With `DebugLog=1`, log `classic disk index file_count=... directory_count=... root_exists=... root_attributes=... last_error=...` on the ten-second index refresh. Also log a bounded `classic disk match` count and up to three unmatched track titles/artists every 20 seconds. These are **diagnostics**, not automatic repair or skipping logic.
- Add regression cases for the exact screenshot file names and the selected MP3 output template, with artist/title identity checks.

No change to original-style `DONE` handling, 50× playback, capture/MP3 publication, playlist UI, skip queue, ignore settings, or 1× listen mode. No user files are modified.

## Test

Install over RC56. On the *Tunes* playlist, verify the existing Shpongle, Oasis and other MP3s show green checks. With Skip downloaded tracks on, confirm they disappear from the *upcoming queue* only, just like original Soggfy. If checks are still missing, provide `Soggfy.log` with `DebugLog=1`; `classic disk index` and `classic disk miss` lines will now distinguish folder enumeration failure from artist/title mismatches.

Passing CI proves builds and filename matching, not in-app filesystem detection until the user verifies it.
