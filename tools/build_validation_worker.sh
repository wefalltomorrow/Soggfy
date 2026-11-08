#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
destination="${1:?output directory required}"
vorbis="${2:?Vorbis object directory required}"
flac="${3:?FLAC object directory required}"
CXX="${CXX:-g++}"
STRIP="${STRIP:-strip}"
mkdir -p "$destination"
# An earlier successful binding must not survive a failed link or inspection.
rm -f -- "$destination/validation_worker_digest.h"
objects=("$vorbis"/vorbis-*.o "$vorbis"/ogg-*.o)
for unit in bitmath bitreader cpu crc fixed float format lpc md5 memory stream_decoder win_utf8_io; do
    objects+=("$flac/$unit.o")
done
"$CXX" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -static -municode \
    -DFLAC__NO_DLL -I native/vendor/libogg/include \
    -I native/vendor/libvorbis/include -I native/vendor/libFLAC/include \
    native/validation_worker.cpp native/vorbis_validation.cpp native/flac_validation.cpp \
    native/flac_history_core.cpp native/compressed_buffer.cpp native/rich_metadata.cpp \
    native/ogg_tags.cpp native/ogg_history_core.cpp "${objects[@]}" -o "$destination/validation-worker.exe"
"$STRIP" --strip-unneeded "$destination/validation-worker.exe"
python3 tools/verify_validation_worker_imports.py "$destination/validation-worker.exe"
# This must be the final binary mutation before generating the caller's digest.
python3 tools/generate_validation_worker_digest.py "$destination/validation-worker.exe" "$destination/validation_worker_digest.h"
