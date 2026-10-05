# Soggfy v3.0.0-rc.22

RC22 fixes the SessionTrackPlayer discovery mistake exposed by the RC21 live log.

## What RC21 proved

RC21 successfully enabled the exact Spotify 1.3.3.264 SessionTrackPlayer setter/getter backend, and every scan found exactly one object whose first pointer matched the validated SessionTrackPlayer vtable. However, RC21 reported `raw_hits=1 valid=0` continuously — including while a new track was actively playing — so it rejected the real object before ever calling Spotify's speed setter.

The rejection came from an over-strict safety check: RC21 required the SessionTrackPlayer's dispatcher and underlying-player virtual methods to live inside Spotify.dll. Those fields are interface objects and their implementations can live in another executable module even though the SessionTrackPlayer itself is the exact byte-validated Spotify class.

## RC22 changes

- Keeps the exact Spotify 1.3.3.264 SessionTrackPlayer vtable match at `Spotify.dll+0x01a07308`.
- Keeps exact byte/vtable validation of Spotify's native speed setter at `+0x005a8d18` and getter at `+0x005a17d8`.
- Accepts dispatcher/player virtual methods from any committed executable module instead of incorrectly requiring every implementation to reside inside Spotify.dll.
- Still requires the SessionTrackPlayer's exact vtable, readable object fields, a sane automation vector, a readable dispatcher vtable/method, and a readable underlying-player getter when a player exists.
- Actually calls Spotify's setter even when playback-speed automation entries are present; Spotify itself can accept or reject the request, and Soggfy then verifies the result with the native getter.
- Adds a detailed `speed_session_raw` diagnostic for any exact-vtable object that still fails validation. It records dispatcher/player pointers, virtual method executability, automation-vector size and cached speed so another false-negative can be diagnosed from one log.
- `speed_effective` remains conservative: it changes from 1x only when Spotify's live underlying player reports the requested rate.

As before, disable Spotify **Automix** under **Edit -> Preferences -> Playback** while testing complete-listen capture.
