# Soggfy v3.0.0-rc.43

RC43 fixes the remaining immediate Spotify startup crash seen with RC40-RC42.

The three new RC42 dumps all fail in the same place as the earlier dumps: execute access violation at 0x01F10B54, returning to Spotify.dll+0x5200C. The dump timestamps match the process creation timestamps, so the crash occurs in the very first second while Spotify.dll is still being initialized.

The previous RC42 change made the individual speed/connectivity paths safer, but we were still calling StartConnectivityHook and StartPlaybackSpeed as soon as GetModuleHandle could see Spotify.dll. A DLL can be visible before its loader initialization has completed, so that was still too early.

RC43 changes the startup model: Soggfy observes Spotify.dll but does not patch it until libcef.dll has loaded. That later CEF load is a practical post-loader readiness point because the Windows loader lock prevents another DLL load from completing while Spotify.dll is still inside its initialization path.

After CEF is present, Soggfy installs the connectivity compatibility hook, the validated 1.3.1.234 playback-speed hook, and the audio capture hook from its normal worker thread. The RC40-RC42 fixes remain otherwise unchanged.
