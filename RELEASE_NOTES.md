# Soggfy v3.0.0-rc.24

RC24 fixes the startup crash that remained in RC23 by moving Spotify-native hook installation out of the loader race entirely.

## What the RC23 crash dump proved

The supplied RC23 minidump is definitely running Soggfy 3.0.0.23 with Spotify 1.3.3.264.

It reproduces the same signature seen in RC22 and the earlier RC13 crashes:

- exception: `0xc0000005` execute access violation
- attempted address: `0x01f7e69c`
- immediate return address: `Spotify.dll+0x00052a0c`

Disassembly of the exact Spotify 1.3.3.264 DLL shows that `Spotify.dll+0x00052a06` is:

`call [Spotify.dll+0x01953738]`

and that IAT slot is Spotify's normal import for `GetCommandLineW`.

The value `0x01f7e69c` is not a valid Windows function pointer. It is the raw PE hint/name RVA that exists in the import thunk before the Windows loader resolves that entry.

So the crash is not caused by the latest SessionTrackPlayer object validation itself. Soggfy was beginning MinHook/IAT work as soon as `Spotify.dll` became visible in the module list, which can happen while Windows is still resolving that DLL's normal imports.

## RC24 behavior

- Does not install any Spotify.dll-native hooks merely because the module is visible.
- Verifies five normal Spotify imports are already resolved to committed executable addresses:
  - `GetCommandLineW`
  - `GetCurrentProcessId`
  - `GetModuleHandleW`
  - `GetProcAddress`
  - `VirtualProtect`
- Rejects raw unresolved import RVAs before any native hook is installed.
- Requires a further 1500 ms quiet grace period after those imports become resolved.
- Only after that gate opens does Soggfy start:
  - the connectivity IAT hook,
  - the playback-speed hook,
  - playback-speed maintenance,
  - and native audio-history hooks.
- The log records:
  `Spotify.dll normal imports resolved; deferred native hooks released after 1500 ms loader grace`
  when native hook installation is finally allowed.
- RC23's safer rule remains: memory-scanned SessionTrackPlayer candidates are diagnostic-only and are never called unless Spotify itself exposes the object through the hooked speed methods.

Capture/conversion behavior is otherwise unchanged.
