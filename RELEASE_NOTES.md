# Soggfy v3.0.0-rc.36

RC36 fixes a concrete crash introduced by RC35's new hookless CEF browser-discovery path.

## Crash-dump result

The supplied dump is a 64-bit Spotify 1.3.3.264 crash with:

- exception: `0xC0000005` access violation;
- crashing thread: `CrBrowserMain`;
- faulting module: Soggfy's `VERSION.dll`;
- faulting offset: `+0x6D3E2`;
- invalid read address: `0x1`.

Disassembly at the fault shows RC35 called a CEF browser-host method, received `1`, then treated that value as a pointer and dereferenced it.

CEF 151's browser-host layout places:

- slot 8: `has_view()` -> integer;
- slot 9: `get_client()` -> client pointer.

RC35 accidentally called slot 8. A normal true return value became the bogus pointer `0x1`.

## RC36 fix

RC36:

- calls the correct browser-host `get_client` slot (9);
- verifies the CEF object is large enough to contain each method before reading the slot;
- verifies each function pointer points to executable memory before calling it;
- verifies the returned client ABI before attaching the console bridge;
- keeps RC35's post-start, hookless CEF discovery model.

No CEF MinHook detours are reintroduced. Telemetry request interception and the native To Disk menu remain disabled in this startup-safe branch while the black-screen issue is being isolated.

The accelerated-download and end-of-track behavior from RC33 remains unchanged.
