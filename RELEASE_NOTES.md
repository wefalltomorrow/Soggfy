# Soggfy v3.0.0-rc.15

RC15 improves the Modern Capture status panel and makes the Spotify 1.3.3.264 playback-speed limitation clearer.

## Modern Capture status

Previously Ogg/Vorbis could remain shown as Unavailable until the entire Ogg stream had reached EOS, even though Soggfy had already parsed the native Vorbis header and knew the codec and sample rate.

RC15 now shows that information earlier as a detected stream:

- Current format can show `Ogg/Vorbis (detected)`.
- Sample rate can show values such as `44,100 Hz (detected)`.
- A new Capture association row reports:
  - `Pending - native stream detected`
  - `Matched - capturing`
  - `Complete`
  - `Ambiguous - waiting for unique stream`
  - `Unavailable`

The detected state is intentionally weaker than a validated match. Soggfy only surfaces an early decoder candidate when playback observation began near the track start, reducing the chance of confusing next-track read-ahead with the current song.

## Playback speed on Spotify 1.3.3.264

RC14 intentionally disabled the unsafe native speed hook on Spotify 1.3.3.264. RC15 now shows:

`1x (disabled for this Spotify build)`

instead of presenting a slider that looks adjustable but cannot safely be used.

The actual 1-50x hook remains limited to Spotify 1.3.1.234 until the 1.3.3.264 player ABI is separately revalidated.

The release remains one all-in-one Windows x64 ZIP.
