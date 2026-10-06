# Soggfy v3.0.0-rc.33

RC33 changes course on accelerated transport after comparing RC32 directly with the preserved original Soggfy source.

## What original Soggfy actually did

Original Soggfy did not contain any code that accelerated Spotify's visible scrubber or elapsed-time display. Its native DecodeAudioData hook simply decoded the full packet and then reduced the amount of PCM returned to Spotify according to the configured playback speed.

On the older Spotify client that original Soggfy targeted, reaching decoder EOS naturally caused Spotify to move to the next track. The TypeScript player code only watched for playbackId changes and treated those as track-end events. It did not manually advance after each completed download.

Original Soggfy also had explicit recovery for Spotify's `playback_stuck` error at very high speeds by resetting the current track.

## RC33 behavior

Current Spotify 1.3.3.264 no longer behaves exactly like that older client: Soggfy can consume the full compressed stream and prove EOS at 50x while Spotify's public transport remains near the beginning and does not move on.

RC33 therefore keeps the original UI behavior but emulates the original end result:

- Spotify's scrubber and elapsed-time display are no longer synthetically overridden.
- Accelerated native EOS remains the authoritative completion proof for capture/download.
- The moment that validated EOS is reached, Soggfy immediately clicks Spotify's own visible Next control exactly once.
- If the visible Next control cannot be found, Soggfy falls back to the internal `skipToNext()` API.
- There is no playbackId or URI gate that can silently suppress the action.
- Original Soggfy's `playback_stuck` reset handler is restored.

RC31's startup-race hardening, RC29's accelerated capture/publication logic and RC30's per-track status indicators remain unchanged.
