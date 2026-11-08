#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
CC="${CC:-x86_64-w64-mingw32-gcc}"
# Accept WSL and MSYS2, with isolated fixtures.
windows_path() { if command -v cygpath >/dev/null; then cygpath -w "$1"; else wslpath -w "$1"; fi; }
if [[ $# -gt 0 ]]; then
    floggfy_test_root="$1"
else
    floggfy_test_root="$(mktemp -d)"
fi
mkdir -p "$floggfy_test_root"
python3 tests/create_quality_fixtures.py "$floggfy_test_root"
python3 tests/create_cache_fixtures.py "$floggfy_test_root/cache-fixtures"
mkdir -p "$floggfy_test_root/cache-users/synthetic-user/primary.ldb"
cp "$floggfy_test_root/cache-fixtures/separate-album.ldb" "$floggfy_test_root/cache-users/synthetic-user/primary.ldb/000001.ldb"
mkdir -p build/windows-tests
python3 tools/generate_native_resources.py
for source in buffer hook trampoline hde/hde64; do
    "$CC" -std=c11 -O2 -Wall -Wextra -c \
        "native/vendor/minhook/src/$source.c" -o "build/windows-tests/minhook-${source//\//-}.o"
done
common=(-std=c++17 -O2 -UNDEBUG -Wall -Wextra -Werror -static)
mkdir -p "$floggfy_test_root/network-patch"
"$CXX" "${common[@]}" tests/windows_network_patch_test.cpp native/pe_imports.cpp -lole32 -luuid -lshell32 -liphlpapi -lws2_32 -o "$floggfy_test_root/network-patch/network-patch-test.exe"
"$floggfy_test_root/network-patch/network-patch-test.exe"
"$CXX" "${common[@]}" -municode tests/windows_validation_worker_trust_test.cpp native/validation_worker_trust.cpp -lbcrypt -o "$floggfy_test_root/validation-worker-trust-test.exe"
worker_test_hash="$(sha256sum "$floggfy_test_root/validation-worker-trust-test.exe" | cut -d' ' -f1)"
"$floggfy_test_root/validation-worker-trust-test.exe" "$worker_test_hash"
"$CXX" "${common[@]}" tests/mp3_layout_test.cpp native/compressed_buffer.cpp -o "$floggfy_test_root/mp3-layout-test.exe"
"$floggfy_test_root/mp3-layout-test.exe"
python3 tests/mp3_layout_decode_test.py "$floggfy_test_root/mp3-layout-test.exe"
"$CXX" "${common[@]}" tests/windows_metadata_transaction_test.cpp native/metadata_refresh_transaction.cpp native/async_log.cpp -lole32 -luuid -lshell32 -o "$floggfy_test_root/metadata-transaction-test.exe"
"$floggfy_test_root/metadata-transaction-test.exe"
fp_runtime="$("$CXX" -print-file-name=CRT_fp10.o)"
test -f "$fp_runtime"
"$CXX" "${common[@]}" tests/vorbis_comments_test.cpp -o "$floggfy_test_root/vorbis-comments-test.exe"
"$floggfy_test_root/vorbis-comments-test.exe"
"$CXX" "${common[@]}" tests/metadata_refresh_plan_test.cpp native/rich_metadata.cpp -o "$floggfy_test_root/metadata-refresh-plan-test.exe"
"$floggfy_test_root/metadata-refresh-plan-test.exe"
"$CXX" "${common[@]}" tests/listen_metadata_test.cpp native/ogg_history_core.cpp -o "$floggfy_test_root/listen-metadata-test.exe"
"$floggfy_test_root/listen-metadata-test.exe"
"$CXX" "${common[@]}" tests/windows_decoder_hook_test.cpp tests/decoder_context_probe.cpp native/playback_speed_compat.cpp build/windows-tests/minhook-*.o -o "$floggfy_test_root/windows-decoder-hook-test.exe"
"$floggfy_test_root/windows-decoder-hook-test.exe"
"$CXX" "${common[@]}" tests/windows_history_test.cpp \
 native/existing_quality.cpp native/file_publication.cpp native/history_settings.cpp \
 native/async_log.cpp native/library_layout.cpp native/ogg_history_core.cpp \
 -lole32 -luuid -lshell32 -o "$floggfy_test_root/windows-history-test.exe"
"$CXX" "${common[@]}" tests/windows_log_lane_test.cpp -o "$floggfy_test_root/windows-log-lane-test.exe"
"$floggfy_test_root/windows-log-lane-test.exe"
"$CXX" "${common[@]}" tests/windows_log_test.cpp native/async_log.cpp \
 native/history_settings.cpp -lole32 -luuid -lshell32 -o "$floggfy_test_root/windows-log-test.exe"
"$CC" -std=c11 -O2 -I native/vendor/libogg/include -c native/vendor/libogg/src/framing.c -o build/windows-tests/metadata-ogg.o
"$CXX" "${common[@]}" -I native/vendor/libogg/include tests/metadata_refresh_container_test.cpp native/rich_metadata.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/ogg_tags.cpp native/ogg_history_core.cpp build/windows-tests/metadata-ogg.o -o "$floggfy_test_root/metadata-refresh-container-test.exe"
"$floggfy_test_root/metadata-refresh-container-test.exe"
CC="$CC" CXX="$CXX" bash tools/build_vorbis_validation_test.sh "$floggfy_test_root/vorbis-validation"
CC="$CC" CXX="$CXX" python3 tools/build_flac_validation_test.py build/windows-tests/flac-validation
CXX="$CXX" bash tools/build_validation_process_test.sh build/windows-tests/validation-process "$floggfy_test_root/vorbis-validation" build/windows-tests/flac-validation
"$CXX" "${common[@]}" -municode -I build/windows-tests/validation-process -I native/vendor/libogg/include tests/windows_metadata_preparation_test.cpp native/validation_worker_trust.cpp native/validation_worker_build.cpp -lbcrypt native/metadata_refresh_preparation.cpp native/metadata_refresh_transaction.cpp native/validation_process.cpp native/rich_metadata.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/ogg_tags.cpp native/ogg_history_core.cpp build/windows-tests/metadata-ogg.o -o "$floggfy_test_root/metadata-preparation-test.exe"
python3 tests/metadata_refresh_decode_test.py "$floggfy_test_root/metadata-refresh-container-test.exe" ffmpeg "$floggfy_test_root/vorbis-validation/vorbis-validation-test.exe" build/windows-tests/flac-validation/flac-validation-test.exe build/windows-tests/validation-process/validation-process-test.exe build/windows-tests/validation-process/validation-worker.exe "$floggfy_test_root/metadata-preparation-test.exe"
"$CXX" "${common[@]}" tests/windows_metadata_identity_test.cpp -I native/vendor/libogg/include \
 native/rich_metadata.cpp native/cached_metadata.cpp native/cached_metadata_windows.cpp native/history_settings.cpp native/async_log.cpp \
 native/classic_ui_backend.cpp native/classic_status_backend.cpp native/classic_path_match.cpp native/library_layout.cpp native/existing_quality.cpp native/file_publication.cpp \
 native/media_session.cpp native/playback_quality_windows.cpp native/playback_quality.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/ogg_tags.cpp native/ogg_history_core.cpp build/windows-tests/metadata-ogg.o build/windows-tests/minhook-*.o -lole32 -luuid -lshell32 \
 "$fp_runtime" -lruntimeobject -lwinhttp -o "$floggfy_test_root/windows-metadata-identity-test.exe"
"$floggfy_test_root/windows-history-test.exe"
"$CXX" "${common[@]}" tests/windows_settings_flush_test.cpp native/async_log.cpp  -lole32 -luuid -lshell32 -o "$floggfy_test_root/windows-settings-flush-test.exe"
"$floggfy_test_root/windows-settings-flush-test.exe"
"$floggfy_test_root/windows-log-test.exe"
"$floggfy_test_root/windows-metadata-identity-test.exe"

"$CXX" "${common[@]}" tests/cached_metadata_test.cpp native/cached_metadata.cpp native/rich_metadata.cpp -o "$floggfy_test_root/cached-metadata-test.exe"
"$floggfy_test_root/cached-metadata-test.exe" "$(windows_path "$floggfy_test_root/cache-fixtures")"
"$CXX" "${common[@]}" tests/windows_cached_metadata_test.cpp native/cached_metadata.cpp native/cached_metadata_windows.cpp native/rich_metadata.cpp -lole32 -luuid -lshell32 -o "$floggfy_test_root/windows-cached-metadata-test.exe"
"$floggfy_test_root/windows-cached-metadata-test.exe" "$(windows_path "$floggfy_test_root/cache-users")"

"$CC" -std=c11 -O2 -I native/vendor/libogg/include -c native/vendor/libogg/src/framing.c -o build/windows-tests/ogg-framing.o
quality_sources=(native/playback_quality.cpp native/flac_history_core.cpp native/compressed_buffer.cpp native/ogg_tags.cpp native/ogg_history_core.cpp build/windows-tests/ogg-framing.o)
"$CXX" "${common[@]}" -I native/vendor/libogg/include tests/playback_quality_test.cpp "${quality_sources[@]}" -o "$floggfy_test_root/playback-quality-test.exe"
"$floggfy_test_root/playback-quality-test.exe"
"$CXX" "${common[@]}" -I native/vendor/libogg/include tests/windows_menu_quality_test.cpp "${quality_sources[@]}" native/history_settings.cpp native/async_log.cpp build/windows-tests/minhook-*.o -lole32 -luuid -lshell32 -o "$floggfy_test_root/windows-menu-quality-test.exe"
"$floggfy_test_root/windows-menu-quality-test.exe"

"$CXX" "${common[@]}" -I native/vendor/libogg/include tests/windows_quality_observer_test.cpp \
 native/playback_quality_windows.cpp "${quality_sources[@]}" native/media_session.cpp native/library_layout.cpp \
 native/existing_quality.cpp native/file_publication.cpp native/spotify_hook_discovery.cpp native/rich_metadata.cpp \
 native/async_log.cpp native/classic_ui_backend.cpp native/classic_status_backend.cpp native/classic_path_match.cpp \
 native/post_process.cpp build/windows-tests/minhook-*.o "$fp_runtime" -lole32 -luuid -lshell32 -lruntimeobject -lwinhttp \
 -o "$floggfy_test_root/windows-quality-observer-test.exe"
"$floggfy_test_root/windows-quality-observer-test.exe"
