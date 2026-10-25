#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
mkdir -p build
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -static \
    tests/windows_post_process_test.cpp native/post_process.cpp native/existing_quality.cpp \
    native/ogg_history_core.cpp native/library_layout.cpp \
    -lshell32 -lole32 -luuid -o build/post-process-test.exe
build/post-process-test.exe
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -static \
    tests/windows_classic_status_test.cpp native/classic_status_backend.cpp \
    native/classic_path_match.cpp native/library_layout.cpp -o build/classic-status-test.exe
build/classic-status-test.exe
