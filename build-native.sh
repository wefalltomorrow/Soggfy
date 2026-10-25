#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"

CC="${CC:-x86_64-w64-mingw32-gcc}"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
STRIP="${STRIP:-x86_64-w64-mingw32-strip}"
LD="${LD:-x86_64-w64-mingw32-ld}"
PYTHON="${PYTHON:-python3}"

mkdir -p build

"$PYTHON" tools/generate_native_resources.py

"$WINDRES" -O coff build/version.rc -o build/version-resource.o

for source in buffer hook trampoline hde/hde64; do
    "$CC" -std=c11 -O2 -Wall -Wextra -c \
        "native/vendor/minhook/src/$source.c" -o "build/minhook-${source//\//-}.o"
done

"$CC" -std=c11 -O2 -I native/vendor/libogg/include \
    -c native/vendor/libogg/src/framing.c -o build/ogg-framing.o

# Recent MinGW fesetenv references __mingw_fe_dfl_env, supplied by CRT_fp10.
# DLL startup does not pull that object from libmingw32 before libmingwex is
# scanned. Link the toolchain's own default environment, not a local ABI copy.
fp_runtime="$("$CXX" -print-file-name=CRT_fp10.o)"
if [[ ! -f "$fp_runtime" ]]; then
    echo "Missing MinGW floating-point runtime object: $fp_runtime" >&2
    exit 1
fi

"$CXX" -std=c++17 -O2 -Wall -Wextra -shared \
    -static -static-libgcc -static-libstdc++ \
    -Wl,--no-insert-timestamp,--dynamicbase,--nxcompat \
    native/version_proxy.cpp native/playback_speed.cpp native/playback_speed_compat.cpp native/playback_speed_discovery.cpp native/cef_request_filter.cpp native/cef_request_filter_core.cpp native/pe_imports.cpp native/spotify_hook_discovery.cpp native/audio_history.cpp native/playback_quality.cpp native/playback_quality_windows.cpp native/ogg_history_core.cpp \
    native/media_session.cpp native/classic_path_match.cpp native/classic_ui_backend.cpp native/classic_status_backend.cpp native/post_process.cpp native/ogg_tags.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/history_settings.cpp native/library_layout.cpp \
    native/existing_quality.cpp native/file_publication.cpp native/to_disk_menu.cpp native/rich_metadata.cpp native/cached_metadata.cpp native/cached_metadata_windows.cpp native/metadata_bridge.cpp native/async_log.cpp native/version_exports.S native/version.def \
    build/version-resource.o build/minhook-*.o build/ogg-framing.o -I native/vendor/libogg/include \
    "$fp_runtime" -o build/version.dll -lole32 -luuid -liphlpapi -lws2_32 -lruntimeobject -lshell32 -lwinhttp

SOURCE_DATE_EPOCH=1 "$STRIP" --strip-unneeded build/version.dll

"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -municode -mwindows \
    -static -static-libgcc -static-libstdc++ -Wl,--no-insert-timestamp,--dynamicbase,--nxcompat \
    native/soggfy_launcher.cpp native/startup_launcher.cpp -o build/Soggfy.exe
SOURCE_DATE_EPOCH=1 "$STRIP" --strip-unneeded build/Soggfy.exe

version="$(tr -d '\r\n' < VERSION)"
{
    echo "Soggfy version: $version"
    echo "Target: $("$CC" -dumpmachine)"
    echo "C compiler: $("$CC" --version | head -n 1)"
    echo "C++ compiler: $("$CXX" --version | head -n 1)"
    echo "Linker: $("$LD" --version | head -n 1)"
    echo "Resource compiler: $("$WINDRES" --version | head -n 1)"
    echo "Strip: $("$STRIP" --version | head -n 1)"
    echo "SOURCE_DATE_EPOCH: 1"
} > build/BUILDINFO.txt
