"""Decode real generated audio before/after the native metadata policy."""
import pathlib
import hashlib
import itertools
import os
import shutil
import subprocess
import sys
import tempfile


def run(*args, require_clean_stderr=False):
    # This suite also runs in MSYS/WSL with native Windows test executables.
    # Convert only filesystem arguments, never FFmpeg filters or switches.
    if os.name != "nt":
        executable = pathlib.Path(shutil.which(str(args[0])) or args[0])
        with executable.open("rb") as stream:
            windows = stream.read(2) == b"MZ"
        if windows:
            mapper = shutil.which("cygpath") or shutil.which("wslpath")
            if not mapper:
                raise RuntimeError("Windows executable requires cygpath or wslpath")
            args = tuple(subprocess.check_output([mapper, "-w", str(arg)], text=True).strip()
                         if isinstance(arg, pathlib.Path) else arg for arg in args)
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
    if result.returncode:
        sys.stderr.write(result.stderr.decode("utf-8", errors="replace"))
        result.check_returncode()
    # Native test executables also exercise negative cases, and debug libFLAC
    # logs normal rewinds. FFmpeg runs at error level and must stay silent.
    if require_clean_stderr:
        assert not result.stderr, result.stderr
    return result.stdout


test = str(pathlib.Path(sys.argv[1]).resolve())
ffmpeg = sys.argv[2] if len(sys.argv) > 2 else "ffmpeg"
validator = str(pathlib.Path(sys.argv[3]).resolve()) if len(sys.argv) > 3 else None
flac_validator = str(pathlib.Path(sys.argv[4]).resolve()) if len(sys.argv) > 4 else None
process_validator = str(pathlib.Path(sys.argv[5]).resolve()) if len(sys.argv) > 5 else None
worker = pathlib.Path(sys.argv[6]).resolve() if len(sys.argv) > 6 else None
preparer = str(pathlib.Path(sys.argv[7]).resolve()) if len(sys.argv) > 7 else None
try:
    run(sys.executable, "-c", "import sys; sys.stderr.write('fixture decoder error')", require_clean_stderr=True)
except AssertionError as failure:
    assert failure.args == (b"fixture decoder error",), failure.args
else:
    raise AssertionError("fixture runner accepted zero-exit error diagnostics")
print("PASS: fixture runner rejects zero-exit error diagnostics")
with tempfile.TemporaryDirectory(prefix="soggfy-metadata-") as directory:
    root = pathlib.Path(directory)
    if process_validator:
        assert worker is not None
        run(process_validator, "--accept-worker", worker)
        substitute = root / "substitute-worker.exe"
        changed = bytearray(worker.read_bytes())
        changed[-1] ^= 1
        substitute.write_bytes(changed)
        # Even a matching mutable sidecar cannot change compiled trust.
        (root / "SHA256SUMS.txt").write_text(
            hashlib.sha256(changed).hexdigest() + "  substitute-worker.exe\n")
        run(process_validator, "--reject-worker", substitute)
        run(process_validator, "--reject-worker", root / "missing-worker.exe")
    cases = itertools.product((("ogg", "libvorbis"), ("flac", "flac")),
        ((44100, 2, "0.01", "tone"), (44100, 2, "1", "tone"),
         (48000, 1, "3.25", "tone"), (44100, 2, "1", "silence"),
         (22050, 1, "0.75", "noise"), (96000, 2, "1.25", "noise")))
    for index, ((extension, codec), (rate, channels, duration, signal)) in enumerate(cases):
        source = root / (f"source-{index}." + extension)
        before = root / (f"before-{index}." + extension)
        after = root / (f"after-{index}." + extension)
        generator = f"sine=frequency=440:sample_rate={rate}:duration={duration}"
        if signal == "silence":
            generator = f"anullsrc=r={rate}:cl=stereo:d={duration}"
        elif signal == "noise":
            generator = f"anoisesrc=r={rate}:d={duration}:seed=12345"
        run(ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin",
            "-f", "lavfi", "-i", generator,
            "-ac", str(channels), "-c:a", codec, source, require_clean_stderr=True)
        run(test, source, before, after)
        for path in (before, after):
            picture = run(ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin",
                "-xerror", "-err_detect", "explode", "-i", path, "-map", "0:v:0",
                "-frames:v", "1", "-pix_fmt", "rgb24", "-f", "rawvideo", "-", require_clean_stderr=True)
            assert picture == b"\xff\0\0", "generated 1x1 red artwork did not survive refresh"
        if preparer:
            assert worker is not None
            run(preparer, before, after, worker)
        if validator and extension == "ogg":
            for path in (source, before, after):
                run(validator, path)
        if flac_validator and extension == "flac":
            for path in (source, before, after):
                run(flac_validator, path)
        if process_validator:
            assert worker is not None
            for path in (source, before, after):
                run(process_validator, path, worker)
        decoded = []
        for path in (source, before, after):
            decoded.append(run(ffmpeg, "-hide_banner", "-loglevel", "error",
                "-xerror", "-err_detect", "explode", "-abort_on", "empty_output",
                "-nostdin", "-i", path, "-map", "0:a:0",
                "-c:a", "pcm_s32le", "-f", "s32le", "-", require_clean_stderr=True))
        assert decoded[0] and decoded[0] == decoded[1] == decoded[2], extension
        frame_size = channels * 4
        assert len(decoded[0]) % frame_size == 0, extension
        print(f"PASS: {extension} {signal} {rate}Hz/{channels}ch/{duration}s PCM identical; {len(decoded[0]) // frame_size} samples; artwork pixels preserved")
