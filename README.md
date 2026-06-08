# Pebble Minute Blocks

A Pebble C watchface with twelve five-minute markers around the edge, each
starting as four tiny dots. As the current five-minute block advances, the dots
become square pixels one at a time; when the block is complete, it turns into a
larger box.

The center uses a chunky pixel-style hour readout. Minutes live only in the
outer ring, with small date, weather, battery, and Bluetooth status
complications around the main hour.

The face has separate sizing for compact Pebble Time-era screens and the larger
Pebble Time 2 / `emery` screen.

The complication text uses the Visitor bitmap font by Ænigma, distributed via
DaFont as 100% free. It is bundled at 15, 20, and 25 px sizes so the small
status labels and numbers stay crisp on low-resolution Pebble screens.

## Build

The Pebble SDK needs its bundled ARM toolchain, so the included `justfile`
clears host compiler overrides such as `CC=/opt/homebrew/bin/gcc-15` before
building:

```sh
just build
```

You can still call the Pebble SDK directly if your shell does not override the
C compiler:

```sh
pebble build
```

## Install

For the Pebble Time / Time Steel emulator, use the compact `basalt` target:

```sh
pebble install --emulator basalt
```

For the Pebble Time 2 emulator, use the larger `emery` target:

```sh
pebble install --emulator emery
```

For a physical watch through the Pebble/Rebble app developer connection:

```sh
pebble install --phone PHONE_IP
```

## Settings

The Pebble app settings page supports:

- background color
- minute ring color
- complication color
- center digit color
- time mode: watch setting, 12-hour, or 24-hour
- complication size: normal, medium, or large
- complication visibility: always shown, or hidden until a tap/shake/backlight
- each corner complication: none, date, current temperature, forecast range,
  battery, Bluetooth, or steps
- weather on/off
- weather units: Fahrenheit or Celsius

Weather uses PebbleKit JS on the phone with location permission and Open-Meteo.
It refreshes current temperature and the day's forecast range on startup and
every 30 minutes.

## Store Assets

Store listing copy, screenshots, and listing icons live in `store-assets/`.
Those files are intended for appstore/developer portal forms; the PBW itself
only embeds the small `resources/images/menu_icon.png` menu icon.

## Minute Ring

- Future five-minute blocks: four small dots.
- Current block, minutes 1-4: one dot becomes a square each minute.
- Completed blocks: one larger box.
- The first block, minutes 1-5, sits at 1 o'clock; minutes 6-10 sit at 2
  o'clock, and so on.
