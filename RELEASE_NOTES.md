# Soggfy v3.0.0-rc.56 — original Skip Downloaded Tracks behaviour

RC56 restores original Rafiuth/Soggfy's queue-based skip flow using modern Spotify 1.3.1.x track/queue data. It does not introduce a separate database, alternate library source, or automated skipping of currently playing music.

## Root cause in RC55

Original Soggfy `Sprinkles/src/player-state-tracker.ts` listens to Spotify's `queue_update` event, reads **`data.nextUp`**, creates a templated file-status lookup for queued tracks, caches confirmed matches by URI, then passes downloaded `ContextTrack` entries to **`Player.removeFromQueue()`**.

RC55 ignored event `data`, instead retrieving `player._queue.getQueue().queued`; that is not guaranteed to contain the upcoming `nextUp` tracks in Spotify 1.3.x. RC55 also set a whole-queue signature **before** checking files: missing metadata, empty lookup responses and timeouts could prevent any subsequent check for an unchanged queue.

## Behaviour in RC56

- Use `queue_update` **`data.nextUp`** directly when supplied, exactly like upstream. For modern Spotify builds with different queue representations, accept `event.nextTracks`, `Spicetify.Queue.nextTracks`, raw `PlayerState.next_tracks`, or native `_queue.getQueue()` variants when event `nextUp` is unavailable.
- Map original `name`/`artists`/`album` and modern `metadata.title`/`artist_name`/`album_title` to the **existing file-based status matcher**. Never mark a track downloaded merely because it was played before.
- Cache *only valid native lookup responses* per queued track URI, and forget them after tracks leave the upcoming queue. A timed-out/partial lookup remains unverified and will be retried. The native 128-entry batch limit is handled in chunks so larger queues are not silently truncated.
- Remove only upcoming tracks with status **DONE** via `Player.removeFromQueue()`, preserving each queue entry's **uid** when available so repeat occurrences are handled correctly. Retain the original ignore-list check and don't skip currently playing tracks.
- When `DebugLog` is enabled, use the existing Soggfy `classic status` diagnostics to record the queue source, queue counts and successful removal attempts without track names or a new logging system.

## Regression tests

`tests/classic_skip_downloaded_test.js` verifies the authoritative `queue_update.nextUp` path despite an unrelated empty `getQueue().queued`, older and modern track metadata, UID-aware removal, unverified native response retries, cache re-use, the modern queue fallbacks, duplicate track entries, ignored tracks, missing snapshot retries and >128-song batches.

Retained unchanged: original file-based icon detection from RC55, RC54 50×/1× listen mode, RC53 decoder diagnostics, RC52 playback stall recovery, RC51 status icons, 50× decoder algorithm and MP3 conversion.
