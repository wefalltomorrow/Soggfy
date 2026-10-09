# Soggfy v3.0.0-rc.46

RC46 replaces the Spotify 1.3.1.234 playback-speed implementation rather than trying another queue-reset workaround.

The RC45 log showed the failure precisely: The Magumba State was genuinely decoding at 50x, but after Spotify advanced naturally to Empty Branes the native player reported the configured 50x while the raw playback clock stayed near zero. RC41 then recreated the track, causing a large position rewind and eventually leaving playback paused.

The reason is architectural: the RC38-RC45 speed code changed a speed argument when Spotify constructed a track player. Spotify can pre-create/reuse the next player during a natural transition, bypassing that path. Manual Next creates a fresh player, which is why it appeared to fix the speed.

RC46 returns to the mechanism used by original Soggfy. We reverse-engineered the exact Spotify 1.3.1.234 x64 snd-decoder and hook its live DecodeAudio dispatcher. Spotify consumes the full compressed input normally; after the decoder returns, Soggfy reduces only the number of PCM float samples handed to playback by the configured 1x-50x factor. Because this happens on every decode callback, it applies to every track and every pre-created player without touching the queue.

The hook is fail-closed and validates the exact decoder name, vtable/slot pointers, function prologue, live decoder-call anchor and produced-sample-count store before MinHook is enabled.

RC41's natural-transition reset/watchdog has been removed. Original Soggfy's targeted playback_stuck recovery is restored instead. The installer also now pauses when launched directly from Explorer so successful/error output can be read, while normal terminal usage remains non-blocking.

RC45 SpotX failure handling and RC44 startup loader-readiness protections remain unchanged.
