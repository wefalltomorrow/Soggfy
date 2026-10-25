#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
mkdir -p build
fp_runtime="$("$CXX" -print-file-name=CRT_fp10.o)"
test -f "$fp_runtime"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -static \
    tests/windows_media_observer_test.cpp native/media_session.cpp \
    "$fp_runtime" -lole32 -luuid -lruntimeobject -o build/media-observer-test.exe
# Run on Windows (including MSYS2). This is not the live Spotify test suite.
build/media-observer-test.exe
