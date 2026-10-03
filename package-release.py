#!/usr/bin/env python3
"""Create one deterministic all-in-one Windows x64 release ZIP."""
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
for old_archive in out.glob("Soggfy-v*-Launcher-Windows-x64.zip"):
    old_archive.unlink()

dll = out / "version.dll"
launcher_exe = out / "Soggfy.exe"
launcher_dll = out / "Soggfy.dll"
build_info = out / "BUILDINFO.txt"

shutil.copy2(root / "build" / "version.dll", dll)
shutil.copy2(root / "build" / "Soggfy.exe", launcher_exe)
shutil.copy2(root / "build" / "version.dll", launcher_dll)
shutil.copy2(root / "build" / "BUILDINFO.txt", build_info)

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def add_bytes(archive: zipfile.ZipFile, arcname: str, data: bytes) -> None:
    info = zipfile.ZipInfo(arcname, (1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data)

binary_manifest = (
    f"{sha256(dll)}  version.dll\n"
    f"{sha256(launcher_exe)}  Launcher/Soggfy.exe\n"
    f"{sha256(launcher_dll)}  Launcher/Soggfy.dll\n"
    f"{sha256(build_info)}  BUILDINFO.txt\n"
).encode("utf-8")

files = [
    ("version.dll", dll),
    ("BUILDINFO.txt", build_info),
    ("SHA256SUMS.txt", None),
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
    ("Launcher/Soggfy.exe", launcher_exe),
    ("Launcher/Soggfy.dll", launcher_dll),
    ("Launcher/SpotifyHistory.ini", root / "SpotifyHistory.ini"),
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
        if arcname == "SHA256SUMS.txt":
            add_bytes(archive, arcname, binary_manifest)
        else:
            add_bytes(archive, arcname, path.read_bytes())

archive_digest = sha256(archive_path)
(out / "SHA256SUMS.txt").write_text(
    f"{archive_digest}  {release_name}\n",
    encoding="utf-8",
)

print(f"ZIP: {archive_path}")
print(f"ZIP SHA256: {archive_digest}")
print("Contains automatic version.dll plus Launcher/Soggfy.exe and Launcher/Soggfy.dll")
