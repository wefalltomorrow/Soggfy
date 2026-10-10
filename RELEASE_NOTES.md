# Soggfy v3.0.0-rc.58 — upstream-style PlayerAPI discovery

This release investigates why downloaded-track status icons now work in RC57 but **Skip downloaded tracks** does not. The supplied RC57 log confirms `classic disk index file_count=2214` and `classic status rows=57 responses=57 states={"DONE":56,"NONE":1}`, but contains **no `queue ...` or `queue player ready` events** from our queue-skip logic.

## Why this differs from original Soggfy

Original `Rafiuth/Soggfy/Sprinkles/src/spotify-apis.ts` keeps polling the React root until `Platform.getPlayerAPI()` is available, then `player-state-tracker.ts` attaches its `queue_update` listener and calls `Player.removeFromQueue` for upcoming songs confirmed downloaded on disk.

In RC57 the fallback React Fiber traversal advances via `node.child || node.sibling || node.return`. After entering the first child this can bounce between a leaf and its parent indefinitely, never examining other siblings. The loop gives up after 200 attempts (~10 seconds). If no Player API is found, the entire queue listener/periodic skip subsystem is never installed, explaining the complete absence of queue logs despite working status icons.

## Change

- Traverse React child **and sibling** fibers with a bounded stack and a visited set, prioritising the original upstream child path but covering alternate Spotify 1.3.x tree branches. Do not repeatedly revisit the parent of the first leaf.
- Keep searching for Platform beyond the old fixed timeout, as the original Soggfy did.
- After Platform exists, retry `getPlayerAPI()` until it actually becomes available before installing queue listeners.
- When `DebugLog=1`, log sparse `classic queue platform pending` messages and `classic queue player ready skipDownloaded=N skipIgnored=N`. Existing queue diagnostics will then indicate queue source, matched downloads and removal attempts.

No changes to original file-based download checks, queue match/removal logic, playback-speed hooks, capture, MP3 conversion, user files or any new status database.

## Tests

The new `tests/classic_player_discovery_test.js` reproduces the RC57 first-child traversal cycle, forces Platform to appear **after the old 200-attempt cutoff**, and asserts successful PlayerAPI initialization and queue-listener installation with diagnostics. Existing original Soggfy queue/removal tests run unchanged.

We still need in-app confirmation of actual Spotify queue events and removals; the log will now distinguish 'never found Player API' from 'queue data unsupported'.

## Test instructions

Start Spotify with Skip downloaded tracks enabled, play the Tunes playlist, and inspect the next Soggfy.log. The `queue player ready skipDownloaded=1` entry should appear, followed by `queue source=...` or `queue removed=...`. If it remains pending, the next investigation should target the exact modern Spotify Platform exposure rather than guessing at the filename matcher again.
