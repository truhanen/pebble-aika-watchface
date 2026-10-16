# Pebble appstore listing — Nyquist

Listing material for the Pebble appstore, kept under version control so it is
ready to upload. **Watchface** listing (lighter asset set than a watchapp).

## Text fields (this directory)

| File | Field |
|---|---|
| `title.txt` | Listing title |
| `description.txt` | Listing description (EN) |
| `source_url.txt` | Source code URL |
| `support_email.txt` | Support email |

Watchfaces need no category, but **do** take a large/small icon on the rePebble
appstore (the listing looks bare without one).

## Assets

- **Icons (committed here):** `icon-144.png` (large) + `icon-80.png` (small),
  the original Nyquist launcher icon redrawn at full size by
  `tools/make_store_icons.py` (`python3 -I tools/make_store_icons.py appstore`).
- **Banner** — optional for a watchface; none yet.
- **Screenshots** — ≥1 per supported platform, **unframed**, native resolution.
  Generated on the emulator with `scripts/screenshots.sh` (`make screenshots`):
  `screenshots/readme/01-overview.gif` plus the stills `02-little-widgets.png`,
  `03-many-widgets.png`, `04-leco.png` and `05-night.png` (frames of the GIF, 200x228).
  Version-controlled because the main README shows them too.

## Status

Drafted, awaiting manual upload to the Pebble appstore.
