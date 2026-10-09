# Soggfy v3.0.0-rc.45

RC45 hardens the Soggfy installer around SpotX failures.

The RC44 installer trusted SpotX's process exit code. Upstream SpotX uses bare Exit in several error paths, so a fatal error such as a broken xpui.spa archive can still return exit code 0. That allowed Soggfy to print Done and install version.dll even though SpotX had stopped.

RC45 now validates Spotify's Apps/xpui.spa before SpotX runs, restores a valid Apps/xpui.bak automatically if needed, backs up the Spotify files SpotX may modify, and verifies the resulting xpui archive and SpotX patch marker afterwards. If SpotX fails, Soggfy restores the pre-SpotX files and stops instead of continuing.

For the normal %APPDATA%/Spotify install, RC45 also stops passing -SpotifyPath to SpotX so upstream SpotX can use its normal repair/update path. SpotX is invoked with podcasts_on to avoid the unrelated homepage podcast-removal modification.

The Soggfy runtime itself is otherwise the RC44 build: direct Spotify loader-readiness gating, RC42 startup speed safety, RC41 natural-transition speed recovery, and RC40 Classic UI startup fallback remain unchanged.
