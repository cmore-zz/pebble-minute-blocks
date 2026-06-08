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

## Build

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
- weather on/off
- weather units: Fahrenheit or Celsius

Weather uses PebbleKit JS on the phone with location permission and Open-Meteo.
It refreshes on startup and every 30 minutes.

## Minute Ring

- Future five-minute blocks: four small dots.
- Current block, minutes 1-4: one dot becomes a square each minute.
- Completed blocks: one larger box.
- The first block, minutes 1-5, sits at 1 o'clock; minutes 6-10 sit at 2
  o'clock, and so on.
