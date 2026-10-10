# Soggfy v3.0.0-rc.59 — use Spotify's verified cached PlayerAPI

## What the RC58 user log proves

- `Skip Downloaded Tracks=1` in SpotifyHistory.ini and native config, so the toggle is working.
- `classic disk index file_count=2214` / `2215`, with `DONE` statuses in the playlist, so saved-audio lookup and checkmark generation work.
- At 05:35:42, the **existing read-only metadata collector** reports `FLOGGFY_STATUS:cached player found`.
- Meanwhile the Classic UI logs `queue platform pending attempts=1/150/300/450` without any `queue player ready` or `queue removed` entry. That means the original queue listener never starts; we should not touch the file matcher again.

## Source-level cause

Our `native/metadata_collector.js` already scans the live React service registry (including Map-held PlayerAPI instances), verifies a `getState()` snapshot with a Spotify URI and `getEvents()`, and uses that exact player for live metadata. The Classic UI's `getPlatform()` instead only searched for a `platform` object via older React-props paths, which modern Spotify did not expose in this run. RC58 improved traversal but did not reuse the confirmed player found by the collector.

## RC59 fix

- The collector shares its **already-verified real Spotify PlayerAPI object** as `window.__soggfyVerifiedPlayerAPI` in the same injected page context. No duplicate service construction, network requests, or synthetic playback APIs.
- The original-style Soggfy PlayerAPI initializer uses a real exposed `Spicetify.Platform` when available, otherwise reuses the verified cached player, while retaining its existing React Platform search for clients where that still works.
- Discover the cached player even with optional metadata enrichment turned off: queue skipping must be independent of metadata decoration.
- Extend the existing debug readiness line with `source=verified-cached-player` or `source=platform` so the next `Soggfy.log` proves listener startup and shows existing queue sources/removal attempts.
- Keep `Player.getEvents().addListener('queue_update', ...)`, on-disk `DONE` detection, `Player.removeFromQueue`, and the existing per-URI cache identical to RC58 / original Soggfy. **No new database, no skipping of currently playing audio, and no changes to the working checkmarks.**

## Regression coverage

`tests/classic_cached_player_skip_test.js` provides a modern Spotify-like cached service registry with no global `Spicetify.Platform`. It loads both the real collector and Classic UI modules, verifies the collector shares a PlayerAPI even if metadata enrichment is off, asserts the original queue-update listener is attached, and proves that a queued song confirmed `DONE` calls `Player.removeFromQueue()` with the original queued UID. The RC58 PlayerAPI search regression and RC56 queue contract tests still run.

## Installation / verification

Install over RC58, restart Spotify with Debug Log enabled, play a playlist with already-downloaded tracks and verify they are removed from the **upcoming queue**, not the current track. The new log should have `queue player ready source=verified-cached-player skipDownloaded=1`; then `queue source=...` or `queue removed=N` will identify the next stage if live Spotify's queue schema differs. Native CI success alone cannot guarantee queue removal in the live application.
