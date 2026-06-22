set dotenv-load

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
