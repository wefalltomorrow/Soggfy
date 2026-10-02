# Soggfy v3.0.0-rc.8

RC8 makes Skip Downloaded Tracks safe to use with libraries created by older Soggfy installs.

## Legacy library detection

Soggfy still checks the configured Track Template first, but downloaded-file lookup is now format-independent. An existing MP3, M4A/MP4, Ogg, Opus, FLAC, AAC or WAV can therefore be recognized even when the current output setting is different.

For older flat libraries, Soggfy also recognizes exact legacy filenames such as:

    Technotronic - Pump Up The Jam.mp3
    A$AP Rocky, Rod Stewart, Miguel, Mark Ronson - Everyday.mp3

The fallback requires both artist and track title. It deliberately does not use title-only matching.

Old invalid-character modes and the historical `{all_artist_names}` slash-to-comma behavior are covered as well.

If more than one file matches the same track, Soggfy reports an ambiguous warning instead of silently skipping it.

All RC7 settings-button fixes and the RC6 Classic UI/capture functionality remain included.
