# Building Soggfy

## Release toolchain

Release DLLs are built on GitHub's Windows runner through MSYS2's MINGW64 environment.

The setup action is pinned to commit:

`ec48f7c5447b3140e2b088413ae3a55687bccb6e`

The release job requires:

- GCC 14 or newer
- GNU binutils 2.44 or newer
- the MinGW64/MSVCRT target

The MSVCRT target is intentional. Floggfy RC5's official DLL and the existing Soggfy builds use that runtime; switching the release build to UCRT at the same time as other build changes would alter the DLL's runtime dependency for no capture-engine benefit. MinGW support libraries are linked statically so the release DLL does not require `libwinpthread-1.dll` beside Spotify.

MSYS2's package repository is rolling, so the action pin alone does not make future builds bit-identical forever. Every release therefore includes `BUILDINFO.txt` with the exact compiler, linker, resource compiler and strip versions that produced it. CI also rejects a toolchain below the tested floor.

## Local reference build

A normal Debian/Ubuntu/WSL cross-build remains supported:

```bash
sudo apt install build-essential python3 nodejs mingw-w64
bash test-native.sh
bash build-native.sh
python3 package-release.py
```

Environment variables can override the cross tools:

```bash
CC=gcc CXX=g++ WINDRES=windres STRIP=strip LD=ld PYTHON=python bash build-native.sh
```

## Windows version resource

`build-native.sh` generates a VERSIONINFO resource from the repository `VERSION` file. The resulting `version.dll` exposes Soggfy's product name, file description and release version through Windows Properties while keeping the version-proxy export table unchanged.

For a prerelease such as `3.0.0-rc.4`, Windows' numeric version is `3.0.0.4` and the full prerelease string remains in FileVersion/ProductVersion.

## Release checksums

The release ZIP contains `DLL-SHA256.txt` for its embedded `version.dll`.

The separately published `SHA256SUMS.txt` covers:

- the raw `version.dll` release asset
- the final release ZIP
- `BUILDINFO.txt`

This avoids the circular problem of trying to put a ZIP's own checksum inside that same ZIP.
