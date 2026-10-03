#!/usr/bin/env python3
"""Create deterministic automatic and launcher Windows x64 release packages."""
import hashlib
from pathlib import Path
import shutil
import zipfile

root = Path(__file__).resolve().parent
VERSION = (root / "VERSION").read_text(encoding="utf-8").strip()
out = root / "dist"
out.mkdir(exist_ok=True)
automatic_name = f"Soggfy-v{VERSION}-Windows-x64.zip"
launcher_name = f"Soggfy-v{VERSION}-Launcher-Windows-x64.zip"

for old_archive in out.glob("Soggfy-v*-Windows-x64.zip"):
    if old_archive.name not in {automatic_name, launcher_name}:
        old_archive.unlink()
for old_archive in out.glob("Soggfy-v*-Launcher-Windows-x64.zip"):
    if old_archive.name != launcher_name:
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

def add_bytes(archive: zipfile.ZipFile, arcname: str, data: bytes) -> None:
    info = zipfile.ZipInfo(arcname, (1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data)

dll_digest = sha256(dll)
dll_manifest = out / "DLL-SHA256.txt"
dll_manifest.write_text(f"{dll_digest}  version.dll\n", encoding="utf-8")

automatic_files = [
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
    automatic_files.append(
        (
            f"third-party/{vendor}/LICENSE.txt",
            root / "native" / "vendor" / vendor / "LICENSE.txt",
        )
    )

automatic_path = out / automatic_name
with zipfile.ZipFile(automatic_path, "w") as archive:
    for arcname, path in automatic_files:
        add_bytes(archive, arcname, path.read_bytes())

launcher_exe = (root / "build" / "Soggfy.exe").read_bytes()
launcher_dll = (root / "build" / "version.dll").read_bytes()
launcher_manifest = (
    f"{hashlib.sha256(launcher_exe).hexdigest()}  Soggfy.exe\n"
    f"{hashlib.sha256(launcher_dll).hexdigest()}  Soggfy.dll\n"
).encode("utf-8")
launcher_payloads = {
    "Soggfy.exe": launcher_exe,
    "Soggfy.dll": launcher_dll,
    "SHA256SUMS.txt": launcher_manifest,
    "BUILDINFO.txt": build_info.read_bytes(),
    "VERSION": (root / "VERSION").read_bytes(),
    "README.md": (root / "README.md").read_bytes(),
    "CHANGELOG.md": (root / "CHANGELOG.md").read_bytes(),
    "RELEASE_NOTES.md": (root / "RELEASE_NOTES.md").read_bytes(),
    "UPSTREAMS.md": (root / "UPSTREAMS.md").read_bytes(),
    "LICENSE": (root / "LICENSE").read_bytes(),
    "SpotifyHistory.ini": (root / "SpotifyHistory.ini").read_bytes(),
}
for vendor in ["minhook", "libogg", "cef", "soggfy"]:
    launcher_payloads[f"third-party/{vendor}/LICENSE.txt"] = (
        root / "native" / "vendor" / vendor / "LICENSE.txt"
    ).read_bytes()

launcher_path = out / launcher_name
with zipfile.ZipFile(launcher_path, "w") as archive:
    for arcname, data in launcher_payloads.items():
        add_bytes(archive, arcname, data)

automatic_digest = sha256(automatic_path)
launcher_digest = sha256(launcher_path)
build_info_digest = sha256(build_info)
(out / "SHA256SUMS.txt").write_text(
    f"{dll_digest}  version.dll\n"
    f"{automatic_digest}  {automatic_name}\n"
    f"{launcher_digest}  {launcher_name}\n"
    f"{build_info_digest}  BUILDINFO.txt\n",
    encoding="utf-8",
)

print(f"Automatic DLL: {dll}")
print(f"DLL SHA256: {dll_digest}")
print(f"Automatic ZIP: {automatic_path}")
print(f"Automatic ZIP SHA256: {automatic_digest}")
print(f"Launcher ZIP: {launcher_path}")
print(f"Launcher ZIP SHA256: {launcher_digest}")
print(f"BUILDINFO: {build_info}")
