#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="store-assets/screenshots"
PBW="build/pebble-minute-blocks.pbw"
PEBBLE_PYTHON="${PEBBLE_PYTHON:-/Users/cmore/.local/share/uv/tools/pebble-tool/bin/python3}"
PLATFORMS=(basalt chalk emery aplite diorite flint gabbro)
RETRIES="${RETRIES:-3}"
RETRY_SLEEP="${RETRY_SLEEP:-3}"
SCREENSHOT_TIME="${SCREENSHOT_TIME:-10:11:00}"
SCREENSHOT_BATTERY_PERCENT="${SCREENSHOT_BATTERY_PERCENT:-80}"
INSTALL_SETTLE_SECONDS="${INSTALL_SETTLE_SECONDS:-5}"
SCREENSHOT_SETTLE_SECONDS="${SCREENSHOT_SETTLE_SECONDS:-1}"
RECOVERY_SETTLE_SECONDS="${RECOVERY_SETTLE_SECONDS:-5}"

usage() {
  cat <<'EOF'
Usage: scripts/capture-store-screenshots.sh [--out DIR] [platform...]

Platforms: basalt chalk emery aplite diorite flint gabbro

Examples:
  scripts/capture-store-screenshots.sh
  scripts/capture-store-screenshots.sh basalt
  scripts/capture-store-screenshots.sh --out /tmp/screens basalt chalk
EOF
}

SELECTED_PLATFORMS=()
while (($#)); do
  case "$1" in
    --out)
      if (($# < 2)); then
        echo "--out requires a directory" >&2
        exit 2
      fi
      OUT_DIR="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      SELECTED_PLATFORMS+=("$1")
      shift
      ;;
  esac
done

if ((${#SELECTED_PLATFORMS[@]})); then
  PLATFORMS=("${SELECTED_PLATFORMS[@]}")
  for platform in "${PLATFORMS[@]}"; do
    case "$platform" in
      basalt|emery|aplite|diorite|flint)
        ;;
      chalk|gabbro)
        ;;
      *)
        echo "Unknown platform: ${platform}" >&2
        usage >&2
        exit 2
        ;;
    esac
  done
fi

mkdir -p "$OUT_DIR"

# Keep only the platform currently being captured alive, including on failure.
current_platform=""
cleanup() {
  if [[ -n "$current_platform" ]]; then
    "$PEBBLE_PYTHON" -c 'import sys; from pebble_tool.commands.screenshot import ScreenshotCommand; ScreenshotCommand._shutdown_platform_emulator(sys.argv[1], None)' "$current_platform"
    current_platform=""
  fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

just build

retry() {
  local attempt=1

  while true; do
    if "$@"; then
      return 0
    fi

    if (( attempt >= RETRIES )); then
      echo "Command failed after ${RETRIES} attempts: $*" >&2
      return 1
    fi

    echo "Command failed, retrying in ${RETRY_SLEEP}s (${attempt}/${RETRIES}): $*" >&2
    sleep "$RETRY_SLEEP"
    attempt=$((attempt + 1))
  done
}

install_platform() {
  local platform="$1"

  retry pebble install "$PBW" --emulator "$platform"
  set_platform_time_format "$platform"
  sleep "$INSTALL_SETTLE_SECONDS"
}

set_platform_time_format() {
  local platform="$1"

  if [[ "$platform" == "emery" ]]; then
    retry pebble emu-time-format --emulator "$platform" --format 12h
  fi
}

recover_platform() {
  local platform="$1"

  echo "==> ${platform}: wipe and reinstall after failure" >&2
  pebble wipe || true
  if ! retry pebble install "$PBW" --emulator "$platform"; then
    echo "Recovery install failed for ${platform}" >&2
    return 1
  fi
  set_platform_time_format "$platform"
  sleep "$RECOVERY_SETTLE_SECONDS"
}

retry_capture() {
  local platform="$1"
  shift
  local attempt=1

  while true; do
    if "$@"; then
      return 0
    fi

    if (( attempt >= RETRIES )); then
      echo "Command failed after ${RETRIES} attempts: $*" >&2
      return 1
    fi

    echo "Command failed, recovering ${platform} before retry (${attempt}/${RETRIES}): $*" >&2
    if ! recover_platform "$platform"; then
      echo "Recovery failed for ${platform}; retrying command anyway" >&2
    fi
    sleep "$RETRY_SLEEP"
    attempt=$((attempt + 1))
  done
}

capture_screenshot() {
  local platform="$1"
  local mode="$2"
  local output="$3"
  # Legacy aplite is unstable across successive captures. Use a fresh emulator
  # for each view; the other platforms share one emulator for their whole batch.
  if [[ "$platform" == "aplite" && "$mode" != "normal" ]]; then
    cleanup
    current_platform="$platform"
    install_platform "$platform"
  fi
  local args=(
    scripts/capture-one-screenshot.py
    --emulator "$platform"
    --keep-emulator
    --time "$SCREENSHOT_TIME"
    --battery-percent "$SCREENSHOT_BATTERY_PERCENT"
    --settle-seconds "$SCREENSHOT_SETTLE_SECONDS"
  )

  if [[ "$mode" == "active" ]]; then
    args+=(--tap)
  elif [[ "$mode" == "overlay" ]]; then
    args+=(--overlay)
  elif [[ "$mode" == "overlay-active" ]]; then
    args+=(--overlay --tap)
  elif [[ "$mode" == "blocks-normal" ]]; then
    args+=(--block-size normal --no-complications)
  elif [[ "$mode" == "blocks-large" ]]; then
    args+=(--block-size large --no-complications)
  fi

  args+=("$output")
  retry_capture "$platform" "$PEBBLE_PYTHON" "${args[@]}"
}

for platform in "${PLATFORMS[@]}"; do
  current_platform="$platform"
  echo "==> ${platform}: install"
  install_platform "$platform"

  echo "==> ${platform}: normal screenshot"
  capture_screenshot "$platform" normal "$OUT_DIR/${platform}-normal.png"

  echo "==> ${platform}: active screenshot"
  capture_screenshot "$platform" active "$OUT_DIR/${platform}-active.png"

  if [[ "$platform" == "emery" ]]; then
    capture_screenshot "$platform" blocks-normal "$OUT_DIR/emery-blocks-normal.png"
    capture_screenshot "$platform" blocks-large "$OUT_DIR/emery-blocks-large.png"
  fi

  if [[ "$platform" != "chalk" && "$platform" != "gabbro" ]]; then
    echo "==> ${platform}: overlay screenshot"
    capture_screenshot "$platform" overlay "$OUT_DIR/${platform}-overlay.png"

    echo "==> ${platform}: overlay active screenshot"
    capture_screenshot "$platform" overlay-active "$OUT_DIR/${platform}-overlay-active.png"
  fi
  cleanup
done

echo "Screenshots written to $OUT_DIR"
