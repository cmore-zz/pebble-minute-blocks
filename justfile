set dotenv-load

default: build

build:
    unset CC CXX && pebble build

clean:
    unset CC CXX && pebble clean

rebuild:
    unset CC CXX && pebble clean
    unset CC CXX && pebble build

build-variant variant="minute-blocks":
    ./scripts/build-variant.py {{variant}}

build-kinetic:
    ./scripts/build-variant.py kinetic

install *args:
    unset CC CXX && pebble build
    pebble install build/pebble-minute-blocks.pbw {{args}}

install-kinetic *args:
    just build-kinetic
    pebble install build/pebble-minute-blocks-kinetic.pbw {{args}}

screenshots *args:
    ./scripts/capture-store-screenshots.sh {{args}}
