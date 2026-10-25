"""Real Windows/FFmpeg regressions; only touches a private temporary directory."""
import os
import pathlib
import shutil
import subprocess
import tempfile
import time

assert os.name == "nt", "Run this native Windows process test with Windows Python"
root = pathlib.Path(__file__).resolve().parents[1]
pwsh = os.environ.get("SGF_PWSH_PATH") or shutil.which("pwsh")
ffmpeg = os.environ.get("SGF_REAL_FFMPEG") or shutil.which("ffmpeg")
assert pwsh and ffmpeg, "PowerShell and real FFmpeg are required"
script = root / "Scripts/PostProcess.ps1"

def wait_for(predicate, seconds=25):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError("Timed out waiting for watcher recovery")

def quote(path):
    return "'" + str(path).replace("'", "''") + "'"

def command(path, encoder=ffmpeg, *extra):
    return [pwsh, "-NoProfile", "-File", str(script), "-InputPath", str(path),
            "-FFmpegPath", str(encoder), *extra]

with tempfile.TemporaryDirectory(prefix="soggfy-watch-test-") as folder:
    base = pathlib.Path(folder)
    # Incomplete packages must fail before looking for Spotify or stopping it.
    package = base / "package"
    (package / "Scripts").mkdir(parents=True)
    installer = package / "Scripts/Install.ps1"
    shutil.copy2(root / "Scripts/Install.ps1", installer)
    for empty_dll in (False, True):
        if empty_dll:
            (package / "version.dll").write_bytes(b"")
        preflight = subprocess.run([pwsh, "-NoProfile", "-File", str(installer),
            "-SpotifyPath", str(base / "nonexistent-spotify"), "-SkipSpotX"],
            capture_output=True, text=True, timeout=20)
        assert preflight.returncode != 0
        assert "version.dll is missing or empty" in preflight.stderr, preflight.stderr
    fixture = base / "fixture.flac"
    subprocess.run([ffmpeg, "-v", "error", "-f", "lavfi", "-i",
                    "sine=frequency=440:duration=5", "-c:a", "flac", str(fixture)], check=True)
    good = fixture.read_bytes()
    subprocess.run(command("fixture.flac"), cwd=base, check=True, capture_output=True)
    assert fixture.with_suffix(".mp3").exists(), "Relative file path failed"
    fixture.with_suffix(".mp3").unlink()
    invalid = subprocess.run(command(fixture, ffmpeg, "-Watch", "-DeleteOriginal"), capture_output=True)
    assert invalid.returncode != 0 and fixture.read_bytes() == good
    assert not fixture.with_suffix(".mp3").exists(), "Invalid watch changed source before validation"
    damaged = base / "damaged.flac"
    broken = bytearray(good)
    middle = len(broken) // 2
    broken[middle:middle+128] = bytes(128)
    damaged.write_bytes(broken)
    strict = subprocess.run([ffmpeg, "-v", "error", "-xerror", "-i", str(damaged), "-f", "null", "-"], capture_output=True)
    assert strict.returncode != 0, "Fixture must have corrupt audio frames"
    result = subprocess.run(command(damaged, ffmpeg, "-DeleteOriginal"), capture_output=True)
    assert result.returncode != 0 and damaged.read_bytes() == broken
    assert not damaged.with_suffix(".mp3").exists(), "Damaged source published partial audio"
    watch = base / "watch"
    watch.mkdir()
    marker = base / "encoding"
    wrapper = base / "slow.ps1"
    wrapper.write_text("if ($args -contains '-y') { Set-Content -LiteralPath " + quote(marker) +
                       " -Value ready; Start-Sleep -Seconds 2 }; & " + quote(ffmpeg) +
                       " @args; $global:LASTEXITCODE = $LASTEXITCODE", encoding="utf-8")
    logpath = base / "watch.log"
    with logpath.open("w") as log:
        process = subprocess.Popen(command(watch, wrapper, "-Watch"), stdout=log, stderr=log)
        try:
            wait_for(lambda: "Watching " in logpath.read_text(errors="replace"))
            (watch / "first.flac").write_bytes(good)
            wait_for(marker.exists)
            (watch / "second.flac").write_bytes(good)
            (watch / "permanent.flac").write_bytes(broken)
            wait_for(lambda: (watch / "first.mp3").exists() and (watch / "second.mp3").exists())
            # Stable size while still writable must defer, then recover after close.
            writer = base / "writer.ps1"
            locked = watch / "busy.flac"
            writer.write_text("$s=[IO.File]::Open(" + quote(locked) +
                ",[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite);try{$b=[IO.File]::ReadAllBytes(" +
                quote(fixture) + ");$s.Write($b,0,$b.Length);$s.Flush();Start-Sleep -Seconds 5}finally{$s.Dispose()}", encoding="utf-8")
            subprocess.run([pwsh, "-NoProfile", "-File", str(writer)], check=True, timeout=15)
            wait_for(lambda: locked.with_suffix(".mp3").exists())
            assert process.poll() is None, logpath.read_text(errors="replace")
            assert (watch / "permanent.flac").read_bytes() == broken
            assert not (watch / "permanent.mp3").exists()
        finally:
            process.terminate()
            process.wait(timeout=10)
    # A wrapper may have been encoding when the watcher stopped; no original is deleted.
    assert fixture.read_bytes() == good
print("PASS: relative paths, invalid watch preflight, corrupt source retention, arrivals during encoding and busy-file recovery")
