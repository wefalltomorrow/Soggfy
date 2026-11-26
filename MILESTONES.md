# Engineering milestones

## November 26 CI run 234, package verification and native-provider recheck

RC88 at exact source commit `684758e6f173f2b20527d3cee0082fc50ce7fbb4` passed
all four jobs in [CI run 234](https://github.com/wefalltomorrow/Soggfy/actions/runs/45736957010):
native regressions; both seeded 20,000-iteration ASan/UBSan campaigns; Windows
media-observer and maintenance transaction/watch tests; Windows publication,
settings, logging, metadata and quality tests; and the x64 release build/package
checks. Artifact `15079619167` is bound to that commit. The 3,319,001-byte outer
ZIP SHA-256 is `06365337f29d8229d43bd1116535b6816a44a72133a41d67ed0d7bb2b699c792`;
the 3,320,975-byte RC88 package SHA-256 is
`48ebdc94c6531603617572d3d0e7f845a8b19da650f63b75869b0d5f3915a60d`.
Independent verification passed both ZIP integrity checks, safe paths, all
seven manifest hashes, required contents, validation-helper digest binding,
AMD64 PE/import checks, and normalized source matching for all 27 packaged
source/text files. The verifier and report are `outputs/verify_run234.py` and
`outputs/Soggfy-run234-package-evidence.json`.

On November 26 the installed Spotify 1.3.1.234 DLL was measured at 42,852,224
bytes with SHA-256
`210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`, matching
the pinned artifact. This confirms file identity only. Spotify was not running.
The current REA session has no active binary target; Ghidra is unconfigured,
Hopper is unsupported on Windows, and no IDA adapter is configured. No client
code was executed. Provider selection, callback completion and synchronized
lifetime, decoder-to-playback association, ad/DJ/interstitial isolation and
transitions, and exact-instance current-track skip remain unverified.

Maintenance transactions, bounded watch scans, playlist-save acknowledgements,
diagnostics, sanitizer campaigns, and package verification are implemented and
CI-covered. The Classic M3U 10,000-track guard remains covered by its 200,000-item
single-page regression. Production native MP3 capture and saved-recording
metadata refresh remain disabled pending runtime ownership, provenance,
freshness and lifecycle evidence. Live installation/restart and Classic
save-dialog acceptance have not been performed. PR #64 merge and official
release remain separate gates.

## November 26 CI run 233 and independent package verification

RC88 at exact source commit `d393ff540e39d451713226e1ca87f820b4f64025` passed
all four jobs in [CI run 233](https://github.com/wefalltomorrow/Soggfy/actions/runs/45734307041):
native regressions; both 20,000-iteration seeded ASan/UBSan campaigns; the
Windows media-observer, maintenance transaction/watch, publication, settings,
logging, metadata and quality suites; and the x64 release build/package checks.
GitHub artifact `15079286443` is bound to that commit. Its 3,318,456-byte outer
ZIP has SHA-256
`0559ce70a2f0ba60fd63f055f53f07d6677cdf9370a3ba655899fad884790cb0`; the
3,320,425-byte RC88 package has SHA-256
`1cf6d99cfd55b611ca4efb56991cd185ba9bd0acec36bb13a0b736d5d383f750`.
Independent verification passed outer and inner ZIP integrity, safe paths, all
seven manifest hashes, required contents, helper digest binding, AMD64 PE/import
checks, and normalized source matching for all 27 packaged source/text files.
The helper SHA-256 remains
`d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The
reproducible verifier and report are `outputs/verify_run233.py` and
`outputs/Soggfy-run233-package-evidence.json`.

The Classic M3U exporter now checks the cumulative 10,000-track limit before
appending each API page. Its regression rejects a single 200,000-item page
without sending a save request. This preserves the existing Classic interface,
paths and playback-speed behavior.

The installed Spotify DLL was rechecked against the pinned 1.3.1.234 artifact
identity as detailed below. This proves which artifact is installed, not that
runtime providers, callback completion/lifetime, decoder association,
ad/DJ/interstitial transitions or exact-instance skip are compatible. No usable
native analysis provider was available and no Spotify process was running.
Production native MP3 capture and saved-recording metadata refresh remain
disabled. Live installation/restart and Classic save-dialog acceptance have
not been performed; PR #64 merge and official release remain separate gates.

## November 26 CI run 231 and independent package verification

RC88 at exact source commit `978b1d9195360e767b39097a7d65ae6ccacd12e8` passed
all four jobs in [CI run 231](https://github.com/wefalltomorrow/Soggfy/actions/runs/45729218270):
native regression tests; both 20,000-iteration seeded ASan/UBSan campaigns;
Windows media-observer, maintenance, publication, settings, logging, metadata
and quality tests; and x64 release build/package checks. GitHub artifact
`15076671809` is bound to that commit. Its 3,318,096-byte outer ZIP has SHA-256
`6ac306da54af3c1f368642250e40b0999c93ed95017fcd4e5750f6bf74630427`; the
3,320,059-byte RC88 package has SHA-256
`b379145d4060ed8d225814be9c5cc27c6f98ffa25231de3efdca07e262f09652`.
Independent verification passed ZIP integrity and safe paths, all seven manifest
hashes, required content, helper digest binding, AMD64 PE/import checks, and
normalized comparison of all 27 packaged source/text files to the exact commit.
The helper SHA-256 is
`d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The
reproducible verifier and report are `outputs/verify_run231.py` and
`outputs/Soggfy-run231-package-evidence.json`.

Run 231 is a documentation/evidence refresh of the same RC88 implementation
already covered by run 230. It adds no new Spotify runtime evidence. The pinned
Spotify 1.3.1.234 artifact identity was rechecked November 26 as recorded below,
but no Spotify code was executed. Runtime provider selection, callback completion
and synchronized lifetime, decoder-to-playback association, ad/DJ/interstitial
isolation and transitions, and exact-instance current-track skip remain
unverified. Production native MP3 capture and saved-recording metadata refresh
remain disabled. Live installation/restart and Classic save-dialog acceptance
have not been performed. PR #64 merge and official release remain separate gates.

## November 26 CI run 230, package verification and pinned-client recheck

RC88 at exact source commit `58048d1a1f4bae28e9944a51ef0cf48656500f2b` passed
all four jobs in [CI run 230](https://github.com/wefalltomorrow/Soggfy/actions/runs/45622191137):
native regression tests; both 20,000-iteration seeded ASan/UBSan campaigns;
Windows media-observer, maintenance, publication, settings, logging, metadata
and quality tests; and x64 release build/package checks. GitHub artifact
`15027613274` is bound to that commit. Its 3,316,951-byte outer ZIP has SHA-256
`4c866cc3c5b4571781f84332d07448979185235803de7ec3af13b23737c2ca58`; the
3,318,951-byte RC88 package has SHA-256
`fde62d69de099d4f0182079f99d802f9e36153ed9ac136a4ceeac93479ad1040`.
Independent verification passed both ZIP integrity checks and safe paths, all
seven manifest hashes, required contents, helper digest binding, AMD64 PE/import
constraints, and normalized source matching for all 27 packaged text/source
files. The helper SHA-256 is
`d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The
reproducible verifier and report are `outputs/verify_run230.py` and
`outputs/Soggfy-run230-package-evidence.json`.

On November 26 the installed Spotify executable reported version 1.3.1.234;
`Spotify.dll` remained 42,852,224 bytes with SHA-256
`210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`, matching
the pinned artifact. No Spotify process was running. The REA session had no
target open; Ghidra was not configured, Hopper is unsupported on this Windows
host, and IDA was not configured. No fresh native analysis or Spotify runtime
observation was possible. Static identity/dispatch probes therefore still do
not prove runtime provider selection, callback completion and synchronized
lifetime, decoder-to-playback association, ad/DJ/interstitial isolation and
transitions, or exact-instance current-track skip. Production native MP3
capture and saved-recording metadata refresh remain disabled.

The documented implementation milestones after RC74—maintenance transactions,
bounded watch scans, playlist-save acknowledgements, diagnostics, sanitizer
campaigns and package verification—are implemented and covered by CI. Floggfy
was rechecked: v1.4.1 remains the newest listed release and both `main` and the
tag resolve to `15e1e286ea1ac474be4aff30e6db1541ba790340`. Its remaining catalog,
network-control and decoder features are product or pinned-client compatibility
work, not safe unreviewed ports. Live installation/restart and Classic save-dialog
acceptance have not been performed. PR #64 merge and official release remain
separate gates.

## November 25 CI run 229 and independent package verification

RC88 at exact source commit `af3cf2a1a978195ad96511e3c4f05fe01be3d6e8` passed
all four jobs in [CI run 229](https://github.com/wefalltomorrow/Soggfy/actions/runs/45617009227):
native regression tests, both seeded 20,000-iteration ASan/UBSan campaigns,
Windows media-observer and maintenance tests, the Windows
publication/settings/logging/metadata/quality suite, and x64 release
build/package checks. GitHub artifact `15023503003` is bound to that commit.
Its 3,316,481-byte outer ZIP has SHA-256
`5bf513a8753024dc4f106d8f077d0baa4815eed3f7dab22c93f6b91fed35e4b2`; the
3,318,483-byte RC88 package has SHA-256
`e6b269d42879870268d471131cf052405c2ebaa2ca11ed82e2c23569c76bad64`.
Independent verification passed ZIP integrity and safe-path checks, all seven
manifest hashes, required content, helper digest binding, AMD64 PE/import
constraints, and normalized comparison of all 27 packaged source/text files to
the tested commit. The helper SHA-256 is
`d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The
reproducible verifier and report are `outputs/verify_run229.py` and
`outputs/Soggfy-run229-package-evidence.json`.

On November 25 the installed Spotify DLL was rechecked at 42,852,224 bytes
with SHA-256
`210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`, matching
the pinned 1.3.1.234 artifact; no Spotify process was running. Hash-pinned
static probes checked 1,299 identity-path instructions, 81 dispatch/context
instructions and 35 scheduler-boundary instructions. They strengthen static
provenance only: the configured native analysis provider is unavailable
(`GHIDRA_INSTALL_DIR` is not set), no Spotify code was executed, and runtime
provider selection, callback completion and synchronized lifetime,
decoder-to-playback association, ad/DJ/interstitial isolation and transitions,
and exact-instance current-track skip remain unverified. Native MP3 capture
and saved-recording metadata refresh remain disabled.

Maintenance transactions, bounded watch scans, native playlist-save
acknowledgements, diagnostics, sanitizer campaigns and package verification
are implemented and CI-covered. The current Floggfy v1.4.1 comparison remains
in `FLOGGFY_20261125_REVIEW.md`; its remaining product candidates require
separate design or pinned-client evidence before they can be ported safely.
Live installation/restart and Classic save-dialog acceptance have not been
performed. PR #64 merge and official release remain separate gates.

## November 25 CI run 228, independent package verification and pinned identity follow-up

RC88 at exact source commit `a168e02dc93b65995a095fdd0a45cf5e12d92ada` passed all four jobs in [CI run 228](https://github.com/wefalltomorrow/Soggfy/actions/runs/45611267118): native regression tests, both seeded 20,000-iteration ASan/UBSan campaigns, Windows media-observer and maintenance tests, the Windows publication/settings/logging/metadata/quality suite, and x64 release build/package checks. GitHub artifact `15022175087` is bound to that commit. Its 3,316,214-byte outer ZIP has SHA-256 `a9a324edbc184258d4413be5b71288f971c2364fa2991b1490e5edd26c2d5a62`; the 3,318,220-byte RC88 package has SHA-256 `79f84dfabecefcdce0e2e64c596486711ccb90ceb0b6aca353a6757ce74b9c64`. Independent verification passed ZIP integrity and safe-path checks, all seven manifest hashes, required content, helper digest binding, AMD64 PE/import constraints, and normalized comparison of all 27 packaged source/text files to the tested commit. The helper SHA-256 is `d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The reproducible verifier and report are `outputs/verify_run228.py` and `outputs/Soggfy-run228-package-evidence.json`.

On November 25 the installed Spotify DLL was rechecked at 42,852,224 bytes with SHA-256 `210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`, matching the pinned 1.3.1.234 artifact. Follow-up hash-pinned static probes checked 1,299 parent-path instructions, 81 dispatch/context instructions and 35 scheduler-boundary instructions. They strengthen static provenance only: no Spotify code was executed, and no usable Ghidra or IDA provider was configured. Runtime provider selection, callback completion and synchronized lifetime, decoder-to-playback association, ad/DJ/interstitial isolation and transitions, and exact-instance current-track skip remain unverified. Native MP3 capture and saved-recording metadata refresh remain disabled.

The v1.4.1 Floggfy source/release comparison and port decisions are recorded in `FLOGGFY_20261125_REVIEW.md`. Maintenance transactions, manual watch scanning, playlist-save acknowledgements, diagnostics, sanitizer campaigns and package verification are covered by the Windows/native CI suites. Live installation/restart and Classic save-dialog acceptance have not been performed. PR #64 merge and official release remain separate gates.

## November 25 CI run 227, package verification and pinned-client identity

PR #64 head commit `077e378fb8e06775058ebb2e73ae58491a538145` passed all four
jobs in [CI run 227](https://github.com/wefalltomorrow/Soggfy/actions/runs/45607711580):
native regressions, both seeded ASan/UBSan campaigns, Windows media-observer
and maintenance tests, the Windows publication/settings/logging/metadata/quality
suite, and x64 release build/package checks. GitHub artifact `15019757527` is
bound to that exact commit. Its 3,315,844-byte outer ZIP has SHA-256
`8e20fd89ef6581e3e7401eba5a6a2d91fc7b39d6eb02ecb9bcc2107deb2f47b5`; the
3,317,864-byte RC88 package has SHA-256
`49c5db2cf563bf9ba4970187dfab5eede08c9a7d90b4e314f4ef2286d8f4a9bc`.
Independent verification passed ZIP integrity and safe-path checks, all seven
manifest hashes, required content, helper digest binding, AMD64 PE/import
checks, and normalized comparison of all 27 packaged source/text files to the
tested commit. The helper SHA-256 is
`d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The
reproducible verifier and report are `outputs/verify_run227.py` and
`outputs/Soggfy-run227-package-evidence.json`. The verifier was rerun locally
and passed.

The installed Spotify executable reports version 1.3.1.234. Its 42,852,224-byte
`Spotify.dll` has SHA-256
`210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`, matching
the pinned runtime artifact. No Spotify process was running during this check.
The available deep-analysis providers are not configured on this workstation
(Ghidra and IDA); this check confirms file identity only, not runtime behavior.
Provider selection, callback completion and synchronized lifetime, decoder-to-
playback association, ad/DJ/interstitial isolation and transitions, exact-
instance current-track skip, and production MP3 capture remain unverified.
Production native MP3 capture and saved-recording metadata refresh remain
disabled. Live installation/restart and Classic save-dialog acceptance, PR #64
merge, and official release remain separate gates.

## November 25 CI run 226 and independent package verification

Documentation/source commit `50c36577b94627030717cd872ed74ae13059bf7d`
passed all four jobs in [CI run 226](https://github.com/wefalltomorrow/Soggfy/actions/runs/45604564058): native regressions, both seeded 20,000-iteration ASan/UBSan
campaigns, Windows media-observer and maintenance tests, the Windows
publication/settings/logging/metadata/quality suite, and the x64 release
build/package checks.

GitHub artifact `15017619031` is bound to that exact commit. Its 3,315,454-byte
outer ZIP has SHA-256
`8fa3feb7dc01c80b19a825e9efb06ee8430b579abf4d50ccd8d660bc9a657e69`; the
3,317,496-byte RC88 package has SHA-256
`b391019e22c5dfaab06a8a6f66ae99d672bf225faa69cfe78bda4fc0b626b55b`.
Independent verification passed ZIP integrity and safe-path checks, all seven
manifest hashes, required package content, the DLL-bound validation-helper
digest, AMD64 PE/import checks, and normalized comparison of all 27 packaged
source/text files with the tested commit. The helper SHA-256 is
`d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The
reproducible verifier and report are `outputs/verify_run226.py` and
`outputs/Soggfy-run226-package-evidence.json`.

A follow-up static recheck again matched the pinned Spotify DLL
(42,852,224 bytes; SHA-256
`210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`) and
passed 1,299 identity-path, 81 dispatch/import, and 35 scheduler-boundary
instruction checks using Capstone. The reports are the three
`outputs/*20261125-followup1.json` files. This confirms the inspected static
paths only: no Spotify execution or usable Ghidra/REA provider evidence was
available. Runtime provider selection, callback completion and synchronized
lifetime, decoder-to-playback association, ad/DJ/interstitial isolation and
transitions, exact-instance current-track skip, and production MP3 capture
remain unverified. Production native MP3 capture and saved-recording metadata
refresh remain disabled. Live install/restart and Classic save-dialog
acceptance, PR #64 merge and official release remain separate gates.

## November 25 CI run 225 and playback-identity recovery fence

PR #64 source commit `054137c036dbe6ffd0aa593b6ec316721db16955` adds
playback-ID fencing to Classic recovery retries and skip decisions so work
from an earlier playback cannot act on a later track. All four jobs passed in
[CI run 225](https://github.com/wefalltomorrow/Soggfy/actions/runs/45600564087):
native regression tests, both seeded ASan/UBSan campaigns, Windows
media-observer and maintenance tests, the Windows publication/settings/logging/
metadata/quality suite, and release build/package checks.

GitHub artifact `15017535237` is bound to that exact commit. Its 3,315,013-byte
outer ZIP has SHA-256
`6bd40f9a5ac7e02887ddc3c9d38d75ef99c18c4f0916b154a730dac51a24053d`; the
3,316,980-byte RC88 package has SHA-256
`9f6a799833f74301e4ba0265975ff1aab803a679d3460ffe0b84328e69880f44`.
Independent verification passed ZIP integrity and safe-path checks, all seven
manifest hashes, required package content, the DLL-bound validation-helper
digest, AMD64 PE/import checks, and normalized comparison of all 27 packaged
source/text files with the tested commit. The helper SHA-256 is
`d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The
reproducible verifier and report are `outputs/verify_run225.py` and
`outputs/Soggfy-run225-package-evidence.json`.

These checks establish CI and package provenance, not live Spotify behavior.
Pinned runtime/provider selection, callback completion and synchronized
lifetime, decoder-to-playback association, ad/DJ/interstitial isolation and
transitions, exact-instance current-track skip, production MP3 capture, and
Classic save-dialog acceptance still need the stated live evidence. Production
native MP3 capture and saved-recording metadata refresh remain disabled. No live
installation/restart was performed; PR #64 merge and official release remain
separate gates.

## November 25 CI run 223 and independent package verification

RC88 passed all four CI jobs in [run 223](https://github.com/wefalltomorrow/Soggfy/actions/runs/45591471028) for exact source commit `2d1d757c13a669195ff4470db8b7371d0853294f`: native regression tests, both 20,000-iteration seeded ASan/UBSan campaigns, Windows media-observer and maintenance transactions, the Windows publication/settings/logging/metadata/quality suite, and the release build/package checks. GitHub artifact `15012825498` is bound to that commit. Its 3,313,310-byte outer ZIP has SHA-256 `e4cfbb0cbe52277e18f95278874066a278abea1f136ad5b78875e44b011f647b`; the 3,315,388-byte RC88 package has SHA-256 `a8d1c9b1f668c9d2c15790ed3f8b37ccec872562d11c0c8cec20db30e11a0ca8`. Independent verification passed ZIP integrity and safe-path checks, all seven manifest hashes, required contents, helper digest binding, AMD64 PE/import constraints, and normalized comparison of all 27 packaged source/text files to the tested commit. The helper SHA-256 is `d0e60dd3662ee57d585cb961f66b1617608e8f50b6cd11a120bc0831f190bd1f`. The reproducible verifier and report are `outputs/verify_run223.py` and `outputs/Soggfy-run223-package-evidence.json`.

Run 223 verifies CI and package integrity only. Pinned Spotify runtime selection, callback completion and synchronized lifetime, decoder-to-playback association, ad/DJ/interstitial isolation and transitions, exact-instance current-track skip, and production native MP3 capture still lack live evidence. Production MP3 capture and saved-recording metadata refresh remain disabled pending correctness, provenance, ownership, and lifetime evidence. Live installation/restart and Classic save-dialog acceptance, PR #64 merge, and official release remain separate gates.

## November 25 CI run 222 and pinned-client static recheck

RC88 passed all four jobs in [CI run 222](https://github.com/wefalltomorrow/Soggfy/actions/runs/45586601342) for exact source commit `ee7950113db8196071dce9c3d2cda78a96286f52`: native regressions, both 20,000-iteration seeded ASan/UBSan campaigns, Windows maintenance/integration and native suites, and release build/package checks. GitHub artifact `15010675443` is bound to that commit. The independently verified 3,312,703-byte outer ZIP has SHA-256 `83a5272ba3085d536938ace3f5ab4a199b6a2542a38eb5f5c68ad1e9b2232bc0`; its 3,314,751-byte RC88 package has SHA-256 `18e09f6f873fdb0366c0cca7c15e8c66872742870c684542b06691ac9c7be5e8`. ZIP integrity and safe paths, all seven manifest hashes, required files, helper digest binding, AMD64 PE/import constraints, and normalized comparison of all 27 packaged source/text files passed. The reproducible verifier and report are outputs/verify_run222.py and outputs/Soggfy-run222-package-evidence.json.

On November 25, the installed Spotify.dll was rechecked at 42,852,224 bytes and SHA-256 `210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f`, matching the pinned artifact; no Spotify process was running. Fresh hash-pinned static probes passed 1,299 identity-path instruction checks, 81 dispatch/import checks, and 35 scheduler-boundary checks using Capstone 5.0.7. Their reports are outputs/Soggfy-native-identity-parent-20261125.json, outputs/Soggfy-native-identity-dispatch-context-20261125.json, and outputs/Soggfy-native-notification-scheduler-boundary-20261125.json.

This confirms artifact identity and the inspected static paths only. No usable native analysis provider was configured. Runtime provider selection, callback completion and synchronized lifetime, decoder-to-playback association, ad/DJ/interstitial isolation, transitions, exact-instance current-track skip and production native MP3 capture remain unverified. Saved-recording metadata refresh remains disabled. Live installation/restart and Classic save-dialog acceptance, PR #64 merge and official release are separate gates.

## November 25 Floggfy review and native reliability fix

Prepared as v3.0.0-rc.88 for PR #64; the published release remains v3.0.0-rc.61.
The candidate is not live-accepted or officially released.

The Floggfy v1.4.1 source and release comparison is recorded in
FLOGGFY_20261125_REVIEW.md. Failed-write cleanup now marks the original open
file handle for deletion, with a Windows regression proving that a replacement
created at the old path is preserved. The complete test-native-windows.sh
suite passed locally with MinGW x64 GCC 16.2.0/MSVCRT, including the strict
kernel32.dll/msvcrt.dll helper-import policy. The import verifier now reads
both GNU and LLVM objdump PE metadata while keeping that allowlist unchanged.
An isolated full release build and ZIP verification passed, including PE/import
checks, deterministic packaging, and archive-manifest validation. The packaged
helper was byte-identical to the tested helper. All four jobs passed in [CI run
221](https://github.com/wefalltomorrow/Soggfy/actions/runs/45582817885) for exact
source commit `295488f687bf44f86c0be12c863ea6f2a33e37af`: native regressions, both
20,000-iteration seeded ASan/UBSan campaigns, Windows maintenance/integration
and native suites, release build, and package checks. GitHub artifact
`15008318103` is bound to that commit. Its 3,312,173-byte outer ZIP has SHA-256
`0d7a200b369265081e350bc488cf4311453c72f7b1c998bc70b51a2cae310c6d`; the
3,314,216-byte RC88 package has SHA-256
`0a5a69bb7228fd3adbca2a09a7547562a46009933dfa85280869a60f333f68db`.
Independent verification passed ZIP integrity and path checks, all seven
manifest hashes, required content, helper digest binding, AMD64 PE/import
checks, and normalized source comparison for all 27 packaged text/source
files. The verifier and result are `outputs/verify_run221.py` and
`outputs/Soggfy-run221-package-evidence.json`. Pinned Spotify live acceptance
remains outstanding; this candidate is not an official release.

## November 20 CI run 219 and independent package verification

Source commit `06482452065cc0f85e269737a5a5e4500efe8172` passed all four
CI jobs in run 219 (`44734949744`): native regressions, both 20,000-iteration
seeded ASan/UBSan campaigns, and Windows maintenance/integration/release
packaging. GitHub artifact `14629411727` is bound to that source commit. Its
3,310,454-byte outer archive matches GitHub SHA-256
`75ddf3be99647910c74eb2b4fefedf6ac3c780b8a7af483cf0c8b3218e2864f4`. The
3,312,377-byte RC87 package has SHA-256
`3a35c8f101948248a51831cf77a61a6a42c2949bbd288246b73412308de88a93`.
Independent checks passed ZIP CRC and path safety, all seven manifest hashes,
required files, validation-helper digest binding, AMD64 PE/import checks, and
normalized exact-source comparison for all 27 packaged text/source files. The
reproducible verifier and report are in `outputs/verify_run219.py` and
`outputs/Soggfy-run219-package-evidence.json`. This verifies CI/package
provenance; it does not close the pinned-client runtime or live-acceptance gates
below.

## November 20 CI run 218 and independent package verification

Source commit b93d88d97c835a6c80f4b53b59aeb02a757be710 passed all four
CI jobs in run 218 (44728484638): native tests, both 20,000-iteration
seeded ASan/UBSan campaigns, and Windows release packaging. GitHub artifact
14628095070 is bound to that source. Independent verification passed
outer/inner ZIP integrity, all seven package-manifest hashes, safe archive
paths and required files, validation-helper digest binding, AMD64 PE/import
checks, and normalized exact-source comparison for all 27 packaged text/source
files. The outer artifact is 3,310,185 bytes, SHA-256
92a50db942c5b2c49de8c6d0f8e87dc45bbbc0d9e168631654de994b0d9b282e;
the RC87 package is 3,312,111 bytes, SHA-256
3afbbe07437043c6da6d95d09b4c4f24e3da15aa685a1e7ca1f6b255d0fa5d86.
The reproducible verifier and report are in
outputs/verify_run218.py and outputs/Soggfy-run218-mp3-evidence.json.

This run includes bounded Layer III pair/count1 Huffman syntax traversal,
linbits/sign handling, per-granule budgets and switched-block region selection.
It does not reconstruct spectral coefficients or establish recording
completeness, provenance, native lifetime or production capture readiness.
Production MP3 capture remains disabled. At that checkpoint PR #64 was open
and unmerged; live-client acceptance and official release remain separate
gates.

## November 20 notification scheduler boundary

Added a hash-pinned static probe for the notification scheduler handoff. Its
35 checked instruction locations and a bounded .text call scan confirm that
the sole factory caller obtains a 32-byte value through a provider's virtual
+8 slot and passes it as context through 0x82d7fc and 0x82bac0 into
streaming-object +8. Task initialization/submission/cleanup dispatch through slots
+0x60/+0x68/+0x70. Cleanup passes a zero flag before destroying the task-side
object. The concrete runtime scheduler and the meaning of its cleanup call
remain unresolved; queued-work cancellation does not prove running-callback
drain. Probe evidence: outputs/Soggfy-native-notification-scheduler-boundary-20261120.json.

The pinned DLL is read statically and is not executed. At the CI 218 checkpoint,
PR #64 was open and unmerged; live-client acceptance and release remain
separate gates.

## November 20 native identity lazy-initialization path

Extended the hash-pinned callback probe to verify both KERNEL32.dll InitOnce
imports and their IAT call sites, the callback helper's +0x70 state argument,
the initializer's sole direct caller, the writer's sole direct caller, the
constructor's zeroing of +0x70/+0x78/+0x80, and the writer's unconditional +0x78
and conditional +0x80 stores. All 81 checked instruction locations and the
direct-reference/import checks pass against the pinned Spotify DLL. This
establishes a static lazy-initialization path before the selected-field read;
it does not establish runtime factory selection, value semantics, identity
correctness, full lifetime, or synchronization of later reads/replacement.
Evidence: outputs/Soggfy-native-identity-dispatch-context-20261120.json.
Live-client acceptance, PR #64 merge, and release remain separate outstanding
gates.

## November 20 native identity callback dispatch correction

The pinned callback-context follow-up corrected a provisional RVA mix-up:
factory 0x8527c0 points to an object vtable at 0x19eede0, while the
_player.proto.PlayedItems descriptor is separately located at 0x1a1ede0.
The vtable's slot +8 resolves to 0x852c34, which selects one of two inner
members and fills the callback's 32-byte output buffer. The dispatch-only check
validated 57 instruction locations and the sole direct factory caller; the
lazy-initialization follow-up below extends the same probe to 81 checks. This
clarifies static dispatch only; the data's semantic identity, live selection,
lifetime and post-initialization synchronization remain unverified. See
NATIVE_IDENTITY_AUDIT.md and
outputs/Soggfy-native-identity-dispatch-context-20261120.json.

## November 20 busy-publication retry verification

Commit `b53a2e414397f0814afbe909c6abd012acb880c2` distinguishes a busy destination from
permanent publication failure. It retains the complete capture for up to three
retries (2, 10 and 30 seconds), subject to a two-job / 512 MiB bound, and cancels
retained work when the settings generation or capture epoch changes. This keeps
retry ownership explicit while preserving the existing destination policy.

CI run 216 (`44678355380`) passed all four jobs for that exact source commit. GitHub
bound artifact `14603764378` to the commit. Independent verification passed ZIP
integrity, all seven package-manifest hashes, required-content and helper-binding
checks, AMD64 PE header/import checks, and a normalized comparison of 27 packaged
text/source files with HEAD. The outer artifact is 3,304,734 bytes, SHA-256
`94a2be817c54290ea5a91b566014ef82afbeb0274584570d5b12e86124f403ad`; the inner RC87
package is 3,306,710 bytes, SHA-256
`333713a8321410b21d10be15e2ac00da7949a511917af324ba6a40a001e773b6`. Evidence and the
reproducible verifier are in `outputs/Soggfy-run216-retry-evidence.json` and
`outputs/verify_run216.py`. This confirms CI and package provenance, not live-client
acceptance; PR #64 remains open and unmerged.

## November 20 CI artifact verification

CI run 214 (`44673329029`) passed all four jobs for source commit
`d8fde9ad98322246b118481fd6b354ee80393240`. GitHub binds artifact
`Soggfy-Windows-x64` (`14601322917`) to that exact commit. The independently
downloaded artifact is 3,290,783 bytes and matches GitHub's SHA-256
`32e55fa37b83e1ca1d016e1d5f3e1fa7a62c05d6f474c26012e916e9a7468a37`. Its
RC87 release package passes ZIP integrity checks; all seven entries in
`SHA256SUMS.txt` match. The inner package SHA-256 is
`d957b6513eb6dc79fc693d9d02eed11bb7ac024cd287e37942677d26213e12a8`.
`BUILDINFO.txt` identifies Soggfy 3.0.0-rc.87 and the x86_64 MinGW target.
This verifies CI packaging and artifact provenance, not pinned-client acceptance.

The November 20 upstream recheck found no Floggfy release or main-branch commit
newer than v1.4-RC2 (November 15) and commit
`1e0201492afc72702337fa4980dda9314b9ab000` (November 14). No additional upstream
port was identified.

November 19 upstream review: FLOGGFY_20261119_REVIEW.md pins stable v1.3 and
prerelease v1.4-RC2, records gaps and updates implementation order. Busy-file
retention/retries, unmatched recordings and expanded telemetry are not implied
complete by earlier selective ports. Bounded stream evidence and replay are now
implemented and passed full CI/package verification in run 214, as recorded above.
It also records a canceled-conversion/DONE status defect, corrected in source
with an explicit canceled result. Local object compilation and Classic status
regressions pass; full CI/package verification passed in CI run 206 as recorded
below.

## November 19 bounded stream evidence and replay

The current PR diff adds opt-in Ogg-page and FLAC compressed-read evidence,
unique per-process session folders, 48 MiB payload and 16 MiB index caps
(64 MiB combined), explicit queue-loss and incomplete markers, and a replay
validator that checks the JSONL schema, offsets, CRCs, file bounds and
completion before reconstructing streams. Partial reconstruction requires
`--allow-incomplete`. `StreamDump=0` remains the default; see AUDIO-HISTORY.md
and FLOGGFY_20261119_REVIEW.md.

Local verification passed for the replay corpus, concurrent bounded queue,
Windows logger lane and history tests, and audio_history.cpp object compilation
with Zig 0.13 targeting x86_64 Windows GNU. The object compile suppresses Zig/Clang's
-Wmissing-field-initializers diagnostic for pre-existing Publication
aggregate returns; other -Wall -Wextra -Werror checks remain enabled. Full
CI/package verification passed in run 214; pinned-client live evidence remains
pending.

## November 19 cancellation and compiler compatibility checkpoint

Source `fed216eb6f04db77590cbf7a1fd74bea5be4f1f3` passed all four jobs in
CI run 206 (`44504382147`), including both deterministic sanitizer seeds and
Windows integration/build checks. The first attempt timed out downloading
Ubuntu dependencies for one sanitizer job; rerunning the affected jobs passed.
Earlier runs 203-205 exposed GCC 15 diagnostics in test fixtures: the ID3
mutation now uses bounds-checked access, and hook/playback-speed discovery
fixtures use fixed storage for their optional duplicate entries. Strict
warnings remain enabled, and both duplicate-target ambiguity tests remain.

Independent MinGW package verification passed against merge commit
`0b644df75ff701f935c4d7fe9375adadf5a5ca0a`; its tree equals the source tree
`0c39a1c6f3b2f6b9641e6f6588c409013ce98c96`. The preserved RC87 package is
separate from earlier RC87 artifacts. Package SHA-256:
`c1cd57e43fe50474b9f11674f08d5c0dbef410ab2d588f72469232baac1f6bfe`.
Artifact SHA-256:
`1de3ab05b24ab739b935b7b6a56968a42a50178b386e35e8b6ea20ad196676b5`.
ZIP integrity, manifest, source scripts/docs/configuration, embedded Classic
scripts, AMD64 PE imports, codec licenses and DLL-bound validation helper
were checked independently. This is not a live cancellation-race test or
completion of the open milestones. The staged RC86 live-acceptance package
has not been replaced or installed.

Updated November 8, 2026. RC86 passed all four jobs in CI 42475271762 and
independent artifact verification at source 2d5e9f0cc9f90d47acdb7f7f1a8ff618324535ca.
It is a checkpoint, not completion of all work. RC87 adds supervised artwork
validation to the packaged helper. All four jobs passed in CI 42485859840 at
source 4376d13fbf893b1fb252101e047195570592cd3c, and independent MinGW package
verification passed. Live acceptance remains separate.

| Milestone | State | Completion evidence still required |
| --- | --- | --- |
| RC63-74 selective reliability work | CI/package checkpoints complete | Pinned-client acceptance remains separate |
| Converter error reporting | RC86 CI/package verified | Native and both PowerShell versions reject zero-exit stderr errors and pipe-filling diagnostics in encoding/validation. A real damaged MP3 is rejected even when its wrapper masks the decoder exit status. All four CI jobs and independent MinGW package verification passed; live acceptance remains |
| Installation-directory transaction | RC75 CI/package verified | Live installation acceptance |
| Manual watch scan cost | RC75 CI/package verified | Live acceptance |
| Native playlist save result | RC75 CI/package verified | Live save dialog |
| Native playback/decoder identity | Static candidate traced through consumer, configured generator, recovery invocation and teardown | Null fallback places the upstream read outside its allocation; a traced non-null path spans separate byte fields and part of an ownership pointer. A separate candidate has playback-ID consumer, hex-encoding and configured 16-byte generator evidence. Audio-driver recovery has a conditional synchronous replacement-callback path; teardown replaces a shared byte flag and conditionally destroys the native object when its strong count reaches zero. One callback checks the token before using its stored pointers; its clone retains that guard and is built alongside a copy of the candidate ID. The dispatch-provider pointer is traced to its service-table entry. A registered service-zero candidate now traces configuration ownership into a streaming object: ID at +0x350, guarded wrapper at +0x3a8, and conditional destructor cleanup. Two component cleanup submissions have a completion-counter wait before wrapper destruction; 1,299 pinned instruction checks pass. The notification scheduler handoff now has a 35-instruction pinned probe and unique direct-call path; a provider returns the 32-byte context, while concrete runtime selection and whether cleanup drains running callbacks remain unresolved. This does not establish that those component destructors drain all audio callbacks. On 2026-11-20, the installed Spotify.dll matched the pinned 42,852,224-byte artifact SHA-256; no Spotify process was running and REA had no usable native provider, so this is identity verification only. Runtime provider selection, callback completion, synchronized lifetime, decoder association, integration and transitions remain unverified; see NATIVE_IDENTITY_AUDIT.md |
| Ad/DJ/interstitial isolation | Open | Trustworthy identity plus transition/live evidence |
| Native podcast MP3 capture | Offline inspector only; production capture disabled | Bounded MPEG-1/2/2.5 layout, CRC, reservoir, scalefactor and Layer III Huffman syntax checks cover 36 real mono/stereo CBR/VBR fixtures plus 420 reserved-selector mutations. Runs 217 and 218 passed full CI; run 218 also passed independent RC87 package verification at exact source b93d88d. Syntax traversal does not reconstruct spectral coefficients or establish completeness, provenance, decoder association or capture lifetime. Native capture remains disabled |
| Concurrent capture/cross-format association | RC84 shared-ready rejection and RC85 retention-conflict fix CI/package verified | Late first retention now rejects candidates shared by queued listens, including competing ready captures. Native identity-aware lifecycle, future-listen/preload and live transition evidence remain |
| Playback metadata refresh/instance-bound skip | Partial | RC78 queue fencing and RC79 exact-instance active/final/pending metadata refresh CI/packages verified; native current-track skip remains open |
| Saved-recording metadata refresh/preservation | Offline preparation and RC82 helper deployment verified | Inspection, isolated Ogg/FLAC decoding, minimal text/lyrics policy, output verification and preparation passed CI. RC82 DLL-bound helper packaging and installation passed CI/artifact verification. Local probes demonstrate concurrent ADS, attribute and DACL mutation despite the source lock. Same-file stream swaps preserve tested metadata but expose an empty default stream; even a reader can block publication and rollback. That approach remains disabled. Artwork envelope inventory and RC87 supervised PNG/JPEG decoding are CI/package verified; filesystem metadata preservation, artwork source/replacement policy and production coordinator remain; see METADATA_REFRESH_DESIGN.md |
| Log levels 0/1/2/4 and bounded lanes | RC78 CI/package verified | RC77 test collision corrected in RC78; live acceptance remains separate; both lanes share one disk worker |
| Deterministic sanitizer/fuzz campaign | RC76 campaigns passed | 40,000 seeded iterations plus regressions; does not prove live callback safety |
| NetworkPatch policy | RC83 and RC84 CI/package verified | Explicit default-off startup opt-in, strict bounded INI parsing, preserved external edits and disabled IAT bypass. Upgrade behavior documented. Pinned-client acceptance remains |
| Pinned Spotify live acceptance/stalls | Separate gate | Authorized exact-artifact restart and walkthrough |
| Merge/official release | Separate gate | Review, acceptance and authorization |

Preserve Classic UI, file-based status, Unicode/multiple artists, custom/legacy/
podcast paths, cached PlayerAPI, per-entry UIDs, 1x-30x, Downloads-off effective
1x with selected speed retained, MP3 320 kbps and strict Ogg/FLAC validation.
Only MinGW CI binaries ship. Preserve earlier packages and source archives.

## Offline artwork decoding checkpoint

The bounded PNG decoder passed all four jobs in CI 42481391151 at source
790de51ab4662c57c751320ecf55af210c786415. Independent package verification
matched source/merge trees, scripts, embedded Classic, PE imports, hashes and
helper binding. The DLL, launcher and helper remain identical to CI190. The
package SHA-256 is b5b4378b9ef28b66cbe96452dac1f4687d47a80f26d79fe9759785bbea5633c5.
Ninety PNG layouts and 101 malformed cases run in each job. The preceding run
failed on missing math-library linkage; this run includes the corrected link.

JPEG now has a bounded decoder and regression corpus. The standalone decoder
passed all four jobs in CI 42483537187 at source
4cb2f1dc09dd8965d741b017db26b61f7ba6438e. Each job covers 22 valid/scan-boundary
and 1722 malformed/resource-limit cases. Independent package verification passed;
the DLL, launcher and audio helper match CI190. RC87 subsequently verifies
supervised-worker integration; source/replacement policy remains unfinished.
Progressive JPEG scan-boundary probes demonstrate that valid decoding alone
cannot prove original download completeness. No production
artwork replacement or saved-recording metadata refresh is enabled.

RC87 integrates both image decoders into the build-pinned helper with a distinct
32-byte image reply. The shared supervisor checks the complete reply, retains
source/worker locking and handle restrictions, and enforces cancellation and a
deadline. Artwork has a 4 MiB input cap and 128 MiB process limit in addition to
each decoder's 32 MiB allocation quota. Local Windows diagnostic tests cover
valid/malformed images and supervisor faults; existing audio supervisor tests
also pass. All four jobs passed in CI 42485859840, including 10 valid and 32
malformed/resource-limit images through the actual Windows supervisor plus
reply, resource-limit, handle-isolation, cancellation and deadline faults.
Independent verification matched the source/merge tree, embedded Classic,
scripts, hashes, codec licenses, helper binding and PE dependencies.
Package SHA-256: 5236268bc1cd3f2c478fb151506de95198c975faa8d220f26506e6eb7260201f.
The helper SHA-256 is 16a4edcbf8458acbbc00682123653fe498dbbc49961c471d771fb14469aa6fb0.
Saved-file refresh and artwork replacement remain disabled. The separately
staged RC86 live-acceptance target has not been replaced or installed.

## MP3 reserved Huffman selectors

The offline inspector now rejects pair-codebook selectors 4 and 14 in every
declared region, including empty regions. This is an explicit strict-input
policy; decoder tolerance is not evidence of a supported declaration. The
pinned FFmpeg mpegaudiodec_common.c maps both selectors to table zero, and
mpegaudiodec_template.c can synthesize zero coefficients for that mapping.
Switched blocks have two selectors; the following subblock gains must not be
mistaken for a third. No Huffman payload decoding or production MP3 capture
is enabled by this check.

Local diagnostic tests pass 6,912 selector declarations across all MPEG
versions, granules/channels, long/switched blocks, CRC and fixed-block input.
The real corpus passes 420 reserved-selector mutations while accepting all
36 original encodings and 18 protected PCM-equivalent streams. The existing
scalefactor, Xing, CRC and reservoir checks also pass. All four jobs in CI
42488084676 passed at source 85513d6f3ce650a44172d74910745f534810bcc1.
Each job passed all 6,912 declarations and 420 real-stream selector mutations.
Independent MinGW package verification passed source/merge tree equality,
ZIP/manifest hashes, embedded Classic scripts, codec licenses, helper binding
and PE dependencies. Package SHA-256:
e0b5d47887c0268b381115eacd877cfd1858d1d32cc48e3e137ef098851e05b9.
DLL, launcher and helper are byte-identical to CI201. Evidence and full logs:
outputs/Soggfy-run202-regression-evidence.json and outputs/CI-run202/.

## MP3 protected-frame follow-up

The offline Layer III inspector now validates protected-frame CRC over the
last 16 header bits and complete side information. It rejects coefficient-pair
counts above 288 and switched blocks with type zero. Main audio bytes remain
outside the CRC coverage; passing this inspection does not establish valid
Huffman data, complete playback coverage, gapless timing or native ownership.

Local diagnostic C++ boundary tests and the 36 existing MPEG-1/2/2.5 real
mono/stereo CBR/VBR fixtures pass. Another 18 reservoir-disabled real encodings
are reframed at a larger bitrate to insert CRC and stuffing without removing
compressed bytes. FFmpeg with crccheck+explode independently accepts them and
decodes identical PCM. Header, stored CRC and side-info corruption in middle
frames are rejected by both implementations. Another 72 real-stream mutations
exercise the two side-information constraints with matching rejection causes.
Synthetic tests cover every granule/channel record and every protected bit.
All four jobs passed in CI 42474262462 at source
30281a92f15cb52166b754c98eaf2f1104e3cd97. Each job ran the 36 original
fixtures, 18 protected PCM-equivalence cases and 72 side-information mutations;
the two sanitizer campaigns also completed 40,000 seeded iterations in total.
Independent MinGW package verification matched the source/merge tree, scripts,
hash manifest, helper binding and PE dependencies. The DLL, launcher and helper
are byte-identical to the original RC85 binaries. Package SHA256:
3af36c5c4b03bb236f59c4fbc892c50dfad409bacfb013d60db6dbdbd095b8ad.

Primary implementation cross-check: FFmpeg commit
cc583787bbecd4914fee7c2bf7f09463113822fe, libavcodec/mpegaudiodec_template.c,
handle_crc and Layer III side-information parsing. Retained source SHA256:
ebc2a30146f84bb7bc8cb67ea28cc2af444cdfebd22e4013adbeab6ecf9a5430.
These additions remain offline; no native MP3 capture or production audio
validation was enabled.

## MP3 decoder candidate audit

On November 8, 2026, minimp3 commit
ea99364f61c14656440e8d77e9c233ccf3124633 was evaluated locally, outside the
shipping build. Header SHA256:
57e437c5c1f0e8b243885d3929c8973b5e6c778451e0100ab4251d19915cb3ad.
It is not sufficient as a strict validator through its unmodified public API.

A real MPEG-1 mono encoding was mutated at frame 10, granule zero: the declared
part2_3 length was set to zero while scalefac_compress was set to 15. Its long
block requires 74 scalefactor bits before Huffman data, so the declaration is
inconsistent. The RC86 structural inspector accepts it. minimp3
consumes all 59 frames and returns all 67,968 nominal samples without an empty
decode or skipped frame, just as for the original. Local FFmpeg 9.0.2 also
returns zero without error-level diagnostics, but produces different PCM.
This is a separate limitation from RC86's handling of reported decoder errors.

Source inspection confirms L3_decode has no error return for scalefactor
overrun; L3_huffman restores the declared granule bit position. Counting
decoded samples and forbidding resynchronization cannot establish strict
payload validity. Do not connect this candidate to production validation
without explicit semantic rejection and independent malformed-stream tests.
The reproducible local candidate audit and fixture hashes are preserved in
outputs/Soggfy-MP3-decoder-candidate-audit.json, with the harness under work/.
No decoder dependency, native MP3 publication, or new runtime hook was added.

## MP3 scalefactor-bound follow-up

The offline inspector now rejects granules whose declared part2_3 bit budget
cannot contain their scalefactors. It accounts for all MPEG-1 compression
values and second-granule reuse flags; short blocks do not use those flags.
MPEG-2/2.5 ordinary and intensity-stereo compression ranges use the appropriate
long, short or mixed partition sizes. Intensity coding applies only to the
right channel in joint stereo. No compressed audio is decoded by this check.

Local cross-checking against actual scalefactor consumption in the pinned
minimp3 source passed all 20,480 version/compression/reuse/block/intensity
combinations. Synthetic frame tests exercise exact and one-bit-short budgets,
CRC/non-CRC layouts, every granule/channel and intensity mode isolation.
The real-corpus regression adds 72 impossible scalefactor declarations across
all MPEG versions, mono/stereo and CBR/VBR, while preserving acceptance of the
36 original files and 18 protected PCM-equivalence fixtures. All four jobs
passed in CI 42476996697 at source 2e5e81fd7e6e2b1125b8f2ee16032e4110cd9c53.
Independent MinGW package verification matched the source/merge tree, scripts,
manifest, helper binding and PE dependencies. DLL, launcher and helper are
byte-identical to CI 42475271762. Package SHA256:
03103325ee0a094bc627d6e59bd43773419ced1c5b0f7822bc2eed29286549fb.
Full Huffman semantics, gapless
timing, completeness and capture lifetime remain open; production MP3 capture
is still disabled.

## MP3 Xing/Info extent follow-up

The offline inspector now bounds first-frame Xing/Info optional fields and
compares declared audio-frame and stream-byte counts with observed extents.
Counts exclude ID3 and include the seek frame only in the byte extent. The
seek frame is excluded from reservoir history; the first audio frame cannot
borrow its metadata bytes. Protected seek frames retain the fixed Xing offset
used by LAME and still require a valid MPEG header/side-information CRC.

Local synthetic cases cover all 16 flag combinations, optional-field bounds,
all MPEG versions/channel layouts, protected frames and fixed-block input.
Another 36 real tagged encodings cover mono/stereo and CBR/VBR at all nine
supported sample rates. They pass independent packet-count comparisons, 360
count/truncation/reservoir rejection cases, ID3 exclusion and protected seek
header PCM-equivalence checks. All four jobs passed in CI 42477717796 at source
9b45a527956813bb9b5dbe704396a444691af8bd. Independent package verification
matched the source/merge tree, scripts, manifest, helper binding and PE
dependencies. Package SHA256:
c18a876a3fc7e37f58173f308b9b92b1a6d46ef23e79973a809210e3b9151db2.

These are consistency checks on untrusted declarations. A stripped or rewritten
tag cannot establish completeness; absent counts remain unknown. Nominal sample
counts still include physical metadata frames and are not presentation timing.
Encoder delay/padding, LAME extension CRC, VBRI, iTunSMPB, full Huffman semantics,
provenance and capture lifecycle remain separate work. No native capture enabled.
Primary cross-checks: FFmpeg cc583787bbecd4914fee7c2bf7f09463113822fe,
libavformat/mp3dec.c and mp3enc.c; LAME libmp3lame/VbrTag.c, lame_get_lametag_frame.

## Gapless tag compatibility audit

The local FFmpeg 9.0.2 audit generated 36 real Xing/Info fixtures across all
nine sample rates, mono/stereo and CBR/VBR. In every fixture, declared audio
frames times samples-per-frame minus encoder delay and padding matches the
independently decoded PCM sample count. This does not establish trustworthy
delay/padding for native capture or files with changed metadata.

Only six fixtures match a LAME tag CRC calculated from the MPEG header through
the byte immediately before the stored tag CRC. All 36 match the fixed
190-byte calculation in pinned FFmpeg mp3enc.c (CRC placeholder zeroed when
inside that extent, and zero padding beyond a short seek frame). The six
matching both calculations are MPEG-1 stereo; the other layouts place the
CRC earlier. Requiring just the LAME field-boundary checksum would reject
these 30 otherwise decodable local FFmpeg fixtures. No permissive production
fallback has been introduced; extension compatibility remains explicit work.

Evidence and fixture hashes: outputs/Soggfy-MP3-gapless-tag-audit.json.
Harness: work/probe_mp3_gapless.py; CRC oracle includes the CRC-16/ARC standard
check vector. Pinned mp3enc.c SHA256:
8dec50d701aafec3b1d81fb47ad1a9e3144938cbcf776a5f5633d0b091883f33.

## Independent decode fixture correction

Positive FFmpeg fixture generation, probing and decoding must now produce no
error-level diagnostics, even with a zero exit code. Both MP3 and metadata
refresh harnesses exercise a zero-exit error process to check this requirement.
Native test programs retain their own pass/fail assertions; debug libFLAC may
print normal rewind messages while they exercise expected failure cases.

This exposed malformed artwork in the real metadata fixtures: the synthetic
"opaque" picture comment produced an FFmpeg error even though decoded audio
matched. Real fixtures now contain a generated valid 1x1 PNG, independently
decoded to the expected RGB pixel before and after refresh. Malformed artwork
preservation remains covered by synthetic container tests. All 12 real Ogg/FLAC
cases pass locally through isolated validation and preparation, with identical
PCM, preserved artwork pixels and clean FFmpeg diagnostics. The complete MP3
corpus also passes the stronger harness. CI run 42479107718 passed native,
Windows and both sanitizer jobs. Source 4bd96eccaf1dd3840cf602e1b068062c410fccec
and CI merge e8e0e63a209a9a144f40dcf14823ab76c1082277 share tree
1bc28a14d7f0e18cc7a9dae15ed7da1209322609. Independent package verification
passed for SHA256
74c1bee5a5640646e0558e4ddef8dea7f342db5fb79b2e6790a57bbf821030ec.
DLL, launcher and helper are byte-identical to CI190. These are fixture/evidence
corrections; runtime validation and the disabled metadata refresh coordinator
have not changed. CI193 exposed a harness mistake: requiring clean stderr from
native debug programs rejected normal libFLAC rewind messages. CI194 fixes that
by applying the clean-diagnostic requirement to every FFmpeg operation only.

## MP3 gapless trim metadata follow-up

The offline Xing/Info parser now recognizes bounded nine-byte encoder identifiers
with the LAME-compatible delay/padding fields (LAME, Lavf and Lavc). It reports
the observed audio-frame sample extent after the declared trim and rejects a
declaration whose combined trim exceeds those frames. This remains untrusted tag
metadata and does not establish a valid compressed payload, complete recording,
or production capture readiness. Native cases cover MPEG-1/2/2.5, mono/stereo,
protected/unprotected frames, all three identifiers, a truncated extension, and
an impossible trim. The independent FFmpeg integration corpus confirms parsed
delay/padding against decoded PCM length for all 36 Xing/Info fixtures. Both the
native suite and complete MP3 integration corpus passed locally under UBSan;
`git diff --check` and Python syntax validation passed. ASan and CI have not run
for these uncommitted changes.

## MP3 VBRI extent follow-up

The offline Layer III inspector now recognizes the fixed-offset VBRI header,
bounds its optional TOC within the first frame, and checks its declared stream
byte and audio-frame counts against observed extents. As with Xing/Info, the
VBRI metadata frame is excluded from reservoir history. FFmpeg's parser treats
the VBRI frame count as audio frames and skips the metadata frame when deriving
duration, while physical frame and nominal-sample totals here still include it.
This cross-check follows FFmpeg
cc583787bbecd4914fee7c2bf7f09463113822fe, which reads VBRI at frame offset 36.

New C++ cases exercise all supported MPEG versions, mono/stereo, protected and
unprotected frames, version/count errors, malformed table dimensions,
truncated header/table bounds, and reservoir isolation. The integration harness
also converts 36 FFmpeg-encoded Xing/Info seek frames to synthetic VBRI headers
and checks native inspection, FFprobe packet counts, untrimmed frame-PCM
equivalence (the Xing/LAME gapless extension is intentionally removed), and
malformed-count/table rejection for 216 bad VBRI declarations. A
local interoperability spot-check parsed and decoded one 16-frame MPEG-1
stereo stream (3,551 bytes) after VBRI retagging. An independent FFmpeg/FFprobe
matrix then passed all 36 rate/channel/CBR/VBR combinations: packet counts and
decoded PCM match with automatic Xing/LAME gapless trimming disabled on both
streams. This comparison intentionally isolates frame audio because retagging
removes the Xing/LAME delay/padding extension. These are synthetic VBRI tags,
not output from a VBRI encoder. The native C++ suite was compiled with the
checksum-verified LLVM-MinGW 2026-11-17 toolchain (Clang 23.1.6) and passed;
the complete FFmpeg integration harness also passed, including the VBRI matrix.
The portable compiler lived in a task-specific temporary folder and was not
installed system-wide. On November 20 the current worktree was recompiled with
`-std=c++17 -Wall -Wextra -Werror`; the native suite passed, followed by the full
integration harness against FFmpeg/FFprobe 9.0.2. This rerun passed the complete
36-stream corpus, 216 malformed VBRI declarations, existing Xing and reservoir
checks, CRC and side-information mutations, and clean-diagnostic fixture guard.
`git diff --check` also passes. UndefinedBehaviorSanitizer coverage passes for
the same native suite and full integration harness.
AddressSanitizer and the repository's seeded sanitizer campaign remain pending; the
available Windows LLVM-MinGW distribution has no compatible AddressSanitizer runtime.
Spectral reconstruction and full Huffman semantic validation, recording
completeness/provenance and capture lifecycle remain open, and production MP3
capture remains disabled. Gapless trim parsing is a bounded metadata check
only. The bounded syntax traversal described in the following follow-up does
not close those remaining validation requirements.

## MP3 Huffman payload syntax follow-up

The offline Layer III inspector now traverses bounded pair-codebook payloads,
including linbits escapes and sign bits, followed by count1 symbols and their
sign bits. It applies each granule's declared bit budget and selects Huffman
regions across long, start, stop, ordinary short and mixed-block boundaries.
Focused tests check exact and one-bit-short limits for table 16 and 31 escapes
and both count1 tables, plus region crossings in all switched-block layouts.
This establishes bounded syntax traversal; it does not reconstruct spectral
coefficients or establish whole-recording completeness, provenance or capture
lifecycle safety. Production MP3 capture remains disabled.

The native suite and full FFmpeg/FFprobe 9.0.2 integration corpus passed for
both the strict C++17 build and the UBSan build. The corpus covered 36 real
MPEG-1/2/2.5 mono/stereo CBR/VBR configurations, 420 reserved-selector
mutations, protected-frame PCM comparisons, and the existing reservoir, CRC,
Xing/VBRI, side-information and scalefactor checks. `git diff --check` passed.
AddressSanitizer and the repository's seeded sanitizer campaign were not rerun
for this follow-up; no CI/package or live-client acceptance is claimed.

CI and package verification follow-up for commit
310bc7d18a2aaf39193db5b58149f3e237b2a154: all four jobs passed in run 217
(44725198734), including native tests, both 20,000-iteration seeded ASan/UBSan
campaigns, and Windows release packaging. GitHub associated artifact
14625055959 with that exact commit. Independent verification passed outer and
inner ZIP integrity, all seven manifest hashes, required-content and helper
digest binding checks, AMD64 PE/import checks, and normalized source comparison
for all 27 packaged text/source files. The outer artifact is 3,309,781 bytes,
SHA-256 da8e88e574932dcad1e8458cd6a2b8b469cc2145f5c3e61d1a97620460829757; the
inner RC87 package is 3,311,717 bytes, SHA-256
4b31f1bedf69a42824540ca0b697f9c760f524b0c97f76bf735f8db465a416bb. The
reproducible verifier and result are in outputs/verify_run217.py and
outputs/Soggfy-run217-mp3-evidence.json. These checks establish CI/package
provenance for the offline inspector changes; they do not establish spectral
reconstruction, recording completeness, live-client acceptance, or production
capture readiness. PR #64 remains open and unmerged.
