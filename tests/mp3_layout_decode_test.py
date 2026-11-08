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


def run(*args, expected_return=0, expected_stderr=None):
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
    # Some FFmpeg versions report a decode error but exit zero even with
    # -xerror. Those negative fixtures require the specific diagnostic AND
    # non-equivalent PCM at the call site, not a particular process status.
    if expected_return is None:
        assert expected_stderr, "unconstrained exits require an error diagnostic"
    matches = expected_return is None or result.returncode == expected_return
    if not matches:
        sys.stderr.write(result.stderr.decode("utf-8", errors="replace"))
        result.check_returncode()
        raise AssertionError(f"expected exit {expected_return}, got {result.returncode}")
    if expected_stderr is not None:
        assert expected_stderr in result.stderr, result.stderr
    return result.stdout


def protect_frames(source, packets, rate, channels):
    """Reframe reservoir-disabled CBR audio with CRC and additional stuffing.

    Compressed data is preserved. A larger bitrate provides room for the CRC;
    no main-data bytes are removed. FFmpeg independently checks CRC and PCM.
    """
    result = bytearray()
    for packet in packets:
        at, size = int(packet["pos"]), int(packet["size"])
        frame = source[at:at + size]
        assert frame[1] & 1, "expected unprotected encoder output"
        high = ((frame[1] >> 3) & 3) == 3
        side = (17 if channels == 1 else 32) if high else (9 if channels == 1 else 17)
        back = (frame[4] << 1) | (frame[5] >> 7) if high else frame[4]
        assert back == 0, "reframing requires disabled reservoir"
        index = (frame[2] >> 4) + 1
        rates = (0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320) if high else (0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160)
        assert index < len(rates)
        length = (144000 if high else 72000) * rates[index] // rate + ((frame[2] >> 1) & 1)
        assert length >= size + 2
        protected = bytearray(frame[:4])
        protected[1] &= 0xfe
        protected[2] = (index << 4) | (protected[2] & 15)
        protected.extend(b"\0\0" + frame[4:] + bytes(length - size - 2))
        message = protected[2:4] + protected[6:6 + side]
        # Integer polynomial division, independent of the C++ CRC bit loop.
        remainder = (0xffff << (len(message) * 8)) ^ (int.from_bytes(message, "big") << 16)
        while remainder.bit_length() > 16:
            remainder ^= 0x18005 << (remainder.bit_length() - 17)
        protected[4:6] = remainder.to_bytes(2, "big")
        result.extend(protected)
    return result


def set_bits(data, byte, bit, count, value):
    for index in range(count):
        position = bit + index
        mask = 1 << (7 - position % 8)
        at = byte + position // 8
        data[at] = (data[at] & ~mask) | (mask if value & (1 << (count - index - 1)) else 0)


test = str(pathlib.Path(sys.argv[1]).resolve())
ffmpeg = sys.argv[2] if len(sys.argv) > 2 else "ffmpeg"
ffprobe = sys.argv[3] if len(sys.argv) > 3 else "ffprobe"
with tempfile.TemporaryDirectory(prefix="soggfy-mp3-layout-") as directory:
    missing_predecessors = 0
    protected_cases = 0
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
                middle = int(packets[len(packets) // 2]["pos"])
                assert source[middle + 1] & 1
                high = ((source[middle + 1] >> 3) & 3) == 3
                first_record = (18 if channels == 1 else 20) if high else (9 if channels == 1 else 10)
                for kind, native_error, decoder_error in (
                    ("pairs", b"coefficient pair count", b"big_values too big"),
                    ("block", b"switched block type", b"invalid block type"),
                ):
                    damaged = bytearray(source)
                    if kind == "pairs":
                        set_bits(damaged, middle + 4, first_record + 12, 9, 289)
                    else:
                        set_bits(damaged, middle + 4, first_record + (33 if high else 38), 3, 4)
                    malformed = pathlib.Path(directory) / "side-info.mp3"
                    malformed.write_bytes(damaged)
                    run(test, malformed, expected_return=1, expected_stderr=native_error)
                    damaged_pcm = run(ffmpeg, "-v", "error", "-nostdin", "-xerror", "-err_detect", "explode",
                                      "-i", malformed, "-f", "s16le", "-", expected_return=None,
                                      expected_stderr=decoder_error)
                    assert damaged_pcm != pcm
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
                if not vbr:
                    plain = pathlib.Path(directory) / "no-reservoir.mp3"
                    run(ffmpeg, "-v", "error", "-nostdin", "-y", "-f", "lavfi", "-i",
                        f"anoisesrc=r={rate}:d=0.37:seed=12345", "-ac", str(channels),
                        "-c:a", "libmp3lame", "-b:a", "64k", "-reservoir", "0",
                        "-write_xing", "0", "-id3v2_version", "0", plain)
                    plain_packets = json.loads(run(ffprobe, "-v", "error", "-show_packets",
                                                   "-select_streams", "a:0", "-of", "json", plain))["packets"]
                    protected = protect_frames(plain.read_bytes(), plain_packets, rate, channels)
                    protected_path = pathlib.Path(directory) / "protected.mp3"
                    protected_path.write_bytes(protected)
                    result = list(map(int, run(test, protected_path).split()))
                    assert result[:3] == [rate, channels, len(plain_packets)]
                    assert result[4:] == [0, len(protected)]
                    def decode(path):
                        return run(ffmpeg, "-v", "error", "-nostdin", "-xerror",
                                   "-err_detect", "crccheck+explode", "-i", path,
                                   "-map", "0:a:0", "-f", "s16le", "-")
                    plain_pcm = decode(plain)
                    assert decode(protected_path) == plain_pcm
                    # Corrupt a middle frame so rejection cannot be attributed
                    # only to demuxer probing of the first frame.
                    protected_packets = json.loads(run(ffprobe, "-v", "error", "-show_packets",
                                                       "-select_streams", "a:0", "-of", "json", protected_path))["packets"]
                    middle = int(protected_packets[len(protected_packets) // 2]["pos"])
                    for byte in (middle + 3, middle + 4, middle + 6):
                        damaged = bytearray(protected)
                        damaged[byte] ^= 1
                        bad.write_bytes(damaged)
                        run(test, bad, expected_return=1)
                        damaged_pcm = run(ffmpeg, "-v", "error", "-nostdin", "-xerror", "-err_detect",
                                          "crccheck+explode", "-i", bad, "-f", "s16le", "-",
                                          expected_return=None, expected_stderr=b"CRC mismatch")
                        assert damaged_pcm != plain_pcm
                    protected_cases += 1
    assert missing_predecessors == 36, missing_predecessors
    assert protected_cases == 18, protected_cases
    print("PASS: 36 real MPEG-1/2/2.5 mono/stereo CBR/VBR layouts agree with FFprobe and decoded sample counts; missing reservoir prefixes rejected")
    print("PASS: 18 CRC-protected reframed streams preserve decoded PCM; header, checksum and side-info corruption rejected by native inspection and FFmpeg")
    print("PASS: 72 real-stream side-information mutations rejected by native inspection and FFmpeg with matching causes")
