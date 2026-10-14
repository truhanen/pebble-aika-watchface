# Pebble appstore listing — Sykerö TimeStyle

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
  from the kellotaulu artwork (Einstein holding a light-green clock) — matches the
  Sykerö Track Work Time / MIDI Recorder family.
- **Banner** — optional for a watchface. `project_banner.png` exists in the repo
  root.
- **Screenshots** — ≥1 per supported platform, **unframed**, native resolution.
  Generated on the emulator with `scripts/pebble-appstore-screenshots.sh`; output
  is gitignored (regenerable).

## Status

Drafted, awaiting manual upload to the Pebble appstore.
