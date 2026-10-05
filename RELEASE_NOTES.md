# Soggfy v3.0.0-rc.28

RC28 fixes the playback-speed target again, this time by returning to the exact function old Soggfy actually modified: the decoder dispatcher that owns both the compressed input and decoded PCM output count.

## What the RC27 log proved

RC27/RC26 successfully enabled the experimental PCM filter-chain hook at `Spotify.dll+0x00463954`, but the live log contains no `speed_pcm_hook` callbacks at all while Ogg music is actively decoding and playing.

The configured rate changes from 26x to 14x, `speed_effective` remains 1x, and native Ogg capture continues normally. That proves the RC26 target is a real Spotify PCM helper but is not on the active music playback path used by this client.

## Re-analysis of old Soggfy's real speed method

The original x86 Soggfy hooked `DecodeAudioData`. After Spotify decoded a compressed packet, it deliberately changed the decoded-output span so the caller saw only:

`decoded_samples / playback_speed`

samples, while Spotify had still consumed the full compressed packet.

RC28 re-identified the x64 equivalent in the official Spotify 1.3.3.264 DLL:

- decoder dispatcher: `Spotify.dll+0x00d5a240`
- decoder vtable: `Spotify.dll+0x01ae5b28`
- dispatcher vtable slot: `Spotify.dll+0x01ae5b30`
- decoder constructor: `Spotify.dll+0x00d59a40`

The dispatcher is directly connected to the live Ogg path: at offset `+0x180` it calls the Ogg decoder routine that in turn calls the same Ogg page parser already observed by Soggfy's working native capture hook.

Its x64 ABI is also visible directly in the function:

- R8 = PCM float destination
- R9 = in/out PCM sample-count pointer
- stack arg 5 = compressed input pointer
- stack arg 6 = in/out compressed-byte count
- stack arg 7 = decode flags

At the end of the function Spotify writes the produced PCM sample count back through R9.

## RC28 behavior

- Removes Spotify 1.3.3.264 playback speed from the unused RC26 PCM filter helper.
- Hooks the exact live decoder dispatcher instead.
- Lets Spotify decode and consume the complete compressed packet first.
- Then reduces only the returned PCM sample count by the configured speed, matching old Soggfy's actual strategy.
- Keeps the compressed Ogg/FLAC capture path untouched.
- Validates the exact function prologue, live Ogg-decoder call, produced-sample store, constructor vtable assignment, vtable base and dispatcher slot before enabling the hook.
- Logs `speed_decode_hook` with PCM capacity, produced/kept samples, compressed input counts, requested speed and whether thinning actually occurred.
- `speed_effective` changes from 1x only after a real live decoder call has actually been thinned.

RC27's host-independent BlockTheSpot-style ad filtering remains unchanged.
