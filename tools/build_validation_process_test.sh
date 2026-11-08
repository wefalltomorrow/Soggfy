#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
destination="${1:?output directory required}"
vorbis="${2:?Vorbis object directory required}"
flac="${3:?FLAC object directory required}"
CXX="${CXX:-g++}"
CXX="$CXX" bash tools/build_validation_worker.sh "$destination" "$vorbis" "$flac"
flags=(-std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -static -municode)
"$CXX" "${flags[@]}" -I "$destination" tests/windows_validation_process_test.cpp native/validation_process.cpp native/validation_worker_trust.cpp native/validation_worker_build.cpp \
    -lbcrypt -o "$destination/validation-process-test.exe"
"$CXX" "${flags[@]}" -I "$destination" tests/windows_artwork_process_test.cpp native/validation_process.cpp native/validation_worker_trust.cpp native/validation_worker_build.cpp -lbcrypt -o "$destination/artwork-process-test.exe"
python3 tests/artwork_process_decode_test.py "$destination/artwork-process-test.exe" "$destination/validation-worker.exe"
