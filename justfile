set dotenv-load

PEBBLE_PYTHON := env_var_or_default("PEBBLE_PYTHON", "/Users/cmore/.local/share/uv/tools/pebble-tool/bin/python3")

default: build

build:
    unset CC CXX && pebble build

clean:
    unset CC CXX && pebble clean

rebuild:
    unset CC CXX && pebble clean
    unset CC CXX && pebble build

install *args:
    unset CC CXX && pebble build
    pebble install build/pebble-minute-blocks.pbw {{args}}

screenshots *args:
    ./scripts/capture-store-screenshots.sh {{args}}

# Capture a kinetic animation GIF. Install the build on the emulator first, e.g.
#   just install --emulator basalt
#   just capture-animation --at 10:44:57 --duration 3.5 --out /tmp/smash.gif
# See scripts/capture-animation.py for trigger windows and gotchas.
capture-animation *args:
    {{PEBBLE_PYTHON}} scripts/capture-animation.py {{args}}
