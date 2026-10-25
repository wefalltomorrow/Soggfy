#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
if [[ -n "${SGF_PWSH_PATH:-}" ]]; then
    maintenance_pwsh="$(cygpath -u "$SGF_PWSH_PATH")"
else
    maintenance_pwsh="$(command -v pwsh || true)"
fi
if [[ ! -x "$maintenance_pwsh" ]]; then
    echo "Windows maintenance tests require PowerShell 7" >&2
    exit 1
fi
mkdir -p build
if [[ -z "${SGF_REAL_FFMPEG:-}" ]] && command -v ffmpeg >/dev/null; then
    export SGF_REAL_FFMPEG="$(cygpath -w "$(command -v ffmpeg)")"
fi
if [[ "${CI:-}" == "true" && -z "${SGF_REAL_FFMPEG:-}" ]]; then
    echo "CI requires real FFmpeg codec validation" >&2
    exit 1
fi
"$CXX" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -static \
    tests/windows_post_process_test.cpp native/post_process.cpp native/existing_quality.cpp \
    native/ogg_history_core.cpp native/library_layout.cpp \
    -lshell32 -lole32 -luuid -o build/post-process-test.exe
build/post-process-test.exe
"$maintenance_pwsh" -NoProfile -File tests/manual_post_process_test.ps1 -ConverterPath build/post-process-test.exe
if [[ -n "${SGF_PYTHON_PATH:-}" ]]; then
    "$(cygpath -u "$SGF_PYTHON_PATH")" tests/manual_watch_test.py
else
    python tests/manual_watch_test.py
fi
"$CXX" -std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -static \
    tests/windows_classic_status_test.cpp native/classic_status_backend.cpp \
    native/classic_path_match.cpp native/library_layout.cpp -o build/classic-status-test.exe
build/classic-status-test.exe
