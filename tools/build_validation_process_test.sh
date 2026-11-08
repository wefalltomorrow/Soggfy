#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
destination="${1:?output directory required}"
vorbis="${2:?Vorbis object directory required}"
flac="${3:?FLAC object directory required}"
CXX="${CXX:-g++}"
mkdir -p "$destination"
objects=("$vorbis"/vorbis-*.o "$vorbis"/ogg-*.o)
for unit in bitmath bitreader cpu crc fixed float format lpc md5 memory stream_decoder win_utf8_io; do
    objects+=("$flac/$unit.o")
done
flags=(-std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -static -municode)
"$CXX" "${flags[@]}" -DFLAC__NO_DLL -I native/vendor/libogg/include \
    -I native/vendor/libvorbis/include -I native/vendor/libFLAC/include \
    native/validation_worker.cpp native/vorbis_validation.cpp native/flac_validation.cpp \
    native/flac_history_core.cpp native/compressed_buffer.cpp native/rich_metadata.cpp \
    native/ogg_tags.cpp native/ogg_history_core.cpp "${objects[@]}" -o "$destination/validation-worker.exe"
python3 tools/generate_validation_worker_digest.py "$destination/validation-worker.exe" "$destination/validation_worker_digest.h"
"$CXX" "${flags[@]}" -I "$destination" tests/windows_validation_process_test.cpp native/validation_process.cpp native/validation_worker_trust.cpp native/validation_worker_build.cpp \
    -lbcrypt -o "$destination/validation-process-test.exe"
