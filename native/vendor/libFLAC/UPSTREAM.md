Reference libFLAC 1.5.0, pinned Xiph commit
1507800de4b70e21be71f38caa0d9079d0bc6e45.
Source: https://github.com/xiph/flac/tree/1507800de4b70e21be71f38caa0d9079d0bc6e45

The decoder and its C support units, required headers, Windows UTF-8 support
and COPYING.Xiph are copied unchanged. SHA256.json records the original bytes.
The test build disables assembly/intrinsics, Ogg-FLAC and DLL exports. Encoder
implementation and command-line applications are not included. Shared headers
and support units may declare or implement functions unused by the decoder.

The codec C units are compiled with -fwrapv in every build: corrupted predictor
residuals can otherwise overflow signed arithmetic before frame CRC validation
(observed at lpc.c:1193). This defines wrapping arithmetic rather than disabling
sanitizer instrumentation. The wrapper rejects all codec errors, bad CRCs and
sample-range/coverage mismatches; wrapping never authorizes acceptance. Our own
C++ sources do not receive -fwrapv. Full ASan/UBSan checks remain enabled in CI.

This dependency is linked only into validation tests. Before production use,
run it in a supervised process with time/memory/cancellation bounds and stable
input identity. Input-size limits alone do not bound codec resources.
