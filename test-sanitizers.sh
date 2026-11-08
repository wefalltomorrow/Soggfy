#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p build/sanitizers
CC="${CC:-gcc}"
CXX="${CXX:-g++}"
flags=(-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all)
"$CC" "${flags[@]}" -I native/vendor/libogg/include -c native/vendor/libogg/src/framing.c -o build/sanitizers/ogg.o
sources=(native/ogg_history_core.cpp native/ogg_tags.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/rich_metadata.cpp native/spotify_hook_discovery.cpp)
"$CXX" -std=c++17 -UNDEBUG -Wall -Wextra -Werror "${flags[@]}" -pthread -I native/vendor/libogg/include \
  tests/capture_mutation_test.cpp "${sources[@]}" build/sanitizers/ogg.o -o build/sanitizers/mutations
export ASAN_OPTIONS=detect_leaks=1:allocator_may_return_null=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
build/sanitizers/mutations "${1:-20000}" "${2:-4043931417}"
# Exercise exact regression scenarios under the same instrumentation.
for test in log_queue history_core recording_identity vorbis_comments metadata_refresh_plan metadata_refresh_container listen_metadata decoder_context client_playback compressed_buffer mp3_layout spotify_hook_discovery flac_history ogg_tags; do
  extra=()
  case "$test" in
    metadata_refresh_plan) extra=(native/rich_metadata.cpp);;
    metadata_refresh_container) extra=(native/rich_metadata.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/ogg_tags.cpp native/ogg_history_core.cpp build/sanitizers/ogg.o);;
    flac_history|ogg_tags) extra=(native/flac_history_core.cpp native/compressed_buffer.cpp native/ogg_tags.cpp native/ogg_history_core.cpp build/sanitizers/ogg.o);;
    history_core|recording_identity|listen_metadata) extra=(native/ogg_history_core.cpp);;
    decoder_context) extra=(tests/decoder_context_probe.cpp);;
    client_playback) extra=(native/rich_metadata.cpp native/ogg_history_core.cpp);;
    spotify_hook_discovery) extra=(native/spotify_hook_discovery.cpp);;
    compressed_buffer|mp3_layout) extra=(native/compressed_buffer.cpp);;
  esac
  "$CXX" -std=c++17 -UNDEBUG "${flags[@]}" -pthread -I native/vendor/libogg/include "tests/${test}_test.cpp" "${extra[@]}" -o "build/sanitizers/$test"
  "build/sanitizers/$test"
done
CC="$CC" CXX="$CXX" SGF_SANITIZE=1 bash tools/build_vorbis_validation_test.sh build/sanitizers/vorbis-validation
CC="$CC" CXX="$CXX" SGF_SANITIZE=1 python3 tools/build_flac_validation_test.py build/sanitizers/flac-validation
python3 tests/metadata_refresh_decode_test.py build/sanitizers/metadata_refresh_container ffmpeg build/sanitizers/vorbis-validation/vorbis-validation-test.exe build/sanitizers/flac-validation/flac-validation-test.exe
