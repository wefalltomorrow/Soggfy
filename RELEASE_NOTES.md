# Soggfy v3.0.0-rc.36

RC36 fixes the crash introduced by RC35 and pushes the remaining invasive Spotify.dll hooks well past application startup.

## RC35 crash root cause

The RC35 crash dump is definitive:

- Exception: `0xC0000005` access violation.
- Faulting module: Soggfy `VERSION.dll`.
- Fault offset: `VERSION.dll+0x6D3E2`.
- Faulting access: read from address `0x1`.

The faulting instruction was the CEF client-size check. RC35's new hookless browser discovery obtained the browser host and then called browser-host method slot 8 as though it were `get_client`.

On CEF 151.3.18, browser-host slot 8 is actually `has_view()`; `get_client()` is slot 9. A normal windowed Spotify browser returned `1` from `has_view()`, and RC35 then treated that integer as a client pointer.

RC36 uses the correct slot 9 and validates that the browser-host object exposes the required prefix before calling it.

## Black-screen isolation

The black screen occurred before RC35's delayed browser-discovery crash, so the crash itself does not explain the initial display failure.

RC35 already removed every CEF MinHook detour. RC36 therefore targets the remaining startup-time MinHook activity in Spotify.dll:

- connectivity repair remains early because it is a direct IAT pointer patch;
- playback-speed MinHook installation is delayed until 10 seconds after the post-loader stage begins;
- audio/capture MinHook installation is delayed until 11 seconds after that stage begins.

Combined with the existing 2.5-second loader grace, Spotify and Chromium should have roughly 12-14 seconds to finish normal startup before any Spotify.dll MinHook detour is enabled.

RC35's hookless CEF bridge and RC33's validated accelerated-EOS Next-track behavior remain present.
