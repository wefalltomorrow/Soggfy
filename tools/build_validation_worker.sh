#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
destination="${1:?output directory required}"
vorbis="${2:-$destination/vorbis}"
flac="${3:-$destination/flac}"
CXX="${CXX:-g++}"
STRIP="${STRIP:-strip}"
PYTHON="${PYTHON:-python3}"
mkdir -p "$destination"
# An earlier successful binding must not survive a failed link or inspection.
rm -f -- "$destination/validation_worker_digest.h"
if [[ $# == 1 ]]; then
    SGF_CODEC_OBJECTS_ONLY=1 bash tools/build_vorbis_validation_test.sh "$vorbis"
    SGF_CODEC_OBJECTS_ONLY=1 "$PYTHON" tools/build_flac_validation_test.py "$flac"
elif [[ $# != 3 ]]; then
    echo 'Supply output directory, optionally followed by both codec object directories' >&2
    exit 1
fi
objects=("$vorbis"/vorbis-*.o "$vorbis"/ogg-*.o)
for unit in bitmath bitreader cpu crc fixed float format lpc md5 memory stream_decoder win_utf8_io; do
    objects+=("$flac/$unit.o")
done
"$CXX" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -static -municode \
    -Wl,--no-insert-timestamp,--dynamicbase,--nxcompat \
    -DFLAC__NO_DLL -I native/vendor/libogg/include \
    -I native/vendor/libvorbis/include -I native/vendor/libFLAC/include \
    native/validation_worker.cpp native/vorbis_validation.cpp native/flac_validation.cpp \
    native/flac_history_core.cpp native/compressed_buffer.cpp native/rich_metadata.cpp \
    native/ogg_tags.cpp native/ogg_history_core.cpp "${objects[@]}" -o "$destination/validation-worker.exe"
SOURCE_DATE_EPOCH=1 "$STRIP" --strip-unneeded "$destination/validation-worker.exe"
"$PYTHON" tools/verify_validation_worker_imports.py "$destination/validation-worker.exe"
# This must be the final binary mutation before generating the caller's digest.
"$PYTHON" tools/generate_validation_worker_digest.py "$destination/validation-worker.exe" "$destination/validation_worker_digest.h"
