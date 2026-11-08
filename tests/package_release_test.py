"""Packaging failure must preserve delivered archives, including the same version."""
import pathlib
import shutil
import subprocess
import sys
import tempfile
import zipfile
import hashlib

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="soggfy-package-test-") as folder:
    fixture = pathlib.Path(folder)
    for name in ("VERSION", "README.md", "CHANGELOG.md", "RELEASE_NOTES.md", "UPSTREAMS.md",
                 "MILESTONES.md", "NATIVE_IDENTITY_AUDIT.md", "METADATA_REFRESH_DESIGN.md",
                 "LICENSE", "SpotifyHistory.ini", "package-release.py"):
        shutil.copy2(root / name, fixture / name)
    shutil.copytree(root / "Scripts", fixture / "Scripts")
    for vendor in ("minhook", "libogg", "cef", "soggfy"):
        target = fixture / "native/vendor" / vendor
        target.mkdir(parents=True)
        shutil.copy2(root / "native/vendor" / vendor / "LICENSE.txt", target / "LICENSE.txt")
    for vendor, name in (("libvorbis", "COPYING"), ("libFLAC", "COPYING.Xiph")):
        target = fixture / "native/vendor" / vendor
        target.mkdir(parents=True)
        shutil.copy2(root / "native/vendor" / vendor / name, target / name)
    (fixture / "build").mkdir()
    for name in ("version.dll", "Soggfy.exe", "BUILDINFO.txt"):
        (fixture / "build" / name).write_bytes(b"fixture")
    worker = fixture / "build/validation-worker/validation-worker.exe"
    worker.parent.mkdir()
    worker.write_bytes(b"test-worker")
    worker_hash = hashlib.sha256(worker.read_bytes()).hexdigest()
    worker_name = f"SoggfyValidation-{worker_hash}.exe"
    (fixture / "build/version.dll").write_bytes(b"fixture-bound-" + worker_hash.encode())
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
        assert package.read(worker_name) == package.read(f"Launcher/{worker_name}") == worker.read_bytes()
        assert package.read("VALIDATION_WORKER.txt") == (worker_hash + "\n").encode()
        for line in package.read("SHA256SUMS.txt").decode().splitlines():
            digest, name = line.split("  ", 1)
            assert hashlib.sha256(package.read(name)).hexdigest() == digest
        assert package.read("third-party/libvorbis/LICENSE.txt")
        assert package.read("third-party/libFLAC/LICENSE.txt")
    subprocess.run(command, check=True, capture_output=True)
    assert archive.read_bytes() == expected, "Nondeterministic archive"
    worker.write_bytes(b"substituted-worker")
    assert subprocess.run(command, capture_output=True).returncode != 0
    assert archive.read_bytes() == expected, "Mismatched helper replaced delivered archive"
    worker.write_bytes(b"test-worker")
    (fixture / "Scripts/Install.ps1").unlink()
    assert subprocess.run(command, capture_output=True).returncode != 0
    assert archive.read_bytes() == expected, "Failed packaging replaced delivered archive"
    assert previous.read_bytes() == b"previous-package"
    assert not list((fixture / "dist").glob("*.tmp"))
print("PASS: deterministic package, previous archive preservation and atomic failure cleanup")
