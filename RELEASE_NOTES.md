# Soggfy v3.0.0-rc.25

RC25 fixes the immediate startup crash introduced by RC24's loader-readiness check.

## RC24 dump diagnosis

Both supplied RC24 dumps fail identically:

- exception: `0xc0000005` write access violation
- faulting instruction: `VERSION.dll+0x217e`
- destination: `Spotify.dll+0x01953738`
- that destination is Spotify's `GetCommandLineW` IAT slot

The faulting instruction is:

`lock cmpxchg [rdx], rax`

RC24 used `InterlockedCompareExchangePointer(slot, nullptr, nullptr)` as an atomic read of the IAT entry. That API is still a read-modify-write operation, so it faults when the IAT page is read-only.

## RC25 behavior

- Reads Spotify IAT entries with a plain read-only memory load instead of an interlocked RMW instruction.
- Keeps RC24's loader-readiness gate:
  - critical normal imports must already resolve to executable addresses;
  - raw unresolved import RVAs are rejected;
  - a further 1500 ms grace period is required before native hooks begin.
- Keeps RC23's rule that memory-scanned SessionTrackPlayer candidates are diagnostic-only.

No playback-speed or capture logic is otherwise changed in this build.
