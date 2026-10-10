# Soggfy v3.0.0-rc.53 — decoder stall diagnostic

**RC53 is a diagnostic build, not another playback retry or speed fix.** RC52 sometimes stops receiving compressed audio and PCM decoder callbacks at 50× while the Spotify session still reports playing. A synthetic position at the song duration is not evidence that the audio was captured.

## Why this instrumentation

Original Rafiuth/Soggfy hooks the compressed-audio decoder and adjusts a PCM output span (remaining bytes and pointer) in older x86 Spotify. The current fork hooks the validated x64 `snd-decoder` dispatcher and changes the reported sample count. These ABIs are different; copying pointer arithmetic from the original would be unsafe without a complete x64 ABI analysis.

The old Soggfy source also notes that Spotify can stop at speeds >=30× and listens for `playback_stuck`. Our logs show the callback/page counters freezing without that event. We need evidence of where the pipeline stops instead of adding more skip loops.

## What's new

When `DebugLog=1`, the history worker writes a bounded `decoder_probe` line approximately every two seconds while capture is enabled. It records:

- `enter`, `exit`, `inflight` and per-window `enter_delta`/`exit_delta`: distinguishes no calls from an original decoder call that entered but never returned.
- `last_exit_age_ms`: duration since the last completed decode call.
- `pcm_decoded_delta`/`pcm_kept_delta`: raw vs compressed PCM output from the existing 50× hook.
- `encoded_reported_delta`, `no_encoded_delta`: the decoder ABI's *reported encoded-count difference*, not a verified network byte count.
- `ogg_calls_delta`, `ogg_pages_delta`: independent compressed-audio capture activity over the same window.
- Last input/output counts, capacity and flags, plus the current track title and an **observational**, not causal, classification.

The classification may be `no_new_calls`, `call_unreturned_5s`, `calls_no_pcm`, `pcm_no_reported_input_delta` or `decode_progress`. Neither `no_new_calls` nor a zero encoded-count delta alone proves network starvation. Spotify's player/transport scheduler, decode thread, and cached-buffer internals are not instrumented yet.

**The decoder hook uses only lock-free atomic counters—no new hooks, heap allocation, locks, file I/O or UI actions in the audio callback.** All logging is on Soggfy's existing background history worker. The probe is off by default and enabled by the current DebugLog setting.

The RC52 recovery watchdog, 50× PCM manipulation, MP3 publication, SpotX integration, and RC51 playlist status UI remain unchanged.

## How to test

Keep your current Spotify 1.3.1.234 setup, enable `DebugLog=1` in `SpotifyHistory.ini` if it isn't already enabled, and reproduce a stalled song at 50×. Let it sit for 10–20 seconds, then provide `Soggfy.log`, including the `decoder_probe` lines before and during the stall. We can then decide whether to investigate a blocked decode call, Spotify no longer requesting decoding, or decoder calls that return without audio. The logging itself may slightly affect timing, so compare against RC52 before claiming a root cause.
