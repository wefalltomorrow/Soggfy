"""Build the test-only pinned reference FLAC decoder with explicit semantics."""
import os
from pathlib import Path
import subprocess
import sys

os.chdir(Path(__file__).resolve().parent.parent)
destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
cc, cxx = os.environ.get("CC", "gcc"), os.environ.get("CXX", "g++")
flags = ["-O2"]
if os.environ.get("SGF_SANITIZE") == "1":
    flags = ["-g", "-O1", "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=all"]
target = subprocess.check_output([cc, "-dumpmachine"], text=True)
windows = "mingw" in target or "windows" in target
root = Path("native/vendor/libFLAC")
definitions = ["-DFLAC__NO_DLL", "-DFLAC__NO_ASM", "-DFLAC__HAS_OGG=0",
               "-DHAVE_STDINT_H", "-DHAVE_INTTYPES_H", "-DHAVE_FSEEKO",
               "-DHAVE_LROUND=1", '-DPACKAGE_VERSION="1.5.0"',
               "-D_POSIX_C_SOURCE=200809L"]
includes = ["-I" + str(root / "include"), "-I" + str(root / "src/libFLAC/include"),
            "-Inative/vendor/libogg/include"]
objects = []
units = "bitmath bitreader cpu crc fixed float format lpc md5 memory stream_decoder".split()
for unit in units:
    obj = destination / (unit + ".o")
    # Corrupted residuals can overflow predictor arithmetic before the CRC check.
    # Define wrapping in ordinary AND sanitizer codec builds; never suppress a
    # sanitizer. Our own C++ code retains ordinary signed-overflow diagnostics.
    subprocess.run([cc, "-std=c11", *flags, "-fwrapv", *definitions, *includes,
                    "-c", str(root / "src/libFLAC" / (unit + ".c")), "-o", str(obj)], check=True)
    objects.append(str(obj))
if windows:
    obj = destination / "win_utf8_io.o"
    subprocess.run([cc, "-std=c11", *flags, *definitions, *includes, "-c",
                    str(root / "src/share/win_utf8_io/win_utf8_io.c"), "-o", str(obj)], check=True)
    objects.append(str(obj))
if os.environ.get("SGF_CODEC_OBJECTS_ONLY") == "1":
    raise SystemExit(0)
obj = destination / "ogg-framing.o"
subprocess.run([cc, "-std=c11", *flags, *includes, "-c",
                "native/vendor/libogg/src/framing.c", "-o", str(obj)], check=True)
objects.append(str(obj))
subprocess.run([cxx, "-std=c++17", "-UNDEBUG", "-Wall", "-Wextra", "-Werror",
                *flags, *definitions, *includes, "tests/flac_validation_test.cpp",
                "native/flac_validation.cpp", "native/flac_history_core.cpp",
                "native/compressed_buffer.cpp", "native/rich_metadata.cpp",
                "native/ogg_tags.cpp", "native/ogg_history_core.cpp", *objects,
                "-o", str(destination / "flac-validation-test.exe")], check=True)
