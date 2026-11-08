# Xiph libvorbis reference codec

Source: https://github.com/xiph/vorbis
Pinned commit: c2aa86b05e981c96bf381fc6aa11cdd03eccc2fb (2025-12-05).

The core libvorbis sources, required internal headers and public codec header
are copied unchanged. SHA256.json records every copied file. COPYING contains
the upstream BSD license. No libvorbisenc tuning tables or libvorbisfile API
are included. Core files include shared encoder code required by the upstream
registry; Soggfy's validator uses synthesis APIs only.

The new validation path is under development and is not yet linked into the
shipping DLL. It must receive resource isolation and complete regression/CI
coverage before it can authorize saved-file changes.
