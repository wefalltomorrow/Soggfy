# Soggfy v3.0.0-rc.14

RC14 fixes the startup crash on Spotify for Windows x64 1.3.3.264.

## What the crash dumps showed

Three independent dumps from Spotify 1.3.3.264 with Soggfy RC13 all failed the same way: an execute access violation at the same unmapped low address during very early Spotify.dll startup. Floggfy v1.1.0 works on this same Spotify build, and the extra early native hook present in Soggfy is the restored playback-speed hook.

The old speed-target pattern still produced a unique match on 1.3.3.264, but the player constructor ABI/layout is no longer safe to call with the 1.3.1-era signature. That meant the old "unique pattern = safe" validation was not strict enough.

## RC14 behavior

- Spotify 1.3.3.264 no longer installs the unsafe playback-speed hook.
- Soggfy therefore runs at 1x on this build, matching Floggfy's working playback path.
- Capture, Classic UI, Skip Downloaded, metadata and FFmpeg conversion remain enabled.
- The speed control is disabled in the UI when the build is not explicitly validated.
- Spotify 1.3.1.234 keeps the native 1-50x speed feature because that ABI was runtime-validated.
- Unknown/new Spotify versions also fail closed at 1x instead of risking a startup crash.

Once normal downloading is confirmed on 1.3.3.264, the speed hook can be reworked separately against the new player ABI.

The release remains one all-in-one Windows x64 ZIP.
