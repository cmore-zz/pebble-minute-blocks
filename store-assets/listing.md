# Minute Blocks Store Listing

## Basic Info

- Title: Minute Blocks
- Developer: cmore
- Category: Faces
- Type: Watchface
- Version: 1.0.0
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

A blocky digital watchface with minute-by-minute geometry, pixel text, and configurable corner "complications" on rectangular Pebble watches, and centered complications on round Pebble watches (two on Pebble Time Round, four on the larger Pebble Round 2).

## Description

Minute Blocks in its default configuration is a quiet digital watchface built around a custom block-number display. The center time uses a grid of square cells, while configurable corner complications can show date, battery, Bluetooth status, current temperature, forecast range, or steps.

The complication text uses the Visitor bitmap font for a crisp low-resolution look that matches Pebble displays. Complications can stay visible or hide until the watch is raised, tapped, or the backlight comes on.

Weather uses the phone's location permission and fetches current and forecast temperatures from Open-Meteo. No API key is required.

Two optional extras can be switched on in settings (both off by default): a subtle seconds indicator that sweeps the ring, and a "Kinetic" mode that animates the blocks falling, smashing together, and cascading away at the top of the hour.

## Animations

Both features below are optional — enable them in the Pebble app settings.

A subtle seconds indicator can sweep the ring: set it to never show, reveal on
shake, tap, or backlight, or stay always on.

A "Kinetic" mode (off by default) adds playful motion to the blocks:
* a new block falls into place as each minute ticks (minutes 1–4 of every five)
* the four blocks stretch apart and then smash together as a five-minute marker completes
* all twelve blocks cascade off the bottom of the screen at the top of each hour

## Additional Customization

Watch face element colors can be customized, and (as mentioned above) "complication" slots can be filled with any of the available sources. (Currently options include temperature, temperature forecast, date, steps, etc.)

## Privacy Notes

- Location is used only for weather lookup.
- Weather requests are sent to Open-Meteo from PebbleKit JS on the phone.
- Step counts are read from Pebble Health on the watch.
- Settings are stored locally by the Pebble app.

## Attribution

The [Visitor](https://www.dafont.com/visitor.font) font is a Freeware font by Aenigma (Brian Kent) and was downloaded from DaFont.

## Assets

### Screenshots

| Platform | Normal | Active complications | Event overlay | Active overlay |
| --- | --- | --- | --- | --- |
| OG Pebble / Steel / Pebble 2 (`aplite`) | [`aplite-normal.png`](screenshots/aplite-normal.png) | [`aplite-active.png`](screenshots/aplite-active.png) | [`aplite-overlay.png`](screenshots/aplite-overlay.png) | [`aplite-overlay-active.png`](screenshots/aplite-overlay-active.png) |
| Pebble Time / Time Steel (`basalt`) | [`basalt-normal.png`](screenshots/basalt-normal.png) | [`basalt-active.png`](screenshots/basalt-active.png) | [`basalt-overlay.png`](screenshots/basalt-overlay.png) | [`basalt-overlay-active.png`](screenshots/basalt-overlay-active.png) |
| Pebble Time Round (`chalk`) | [`chalk-normal.png`](screenshots/chalk-normal.png) | [`chalk-active.png`](screenshots/chalk-active.png) | - | - |
| Pebble Round 2 (`gabbro`) | [`gabbro-normal.png`](screenshots/gabbro-normal.png) | [`gabbro-active.png`](screenshots/gabbro-active.png) | - | - |
| Pebble 2 SE (`diorite`) | [`diorite-normal.png`](screenshots/diorite-normal.png) | [`diorite-active.png`](screenshots/diorite-active.png) | [`diorite-overlay.png`](screenshots/diorite-overlay.png) | [`diorite-overlay-active.png`](screenshots/diorite-overlay-active.png) |
| Pebble 2 Duo (`flint`, same render as `diorite`) | [`diorite-normal.png`](screenshots/diorite-normal.png) | [`diorite-active.png`](screenshots/diorite-active.png) | [`diorite-overlay.png`](screenshots/diorite-overlay.png) | [`diorite-overlay-active.png`](screenshots/diorite-overlay-active.png) |
| Pebble Time 2 (`emery`) | [`emery-normal.png`](screenshots/emery-normal.png) | [`emery-active.png`](screenshots/emery-active.png) | [`emery-overlay.png`](screenshots/emery-overlay.png) | [`emery-overlay-active.png`](screenshots/emery-overlay-active.png) |

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
- Increment `version` in `package.json` for each released PBW.
