# Box Minutes

A Pebble C watchface for the Pebble Time 2 idea: twelve five-minute markers
around the edge, each starting as four tiny dots. As the current five-minute
block advances, the dots become square pixels one at a time; when the block is
complete, it turns into a larger box.

The center uses a chunky pixel-style hour readout. Minutes live only in the
outer ring, with small date, battery, and Bluetooth status complications around
the main hour.

## Build

```sh
pebble build
```

## Install

For the Pebble Time / Time Steel emulator:

```sh
pebble install --emulator basalt
```

For a physical watch through the Pebble/Rebble app developer connection:

```sh
pebble install --phone PHONE_IP
```

## Minute Ring

- Future five-minute blocks: four small dots.
- Current block, minutes 1-4: one dot becomes a square each minute.
- Completed blocks: one larger box.
