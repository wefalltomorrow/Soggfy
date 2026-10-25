"""Packaging failure must preserve delivered archives, including the same version."""
import pathlib
import shutil
import subprocess
import sys
import tempfile
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="soggfy-package-test-") as folder:
    fixture = pathlib.Path(folder)
    for name in ("VERSION", "README.md", "CHANGELOG.md", "RELEASE_NOTES.md", "UPSTREAMS.md",
                 "LICENSE", "SpotifyHistory.ini", "package-release.py"):
        shutil.copy2(root / name, fixture / name)
    shutil.copytree(root / "Scripts", fixture / "Scripts")
    for vendor in ("minhook", "libogg", "cef", "soggfy"):
        target = fixture / "native/vendor" / vendor
        target.mkdir(parents=True)
        shutil.copy2(root / "native/vendor" / vendor / "LICENSE.txt", target / "LICENSE.txt")
    (fixture / "build").mkdir()
    for name in ("version.dll", "Soggfy.exe", "BUILDINFO.txt"):
        (fixture / "build" / name).write_bytes(b"fixture")
    (fixture / "dist").mkdir()
    previous = fixture / "dist/Soggfy-vold-Windows-x64.zip"
    previous.write_bytes(b"previous-package")
    command = [sys.executable, str(fixture / "package-release.py")]
    subprocess.run(command, check=True, capture_output=True)
    version = (fixture / "VERSION").read_text().strip()
    archive = fixture / f"dist/Soggfy-v{version}-Windows-x64.zip"
    expected = archive.read_bytes()
    with zipfile.ZipFile(archive) as package:
        assert package.testzip() is None
        assert package.read("version.dll") == package.read("Launcher/Soggfy.dll")
    subprocess.run(command, check=True, capture_output=True)
    assert archive.read_bytes() == expected, "Nondeterministic archive"
    (fixture / "Scripts/Install.ps1").unlink()
    assert subprocess.run(command, capture_output=True).returncode != 0
    assert archive.read_bytes() == expected, "Failed packaging replaced delivered archive"
    assert previous.read_bytes() == b"previous-package"
    assert not list((fixture / "dist").glob("*.tmp"))
print("PASS: deterministic package, previous archive preservation and atomic failure cleanup")
