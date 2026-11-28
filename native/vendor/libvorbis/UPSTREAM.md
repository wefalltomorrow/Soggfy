# Xiph libvorbis reference codec

Source: https://github.com/xiph/vorbis
Pinned commit: c2aa86b05e981c96bf381fc6aa11cdd03eccc2fb (2025-12-05).

The core libvorbis sources, required internal headers and public codec header
are copied unchanged. SHA256.json records every copied file. COPYING contains
the upstream BSD license. No libvorbisenc tuning tables or libvorbisfile API
are included. Core files include shared encoder code required by the upstream
registry; Soggfy's validator uses synthesis APIs only.

RC82 links the codec into validation tests and the separately packaged helper,
never the Spotify-loaded DLL. The isolated validation path has resource limits
and regression/CI coverage; production saved-file refresh remains disabled
pending filesystem preservation and coordinator work. COPYING ships with it.
