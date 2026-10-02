# Soggfy v3.0.0-rc.7

RC7 fixes the Classic Soggfy settings button introduced in RC6.

## Fixed

- The sliders/settings control in Spotify's top bar now uses an independent Soggfy button instead of borrowing Spotify's navigation-button class.
- Click handling is isolated from Spotify's surrounding React/top-bar event handling.
- The retractable Downloads/Settings control keeps a stable flex layout while expanding.
- The settings modal is mounted directly into the document instead of through an extra wrapper.
- The modal is kept above Spotify's UI layers and now receives focus when opened.
- Escape closes the settings modal.

All RC6 Classic UI behaviour and the Floggfy RC5-derived x64 capture backend remain unchanged.
