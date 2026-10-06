# Soggfy v3.0.0-rc.34

RC34 targets the intermittent startup black-screen failure that still reproduced on RC33.

## What was still racing

RC31 combined the three initial CEF browser-creation hooks into one queued MinHook apply and delayed the telemetry hook. That removed some startup churn, but the Classic metadata/UI bridge still installed five additional MinHook detours later from inside CEF callbacks:

- client display getter
- client load getter
- console callback
- loading-state callback
- load-end callback

Those installs could happen while Chromium was actively creating Spotify's browser/compositor. MinHook temporarily suspends and resumes process threads while enabling a detour, so doing that from inside browser/client callbacks remained a plausible intermittent compositor deadlock/render failure.

## RC34 change

Those five callback hooks no longer use MinHook at all.

RC34 patches only the relevant function-pointer field on the concrete CEF C callback object. The original object is preserved, the original callback pointer is retained for forwarding, and no process-wide thread suspension occurs when those callbacks are attached.

The initial three audited browser-creation entry points are still enabled together in one queued MinHook apply so Soggfy can observe Spotify's main browser from its creation. After that, callback attachment is slot-only.

New debug lines look like:

`metadata callback display getter patched by object slot; no MinHook suspend`

`metadata callback load getter patched by object slot; no MinHook suspend`

`metadata callback loading patched by object slot; no MinHook suspend`

RC33's original-style accelerated transport behavior, including validated-EOS Next advancement and playback_stuck recovery, remains unchanged.
