# Soggfy v3.0.0-rc.35

RC35 is a startup-stability isolation build for the intermittent full-primary-monitor black screen that still reproduced on RC34.

## CEF bridge redesign

RC34 removed the five callback-time MinHook installs, but three initial CEF browser-creation entry hooks still remained. The CEF URL-request filter was also still a MinHook detour.

RC35 removes those remaining CEF MinHook operations completely.

Instead of intercepting browser creation, the Classic bridge now:

1. Initializes only lightweight state when `libcef.dll` appears.
2. Waits 8 seconds so Spotify/Chromium can finish creating its compositor and main browser.
3. Posts a task onto CEF's UI thread.
4. Discovers the already-created Spotify browser using `cef_browser_host_get_browser_by_identifier`.
5. Gets the browser's client/display handler and patches only the concrete console callback pointer.
6. Gets the existing main frame and injects the normal Soggfy metadata/Classic UI scripts directly.

There is no MinHook enable/apply anywhere in this CEF bridge path.

## Startup-safe feature tradeoff

To make this test decisive, RC35 also does not install:

- the CEF URL-request/telemetry MinHook;
- the optional native To Disk menu MinHooks.

The Block Telemetry preference is still saved and shown, but native CEF request blocking is temporarily inactive in this build. Classic UI, downloads, metadata, capture, conversion, playback speed, status indicators and RC33's accelerated-end Next behavior remain present.

If RC35 still black-screens, the remaining likely source is no longer the CEF integration and the next isolation target is the Spotify.dll MinHook family itself.
