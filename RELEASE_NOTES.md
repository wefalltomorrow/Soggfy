# Soggfy v3.0.0-rc.60 — cap playback speed at 30x

The user confirmed 30x works on Spotify 1.3.1.234 after certain tracks repeatedly stalled at 50x. This update makes **30x the maximum** while keeping playback speeds 1x through 30x available.

## Changes

- The Classic playback-speed slider and numeric input now stop at **30x**.
- Existing `Playback Speed=31` through `Playback Speed=50` settings automatically load as 30x instead of reverting to 1x.
- The native settings setter rejects requests above 30x. The effective-speed policy and decoder hook also cap unexpected values.
- The playback stall watchdog operates within the same supported speed range.
- The build carries forward RC59's confirmed queue skipping and the separately tested multi-artist status matcher in PR61.

## Testing

C++ tests cover decoder PCM thinning and acceleration policy at 30x and the legacy higher values. JavaScript tests verify that UI synchronisation and native outbound speed messages cannot exceed 30x. The pre-existing queue, status, capture and recovery suites continue to run in CI.

The user confirmed 30x on the earlier build; the newly capped build still needs its own live check.

## Installation

Close Spotify, install the Windows x64 package over the current Soggfy version and restart Spotify. The playback speed slider will show **1x–30x**. Existing 50x preferences should load at 30x. Playback remains 1x whenever Downloads is disabled.
