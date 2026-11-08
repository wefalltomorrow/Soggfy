"""Windows process supervision, wrapper quoting, and watcher deadline recovery."""
import ctypes
import os
import pathlib
import shutil
import subprocess
import tempfile
import time

assert os.name == "nt"
root = pathlib.Path(__file__).resolve().parents[1]
pwsh = os.environ.get("SGF_PWSH_PATH") or shutil.which("pwsh")
ffmpeg = os.environ.get("SGF_REAL_FFMPEG") or shutil.which("ffmpeg")
assert pwsh and ffmpeg
script = root / "Scripts/PostProcess.ps1"
converter = root / "build/post-process-test.exe"
assert converter.is_file()

def quote(value):
    return "'" + str(value).replace("'", "''") + "'"

def wait_for(predicate, seconds=25):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError("Timed out waiting for process test")

def dead(pid):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.OpenProcess(0x100000, False, pid)
    if not handle:
        assert ctypes.get_last_error() == 87
        return True
    try:
        return kernel.WaitForSingleObject(handle, 0) == 0
    finally:
        kernel.CloseHandle(handle)

def command(path, encoder, *extra):
    return [pwsh, "-NoProfile", "-File", str(script), "-InputPath", str(path),
            "-FFmpegPath", str(encoder), *extra]

with tempfile.TemporaryDirectory(prefix="soggfy-process-test-") as folder:
    base = pathlib.Path(folder) / "音楽 & O'Brien $cash [test]"
    base.mkdir()
    fixture = base / "原曲 & ' $track.flac"
    subprocess.run([ffmpeg, "-v", "error", "-f", "lavfi", "-i",
                    "sine=frequency=440:duration=1", "-c:a", "flac", str(fixture)], check=True)
    good = fixture.read_bytes()
    wrapper = base / "encoder & ' $wrapper.ps1"
    # A subprocess wrapper must preserve literal arguments and exit status.
    wrapper.write_text("& " + quote(ffmpeg) + " @args; exit $LASTEXITCODE", encoding="utf-8-sig")
    result = subprocess.run(command(fixture, wrapper), capture_output=True, timeout=20)
    assert result.returncode == 0, result.stderr
    assert fixture.with_suffix(".mp3").is_file() and fixture.read_bytes() == good
    fixture.with_suffix(".mp3").unlink()
    # A real malformed MP3 passed through a wrapper that masks FFmpeg's exit
    # code must still fail validation and retain the valid FLAC source.
    damaged = base / "damaged.mp3"
    subprocess.run([ffmpeg, "-v", "error", "-i", str(fixture), "-ar", "44100",
                    "-ac", "1", "-c:a", "libmp3lame", "-b:a", "64k",
                    "-write_xing", "0", "-id3v2_version", "0", str(damaged)], check=True)
    data = bytearray(damaged.read_bytes())
    offset = 0
    for _ in range(10):
        assert data[offset:offset + 2] == b"\xff\xfb"
        offset += 144000 * 64 // 44100 + ((data[offset + 2] >> 1) & 1)
    # MPEG-1 mono, first granule big_values: 18-bit prefix + 12-bit length.
    for bit in range(9):
        position = 30 + bit
        at, mask = offset + 4 + position // 8, 1 << (7 - position % 8)
        data[at] = (data[at] & ~mask) | (mask if 289 & (1 << (8 - bit)) else 0)
    damaged.write_bytes(data)
    diagnostic = subprocess.run([ffmpeg, "-v", "error", "-xerror", "-i", str(damaged),
                                 "-f", "null", "-"], capture_output=True, timeout=20)
    assert b"big_values too big" in diagnostic.stderr, diagnostic.stderr
    wrapper.write_text("if ($args -contains '-y') { [IO.File]::Copy(" + quote(damaged) +
        ", $args[-1]); exit 0 }; & " + quote(ffmpeg) + " @args; exit 0", encoding="utf-8-sig")
    result = subprocess.run(command(fixture, wrapper, "-DeleteOriginal"), capture_output=True, timeout=20)
    assert result.returncode != 0, "Masked decoder error was accepted"
    assert fixture.read_bytes() == good and not fixture.with_suffix(".mp3").exists()
    assert not list(base.glob("*.soggfy-converting-*"))
    # A single budget covers both encode and validation; it must not reset.
    wrapper.write_text("Start-Sleep -Seconds 2; & " + quote(ffmpeg) +
                       " @args; exit $LASTEXITCODE", encoding="utf-8-sig")
    result = subprocess.run(command(fixture, wrapper, "-TimeoutSeconds", "3", "-DeleteOriginal"),
                            capture_output=True, timeout=12)
    assert result.returncode != 0 and b"timed out" in result.stderr
    assert fixture.read_bytes() == good and not fixture.with_suffix(".mp3").exists()
    assert not list(base.glob("*.soggfy-converting-*"))
    watch = base / "watch"
    watch.mkdir()
    pidfile = base / "child.pid"
    wrapper.write_text("if (($args -contains '-y') -and (($args -join ' ') -match '00-stuck')) { "
        "$env:SGF_TEST_MODE = 'hang'; $env:SGF_TEST_PID = " + quote(pidfile) + "; & " +
        quote(converter) + " @args; exit $LASTEXITCODE }; & " + quote(ffmpeg) +
        " @args; exit $LASTEXITCODE", encoding="utf-8-sig")
    stuck = watch / "00-stuck.flac"
    stuck.write_bytes(good)
    logfile = base / "watch.log"
    with logfile.open("wb") as log:
        process = subprocess.Popen(command(watch, wrapper, "-Watch", "-TimeoutSeconds", "3"), stdout=log, stderr=log)
        try:
            wait_for(pidfile.exists)
            child = int(pidfile.read_text())
            (watch / "healthy.flac").write_bytes(good)
            wait_for(lambda: (watch / "healthy.mp3").is_file())
            wait_for(lambda: dead(child))
            assert stuck.read_bytes() == good and not stuck.with_suffix(".mp3").exists()
            assert process.poll() is None
        finally:
            process.terminate()
            process.wait(timeout=10)
    # Force-stop PowerShell while encoding. Its job must kill the entire tree.
    pidfile.unlink(missing_ok=True)
    with logfile.open("wb") as log:
        process = subprocess.Popen(command(stuck, wrapper, "-TimeoutSeconds", "30"), stdout=log, stderr=log)
        try:
            wait_for(pidfile.exists)
            child = int(pidfile.read_text())
            process.terminate()
            process.wait(timeout=10)
            wait_for(lambda: dead(child), 5)
            assert stuck.read_bytes() == good
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
print("PASS: real damaged MP3 rejected despite masked exit; Unicode/metacharacter wrappers, shared deadline, watcher recovery, parent-exit cleanup")
