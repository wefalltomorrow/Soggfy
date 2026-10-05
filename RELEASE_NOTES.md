# Soggfy v3.0.0-rc.21

RC21 replaces RC20's unused AudioSessionImpl track-creation hook with the live Spotify 1.3.3.264 SessionTrackPlayer speed path.

## What RC20 proved

RC20 successfully installed its exact hook at Spotify.dll+0x0057968c, but a live run never entered that routine. The configured rate changed to 13x and later 5x, and multiple new music tracks started, while the verified effective rate stayed at 1x. There was no `Spotify 1.3.3 track-player create` event at all.

That means the function identified in RC20 is a real Spotify track-player creation routine, but it is not the route used by the active music session in this client configuration.

## Revalidated live SessionTrackPlayer path

Further analysis of the same official Spotify 1.3.3.264 DLL found the live SessionTrackPlayer implementation:

- object vtable: Spotify.dll+0x01a07308
- `setPlaybackSpeed`: Spotify.dll+0x005a8d18, vtable slot +0xc0
- playback-speed getter: Spotify.dll+0x005a17d8, vtable slot +0xc8
- constructor vtable assignment: Spotify.dll+0x0059c09b
- the setter receives the requested rate in XMM1 and dispatches the change through Spotify's own playback worker
- the getter returns the underlying live player's rate, or the SessionTrackPlayer cached rate before the underlying player exists

RC21 validates all of those exact relationships before enabling the backend.

## RC21 behavior

- Hooks Spotify's native SessionTrackPlayer setter and getter so Soggfy can identify the live session without guessing from AudioSessionImpl.
- Overrides Spotify-originated speed setter calls with the configured Soggfy rate while accelerated playback is enabled.
- If Spotify has not called the setter/getter yet, scans for the exact validated SessionTrackPlayer vtable as a fallback and applies the rate through Spotify's own setter.
- Keeps known SessionTrackPlayer objects across playback and rescans periodically to catch track/session replacement.
- Reads Spotify's real SessionTrackPlayer getter and only reports `speed_effective` after the live player actually reports the requested rate.
- Logs `speed_session_candidate`, `speed_session_scan`, and `speed_verified` / `speed_pending` records so a failed attempt now tells us whether the live object was found, whether it has an underlying player, its native rate, and whether playback-speed automation is active.
- Speed changes can apply to the current live track; RC21 no longer needs the Classic UI to force a track recreation.

Spotify has a separate playback-speed automation mechanism. Its own SessionTrackPlayer setter deliberately refuses non-1x manual speed while that automation is active. RC21 reports the number of active automation entries rather than pretending the rate changed. Disable Spotify **Automix** under **Edit -> Preferences -> Playback** while testing/downloading; upstream Floggfy now recommends this as well because Automix trims tracks and breaks complete-listen capture.

Spotify 1.3.1.234 keeps the older validated constructor backend. Unknown Spotify builds still fail closed at 1x.
