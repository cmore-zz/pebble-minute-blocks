# Minute Blocks Store Listing

## Basic Info

- Title: Minute Blocks
- Developer: cmore
- Category: Faces
- Type: Watchface
- Version: 0.1.0
- Website URL:
- Source code URL:
- Support email:

## Platforms

- OG Pebble
- Steel
- Time/Time Steel
- Time Round
- 2
- 2 Duo
- Time 2
- Round 2

The PBW currently targets `aplite`, `basalt`, `chalk`, `diorite`, `emery`, `flint`, and `gabbro`.

## Short Description

A blocky digital watchface with minute-by-minute geometry, pixel text, and configurable corner complications.

## Description

Minute Blocks is a quiet digital watchface built around a custom block-number display. The center time uses a grid of square cells, while configurable corner complications can show date, battery, Bluetooth status, current temperature, forecast range, or steps.

The complication text uses the Visitor bitmap font for a crisp low-resolution look that matches Pebble displays. Complications can stay visible or hide until the watch is raised, tapped, or the backlight comes on.

Weather uses the phone's location permission and fetches current and forecast temperatures from Open-Meteo. No API key is required.

## Privacy Notes

- Location is used only for weather lookup.
- Weather requests are sent to Open-Meteo from PebbleKit JS on the phone.
- Step counts are read from Pebble Health on the watch.
- Settings are stored locally by the Pebble app.

## Attribution

Visitor is by Aenigma and was downloaded from DaFont. Confirm the final license terms before public release.

## Assets

### Screenshots

| Platform | Normal | Active complications | Event overlay |
| --- | --- | --- | --- |
| OG Pebble / Steel / Pebble 2 (`aplite`) | [`aplite-normal.png`](screenshots/aplite-normal.png) | [`aplite-active.png`](screenshots/aplite-active.png) | [`aplite-overlay.png`](screenshots/aplite-overlay.png) |
| Pebble Time / Time Steel (`basalt`) | [`basalt-normal.png`](screenshots/basalt-normal.png) | [`basalt-active.png`](screenshots/basalt-active.png) | [`basalt-overlay.png`](screenshots/basalt-overlay.png) |
| Pebble Time Round (`chalk`) | [`chalk-normal.png`](screenshots/chalk-normal.png) | [`chalk-active.png`](screenshots/chalk-active.png) | - |
| Pebble 2 SE / Pebble 2 Duo (`diorite`) | [`diorite-normal.png`](screenshots/diorite-normal.png) | [`diorite-active.png`](screenshots/diorite-active.png) | [`diorite-overlay.png`](screenshots/diorite-overlay.png) |
| Pebble Time 2 (`emery`) | [`emery-normal.png`](screenshots/emery-normal.png) | [`emery-active.png`](screenshots/emery-active.png) | [`emery-overlay.png`](screenshots/emery-overlay.png) |

Regenerate screenshots with:

```sh
just screenshots
```

### Icons

- Icon source crop: `store-assets/icons/minute-blocks-icon-source.png`
- Large list icon: `store-assets/icons/minute-blocks-list-144.png`
- Small list icon: `store-assets/icons/minute-blocks-list-80.png`
- PBW menu icon: `resources/images/menu_icon.png`

## Publish Checklist

- Upload `build/pebble-minute-blocks.pbw`.
- Upload at least one screenshot per supported platform collection.
- Upload large and small listing icons.
- Choose the Faces category.
- Add website, source, and support links if available.
- Verify `private` visibility before publishing publicly.
- Confirm Visitor font license is acceptable for app distribution.
- Increment `version` in `package.json` for each released PBW.
