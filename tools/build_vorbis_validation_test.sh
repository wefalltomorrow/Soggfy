#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
destination="${1:?output directory required}"
mkdir -p "$destination"
CC="${CC:-gcc}"
CXX="${CXX:-g++}"
flags=(-O2)
if [[ "${SGF_SANITIZE:-0}" == 1 ]]; then
    flags=(-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all)
fi
target="$("$CC" -dumpmachine)"
if [[ "$target" != *mingw* && "$target" != *windows* ]]; then
    flags+=(-DHAVE_ALLOCA_H)
fi
includes=(-I native/vendor/libogg/include -I native/vendor/libvorbis/include)
objects=()
for unit in mdct smallft block envelope window lsp lpc analysis synthesis psy info floor1 floor0 res0 mapping0 registry codebook sharedbook lookup bitrate; do
    object="$destination/vorbis-$unit.o"
    "$CC" -std=c11 "${flags[@]}" "${includes[@]}" -c "native/vendor/libvorbis/lib/$unit.c" -o "$object"
    objects+=("$object")
done
for unit in framing bitwise; do
    object="$destination/ogg-$unit.o"
    "$CC" -std=c11 "${flags[@]}" "${includes[@]}" -c "native/vendor/libogg/src/$unit.c" -o "$object"
    objects+=("$object")
done
if [[ "${SGF_CODEC_OBJECTS_ONLY:-0}" == 1 ]]; then exit 0; fi
"$CC" -std=c11 -UNDEBUG "${flags[@]}" "${includes[@]}" \
    tests/ogg_bitwise_test.c "$destination/ogg-bitwise.o" -o "$destination/ogg-bitwise-test.exe"
"$destination/ogg-bitwise-test.exe"
"$CXX" -std=c++17 -UNDEBUG -Wall -Wextra -Werror "${flags[@]}" "${includes[@]}" \
    tests/vorbis_validation_test.cpp native/vorbis_validation.cpp native/ogg_history_core.cpp \
    "${objects[@]}" -o "$destination/vorbis-validation-test.exe"
