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

"$PYTHON" - <<'PY'
from pathlib import Path
import re

root = Path(".")
version = (root / "VERSION").read_text(encoding="utf-8").strip()
match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(?:-rc\.(\d+))?", version)
if not match:
    raise SystemExit(f"Unsupported VERSION format for Windows resource: {version!r}")

major, minor, patch = (int(match.group(i)) for i in (1, 2, 3))
prerelease = int(match.group(4) or 0)
flags = "0x2L" if "-" in version else "0x0L"

script = "window.__floggfyNative=true;\n" + (root / "native/metadata_collector.js").read_text(encoding="utf-8")
(root / "build/metadata_script.h").write_text(
    'static constexpr wchar_t metadata_script[]=LR"FLOGGFY(' + script + ')FLOGGFY";\n',
    encoding="utf-8",
)
ui_files = [
    "native/ui/classic_core.js",
    "native/ui/classic_settings.js",
    "native/ui/classic_status.js",
    "native/ui/classic_canvas.js",
    "native/ui/classic_m3u.js",
    "native/ui/classic_context.js",
    "native/ui/classic_boot.js",
]
ui_script = "\n".join((root / path).read_text(encoding="utf-8") for path in ui_files)
(root / "build/soggfy_ui_script.h").write_text(
    'static constexpr wchar_t soggfy_ui_script[]=LR"SOGGFYUI(' + ui_script + ')SOGGFYUI";\n',
    encoding="utf-8",
)

resource = f'''#include <winver.h>

VS_VERSION_INFO VERSIONINFO
 FILEVERSION {major},{minor},{patch},{prerelease}
 PRODUCTVERSION {major},{minor},{patch},{prerelease}
 FILEFLAGSMASK 0x3fL
 FILEFLAGS {flags}
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_DLL
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "Comments", "Modern x64 Soggfy continuation; Floggfy-derived capture core"
            VALUE "FileDescription", "Soggfy Spotify history capture proxy"
            VALUE "FileVersion", "{version}"
            VALUE "InternalName", "Soggfy"
            VALUE "LegalCopyright", "MIT / CC0; see bundled licenses"
            VALUE "OriginalFilename", "version.dll"
            VALUE "ProductName", "Soggfy"
            VALUE "ProductVersion", "{version}"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 1200
    END
END
'''
(root / "build/version.rc").write_text(resource, encoding="utf-8")
PY

"$WINDRES" -O coff build/version.rc -o build/version-resource.o

for source in buffer hook trampoline hde/hde64; do
    "$CC" -std=c11 -O2 -Wall -Wextra -c \
        "native/vendor/minhook/src/$source.c" -o "build/minhook-${source//\//-}.o"
done

"$CC" -std=c11 -O2 -I native/vendor/libogg/include \
    -c native/vendor/libogg/src/framing.c -o build/ogg-framing.o

"$CXX" -std=c++17 -O2 -Wall -Wextra -shared \
    -static -static-libgcc -static-libstdc++ \
    -Wl,--no-insert-timestamp,--dynamicbase,--nxcompat \
    native/version_proxy.cpp native/playback_speed.cpp native/playback_speed_discovery.cpp native/cef_request_filter.cpp native/cef_request_filter_core.cpp native/pe_imports.cpp native/spotify_hook_discovery.cpp native/audio_history.cpp native/playback_quality.cpp native/playback_quality_windows.cpp native/ogg_history_core.cpp \
    native/media_session.cpp native/classic_path_match.cpp native/classic_ui_backend.cpp native/post_process.cpp native/ogg_tags.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/history_settings.cpp native/library_layout.cpp \
    native/existing_quality.cpp native/file_publication.cpp native/to_disk_menu.cpp native/rich_metadata.cpp native/cached_metadata.cpp native/cached_metadata_windows.cpp native/metadata_bridge.cpp native/async_log.cpp native/version_exports.S native/version.def \
    build/version-resource.o build/minhook-*.o build/ogg-framing.o -I native/vendor/libogg/include \
    -o build/version.dll -lole32 -luuid -liphlpapi -lws2_32 -lruntimeobject -lshell32 -lwinhttp

SOURCE_DATE_EPOCH=1 "$STRIP" --strip-unneeded build/version.dll

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
