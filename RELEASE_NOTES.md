# Soggfy v3.0.0-rc.37

RC37 is the next black-screen isolation build on top of RC36's concrete crash fix.

## Why this build exists

The RC35 dump proved the delayed crash itself was Soggfy's CEF discovery bug: browser-host slot 8 (`has_view()`) was accidentally called as `get_client()`, producing the bogus pointer `0x1`. RC36 fixes that correctly with slot 9 plus ABI/function-pointer validation.

That crash happened after the monitor had already gone black. RC35/RC36 also have no CEF MinHook detours left, which moves the black-screen investigation to the remaining MinHook activity in Spotify.dll.

## RC37 startup change

RC37 leaves the direct connectivity IAT patch early, because that operation only swaps an import pointer and does not suspend all process threads.

The two MinHook families are now kept well away from startup:

- playback-speed hooks: 10 seconds after the post-loader stage begins;
- audio/capture hooks: 11 seconds after the post-loader stage begins.

The post-loader stage itself still begins only after Spotify.dll's audited imports are resolved and the existing 2.5-second loader grace has elapsed. In normal startup this gives Spotify/Chromium roughly 12-14 seconds before any Spotify.dll MinHook detour is enabled.

This should make the result much more useful:

- if the screen stays normal through startup and only fails when the delayed native hooks activate, the culprit is narrowed to that hook family;
- if it blacks out before those delayed hooks activate, the cause is outside those MinHook detours.

RC35's hookless CEF bridge, RC36's corrected CEF discovery, RC29's accelerated capture/publication, RC30's status indicators and RC33's accelerated EOS-to-Next behavior are otherwise retained.
