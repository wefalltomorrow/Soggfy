# Soggfy v3.0.0-rc.4

This is a build/release hardening update. The active Spotify capture/runtime code remains synced through Floggfy v1.1.0-rc.5.

Changes in this release:

- Builds the release DLL on Windows through a newer MSYS2 MinGW64 toolchain, with CI requiring GCC 14+ and binutils 2.44+.
- Pins the MSYS2 setup action revision and records the exact compiler/linker/resource-tool versions in `BUILDINFO.txt`.
- Keeps the MSVCRT target used by Floggfy RC5 instead of silently changing C runtime families.
- Adds a proper Windows VERSIONINFO resource to `version.dll`, including ProductName, FileDescription, FileVersion and ProductVersion.
- Publishes the raw `version.dll` as a release asset as well as the ZIP.
- Publishes `SHA256SUMS.txt` covering both the raw DLL and the final ZIP, plus `BUILDINFO.txt`.
- Keeps an internal `DLL-SHA256.txt` in the ZIP so the extracted DLL can be verified without creating a circular ZIP checksum.
- CI verifies the version resource, MSVCRT import and all release hashes before publication.

Runtime features from v3.0.0-rc.3 are unchanged: Floggfy RC5 current-track/footer fixes, native Ogg/FLAC capture, cached metadata enrichment, Soggfy path templates, installer/uninstaller, diagnostics and optional external FFmpeg post-processing.
