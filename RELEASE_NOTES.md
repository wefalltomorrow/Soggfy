# Soggfy v3.0.0-rc.5

This release changes the Spotify-facing integration to behave like old Soggfy instead of Floggfy.

The modern x64 capture engine remains synced through Floggfy v1.1.0-rc.5. Native Ogg/FLAC capture, complete-listen validation, dynamic hook discovery, cache metadata, quality safeguards and the hardened rc.4 build/release pipeline remain underneath.

What changes for users:

- The default UI is now a modern reimplementation of old Soggfy's **top-bar integration**.
- Spotify gets a **Downloads** button in the top bar. Its icon/state reflects whether capture is enabled.
- A **Soggfy settings** button beside it opens an in-client Soggfy settings modal.
- The modal controls Downloads, native FLAC/Ogg capture, save location, path template, artist-separator handling, cached metadata enrichment, activity logging and debug logging.
- Settings are written back to `SpotifyHistory.ini` immediately through the native bridge.
- The native Floggfy **To Disk** submenu is suppressed while Classic UI is enabled.
- The To Disk menu remains available as a troubleshooting fallback with `Classic UI=0` and `Native Menu=1`.
- Metadata can be disabled without disabling the Classic Soggfy UI itself.

This intentionally does not resurrect the obsolete Soggfy x86/WebSocket architecture. The UI talks directly to the current in-process x64 backend.

Legacy features that require separate backend work (such as the old accelerated-playback downloader, M3U/context-menu workflow and per-track row status/file lookup) are not silently faked in this release.
