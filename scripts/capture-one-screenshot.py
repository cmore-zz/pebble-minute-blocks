#!/usr/bin/env python3
import argparse
import datetime
import os
import time

import png

from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.qemu.protocol import QemuBattery, QemuTap, QemuTimelinePeek
from libpebble2.protocol.system import SetUTC, TimeMessage
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
  parser.add_argument("--settle-seconds", type=float, default=1)
  parser.add_argument("filename")
  args = parser.parse_args()

  sdk = args.sdk or sdk_manager.get_current_sdk()
  transport = ManagedEmulatorTransport(args.emulator, sdk, False)
  pebble = PebbleConnection(transport)
  pebble.connect()
  pebble.run_async()

  try:
    if args.overlay:
      send_data_to_qemu(pebble.transport, QemuTimelinePeek(enabled=True))

    if args.tap:
      send_data_to_qemu(pebble.transport, QemuTap(axis=QemuTap.Axis.Z, direction=1))

    settle_time(pebble, args.time, args.battery_percent, args.settle_seconds)
    capture(pebble, args.filename)

    if args.overlay:
      send_data_to_qemu(pebble.transport, QemuTimelinePeek(enabled=False))
  finally:
    close_pebble(pebble)


if __name__ == "__main__":
  main()
