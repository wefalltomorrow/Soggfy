#!/usr/bin/env python3
"""Create a deterministic Windows x64 release package and checksum manifest."""
import hashlib
from pathlib import Path
import shutil
import zipfile

root = Path(__file__).resolve().parent
VERSION = (root / "VERSION").read_text(encoding="utf-8").strip()
out = root / "dist"
out.mkdir(exist_ok=True)
release_name = f"Soggfy-v{VERSION}-Windows-x64.zip"

for old_archive in out.glob("Soggfy-v*-Windows-x64.zip"):
    if old_archive.name != release_name:
        old_archive.unlink()

dll = out / "version.dll"
build_info = out / "BUILDINFO.txt"
shutil.copy2(root / "build" / "version.dll", dll)
shutil.copy2(root / "build" / "BUILDINFO.txt", build_info)

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

dll_digest = sha256(dll)
dll_manifest = out / "DLL-SHA256.txt"
dll_manifest.write_text(f"{dll_digest}  version.dll\n", encoding="utf-8")

files = [
    ("version.dll", dll),
    ("DLL-SHA256.txt", dll_manifest),
    ("BUILDINFO.txt", build_info),
    ("VERSION", root / "VERSION"),
    ("README.md", root / "README.md"),
    ("CHANGELOG.md", root / "CHANGELOG.md"),
    ("RELEASE_NOTES.md", root / "RELEASE_NOTES.md"),
    ("UPSTREAMS.md", root / "UPSTREAMS.md"),
    ("LICENSE", root / "LICENSE"),
    ("SpotifyHistory.ini", root / "SpotifyHistory.ini"),
    ("Scripts/Install.ps1", root / "Scripts" / "Install.ps1"),
    ("Scripts/Uninstall.ps1", root / "Scripts" / "Uninstall.ps1"),
    ("Scripts/PostProcess.ps1", root / "Scripts" / "PostProcess.ps1"),
    ("Scripts/Diagnose.ps1", root / "Scripts" / "Diagnose.ps1"),
]
for vendor in ["minhook", "libogg", "cef", "soggfy"]:
    files.append(
        (
            f"third-party/{vendor}/LICENSE.txt",
            root / "native" / "vendor" / vendor / "LICENSE.txt",
        )
    )

archive_path = out / release_name
with zipfile.ZipFile(archive_path, "w") as archive:
    for arcname, path in files:
        info = zipfile.ZipInfo(arcname, (1980, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, path.read_bytes())

zip_digest = sha256(archive_path)
build_info_digest = sha256(build_info)
(out / "SHA256SUMS.txt").write_text(
    f"{dll_digest}  version.dll\n"
    f"{zip_digest}  {release_name}\n"
    f"{build_info_digest}  BUILDINFO.txt\n",
    encoding="utf-8",
)

print(f"DLL: {dll}")
print(f"DLL SHA256: {dll_digest}")
print(f"ZIP: {archive_path}")
print(f"ZIP SHA256: {zip_digest}")
print(f"BUILDINFO: {build_info}")
