"""Compare MP3 structure with independently encoded and decoded fixture audio.

Decoding validates these fixtures, not the native structural inspector.
"""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
from fractions import Fraction


def run(*args, expected_return=0):
    if os.name != "nt":
        executable = pathlib.Path(shutil.which(str(args[0])) or args[0])
        with executable.open("rb") as stream:
            windows = stream.read(2) == b"MZ"
        if windows:
            mapper = shutil.which("cygpath") or shutil.which("wslpath")
            if not mapper:
                raise RuntimeError("Windows executable requires a path mapper")
            args = tuple(subprocess.check_output([mapper, "-w", str(arg)], text=True).strip()
                         if isinstance(arg, pathlib.Path) else arg for arg in args)
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
    if result.returncode != expected_return:
        sys.stderr.write(result.stderr.decode("utf-8", errors="replace"))
        result.check_returncode()
        raise AssertionError(f"expected exit {expected_return}, got {result.returncode}")
    return result.stdout


test = str(pathlib.Path(sys.argv[1]).resolve())
ffmpeg = sys.argv[2] if len(sys.argv) > 2 else "ffmpeg"
ffprobe = sys.argv[3] if len(sys.argv) > 3 else "ffprobe"
with tempfile.TemporaryDirectory(prefix="soggfy-mp3-layout-") as directory:
    missing_predecessors = 0
    for rate in (8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000):
        for channels in (1, 2):
            for vbr in (False, True):
                path = pathlib.Path(directory) / f"{rate}-{channels}-{vbr}.mp3"
                quality = ("-q:a", "4") if vbr else ("-b:a", "64k")
                # Omit the seek frame so every frame is a demuxed packet.
                # Seek headers and gapless timing require separate work.
                run(ffmpeg, "-v", "error", "-nostdin", "-f", "lavfi", "-i",
                    f"anoisesrc=r={rate}:d=0.37:seed=12345", "-ac", str(channels),
                    "-c:a", "libmp3lame", *quality, "-write_xing", "0",
                    "-id3v2_version", "3" if vbr else "4", path)
                actual_rate, actual_channels, frames, samples, offset, size = map(int, run(test, path).split())
                probe = json.loads(run(ffprobe, "-v", "error", "-show_streams", "-show_packets",
                                      "-select_streams", "a:0", "-of", "json", path))
                stream = probe["streams"][0]
                packets = probe["packets"]
                expected_samples = sum(int(p["duration"]) for p in packets) * Fraction(stream["time_base"]) * rate
                assert actual_rate == int(stream["sample_rate"]) == rate
                assert actual_channels == stream["channels"] == channels
                assert frames == len(packets) and samples == expected_samples
                assert offset == int(packets[0]["pos"])
                assert size == sum(int(p["size"]) for p in packets)
                assert offset + size == path.stat().st_size
                pcm = run(ffmpeg, "-v", "error", "-nostdin", "-xerror", "-err_detect", "explode",
                          "-i", path, "-map", "0:a:0", "-f", "s16le", "-")
                assert len(pcm) == samples * channels * 2
                source = path.read_bytes()
                corrupted = bytearray(source)
                # A complete first frame may not refer to absent preceding
                # main-data bytes. Flip only its side-info back-reference.
                side = offset + (4 if source[offset + 1] & 1 else 6)
                corrupted[side] |= 0x80
                bad = pathlib.Path(directory) / "missing-reservoir.mp3"
                bad.write_bytes(corrupted)
                run(test, bad, expected_return=1)
                for packet in packets[1:]:
                    position = int(packet["pos"])
                    side = position + (4 if source[position + 1] & 1 else 6)
                    high = ((source[position + 1] >> 3) & 3) == 3
                    back = ((source[side] << 1) | (source[side + 1] >> 7)) if high else source[side]
                    if back:
                        # An untouched suffix has complete frame boundaries
                        # but depends on reservoir bytes in the removed prefix.
                        bad.write_bytes(source[position:])
                        run(test, bad, expected_return=1)
                        missing_predecessors += 1
                        break
    assert missing_predecessors == 36, missing_predecessors
    print("PASS: 36 real MPEG-1/2/2.5 mono/stereo CBR/VBR layouts agree with FFprobe and decoded sample counts; missing reservoir prefixes rejected")
