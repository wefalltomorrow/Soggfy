# Soggfy v3.0.0-rc.39

RC39 keeps the clean Spotify 1.3.1.234 rollback from RC38 and restores the original Soggfy separation of responsibilities around SpotX.

## Soggfy now blocks telemetry only

Soggfy's native CEF request filter no longer tries to be an ad blocker.

The built-in `Block Telemetry` option now blocks only:

- `gabo-receiver-service`
- `dodo-receiver-service`

Requests under Spotify's `/ads/` and `/ad-logic/` paths are deliberately allowed through Soggfy. Spotify client-update traffic is also left alone by the native Soggfy filter.

## SpotX handles ads and updates

The installer now offers SpotX by default, similar to original Soggfy.

When enabled, RC39 runs the current official SpotX installer with:

- Spotify version pinned to `1.3.1.234.g59d6bf59`
- SpotX's normal ad blocking enabled
- `-block_update_on` forced so Spotify does not replace the Soggfy-compatible client
- the actual Spotify install path passed explicitly

The installer runs SpotX before copying Soggfy's `version.dll`, avoiding SpotX touching an already-installed Soggfy proxy.

Interactive installs ask:

`Install/update SpotX for ad blocking and Spotify update blocking? [Y/n]`

Pressing Enter selects Yes.

For unattended installs:

- `-RunSpotX` forces SpotX on
- `-SkipSpotX` explicitly skips it

If SpotX is skipped, Soggfy still provides telemetry blocking, but it does not block ads or Spotify updates itself.

## Spotify baseline

RC39 still targets Spotify Windows x64 `1.3.1.234.g59d6bf59`. The installer checks the installed client version before applying SpotX/Soggfy so an unsupported Spotify build is not silently patched.

All RC15-RC37 Spotify 1.3.3 experimentation remains removed.
