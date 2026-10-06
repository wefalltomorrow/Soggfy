# Soggfy v3.0.0-rc.29

RC29 fixes the follow-on bug exposed once RC28 finally made playback speed work: complete accelerated captures were reaching Ogg EOS, but Soggfy still refused to publish them because Spotify's public media-session timeline does not advance at the same rate as the decoder-thinning speed hack.

## What the RC28 test proved

The new decoder hook is definitely live. The log shows, for example:

- 3x: 2048 decoded samples -> 682 returned samples
- 17x: complete Ogg streams reaching EOS in roughly 1-2 wall-clock seconds
- 50x: complete long tracks reaching EOS in under 10 wall-clock seconds

So playback acceleration itself is now working correctly.

The download failure happened afterward. Spotify's SMTC timeline periodically refreshes from a much slower raw position. At 17x a track could go from an extrapolated position around 20 seconds back to around 5 seconds even though playback was progressing normally. The complete compressed stream was already in Soggfy's ready queue, but the listener validator interpreted that public-timeline refresh as a seek/rewind and invalidated the listen before publication.

## RC29 behavior

- Keeps RC28's working live `DecodeAudioData`-equivalent speed backend unchanged.
- Keeps normal 1x complete-listen validation unchanged.
- During accelerated playback only, a unique complete Ogg/FLAC stream matching the current track's duration can now prove natural completion.
- The stream must have reached EOS after the current listen began and must still satisfy the existing duration/start-window association rules.
- Once that exact completed stream is present, Soggfy no longer lets a stale SMTC position refresh turn it into a false `position_rewind` / `clock_discontinuity` failure.
- The existing publication queue, metadata tagging, duplicate checks, FFmpeg conversion and output path handling are unchanged.
- Added `accelerated_complete` diagnostics and `completion=capture_eos` to successful accelerated listen-completion records.

One separate case remains intentionally strict: if capture begins from an already-buffered/mid-track Spotify stream and the native Ogg sequence has a real page gap, Soggfy still rejects that incomplete stream instead of writing a corrupt file.
