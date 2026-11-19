#!/usr/bin/env python3
import argparse
import datetime
import signal
import tempfile
import time
import uuid

import png

from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.qemu.protocol import QemuBattery, QemuTap, QemuTimelinePeek
from libpebble2.protocol.system import SetUTC, TimeMessage
from libpebble2.services.appmessage import AppMessageService, Int32
from pebble_tool.commands.emucontrol import send_data_to_qemu
from pebble_tool.commands.screenshot import ScreenshotCommand
from pebble_tool.sdk import sdk_manager
from pebble_tool.sdk.emulator import ManagedEmulatorTransport


def parse_time(value):
  hour, minute, second = (int(part) for part in value.split(":"))
  now = datetime.datetime.now()
  return now.replace(hour=hour, minute=minute, second=second, microsecond=0)


def set_time(pebble, value):
  target = parse_time(value)
  ts = int(target.timestamp())
  tz_offset = -time.altzone if time.localtime(ts).tm_isdst and time.daylight else -time.timezone
  tz_offset_minutes = tz_offset // 60
  tz_name = "UTC%+d" % (tz_offset_minutes // 60)
  pebble.send_packet(TimeMessage(message=SetUTC(
    unix_time=ts,
    utc_offset=tz_offset_minutes,
    tz_name=tz_name,
  )))


def settle_time(pebble, value, battery_percent, settle_seconds):
  set_time(pebble, value)
  time.sleep(0.35)
  set_time(pebble, value)
  send_data_to_qemu(pebble.transport, QemuBattery(percent=battery_percent, charging=False))
  time.sleep(settle_seconds)


def capture(pebble, filename):
  args = argparse.Namespace(no_correction=False, scale=1)
  command = ScreenshotCommand()
  command.pebble = pebble
  if pebble.watch_platform == "aplite":
    # Read the legacy monochrome framebuffer directly, without allocating
    # a screenshot buffer on the watch.
    with tempfile.TemporaryDirectory(prefix="minute-blocks-still-") as directory:
      frame = command._grab_qemu_monitor_image_fast(pebble.transport.qemu_monitor_port, directory, 0)
      left, top = (frame.width - 144) // 2, (frame.height - 168) // 2
      frame = frame.crop((left, top, left + 144, top + 168)).convert("L")
      peak = frame.getextrema()[1]
      frame.point(lambda value: 255 if value > peak / 2 else 0).convert("RGBA").save(filename)
    print("Saved screenshot to {}".format(filename))
    return
  image = command._grab_processed_image(args)
  png.from_array(image, mode="RGBA;8").save(filename)
  print("Saved screenshot to {}".format(filename))


def close_pebble(pebble):
  ws = getattr(getattr(pebble, "transport", None), "ws", None)
  if ws is None:
    return

  try:
    ws.close()
  except Exception:
    pass


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--emulator", required=True)
  parser.add_argument("--sdk")
  parser.add_argument("--time", default="10:11:00")
  parser.add_argument("--battery-percent", type=int, default=80)
  parser.add_argument("--tap", action="store_true")
  parser.add_argument("--overlay", action="store_true")
  parser.add_argument("--block-size", choices=("normal", "large"), default="normal")
  parser.add_argument("--no-complications", action="store_true")
  parser.add_argument("--keep-emulator", action="store_true", help="leave running for a batch capture")
  parser.add_argument("--settle-seconds", type=float, default=1)
  parser.add_argument("filename")
  args = parser.parse_args()

  sdk = args.sdk or sdk_manager.get_current_sdk()
  transport = ManagedEmulatorTransport(args.emulator, sdk, False)
  pebble = PebbleConnection(transport)
  def interrupted(signum, frame):
    raise KeyboardInterrupt
  signal.signal(signal.SIGTERM, interrupted)

  try:
    pebble.connect()
    pebble.run_async()
    # Reset the capture's display settings so prior emulator sessions cannot
    # leave Kinetic, Large, or always-visible complications enabled.
    settings = {10019: Int32(0), 10020: Int32(args.block_size == "large"),
                10013: Int32(1), 10018: Int32(0 if args.no_complications else 2)}
    if args.emulator == "aplite" and args.tap:
      # Reveal directly for stills: the legacy simulator's tap/backlight path
      # can reset the firmware while the clock is being staged.
      settings[10013] = Int32(0)
      settings[10018] = Int32(0)
    if args.no_complications:
      settings.update({key: Int32(0) for key in range(10014, 10018)})
    else:
      settings.update({10014: Int32(1), 10015: Int32(2),
                       10016: Int32(4), 10017: Int32(5)})
    AppMessageService(pebble).send_message(uuid.UUID("4e10c644-2684-4604-aaf3-be90703b8d95"), settings)
    time.sleep(0.5)
    # A previous active view remains revealed for seven seconds. Let it expire
    # before capturing a hidden-complication view in the same emulator session.
    if not args.tap and not args.no_complications:
      time.sleep(7.1)
    if args.overlay:
      send_data_to_qemu(pebble.transport, QemuTimelinePeek(enabled=True))

    settle_time(pebble, args.time, args.battery_percent, args.settle_seconds)
    if args.tap and args.emulator != "aplite":
      send_data_to_qemu(pebble.transport, QemuTap(axis=QemuTap.Axis.Z, direction=1))
      time.sleep(args.settle_seconds)

    capture(pebble, args.filename)

    if args.overlay:
      send_data_to_qemu(pebble.transport, QemuTimelinePeek(enabled=False))
  finally:
    close_pebble(pebble)
    if not args.keep_emulator:
      ScreenshotCommand._shutdown_platform_emulator(args.emulator, sdk)


if __name__ == "__main__":
  main()
