#!/usr/bin/env python3
"""Capture a kinetic animation from a Pebble emulator as a GIF.

Jumps the emulator clock to just before an animation's trigger, enables Kinetic
mode, burst-grabs screenshots across the window, and writes a looping GIF whose
per-frame durations match real time.

Run it with the Pebble SDK's Python (it needs libpebble2 / pebble_tool), e.g.:

    PEBBLE_PYTHON=~/.local/share/uv/tools/pebble-tool/bin/python3
    $PEBBLE_PYTHON scripts/capture-animation.py --at 10:44:57 --duration 3.5 \
        --out /tmp/smash.gif

Trigger windows (each --at is a few seconds before the boundary):
  * Falling block  : any minute roll; bottom-center is longest/clearest.
                     --at 10:25:56 --duration 5.0   (block falls by 10:26:00)
  * Smash          : a minute ending in 4 or 9 (marker completes next minute).
                     --at 10:44:57 --duration 3.5   (smash into 10:45:00)
  * Smash + cascade: top of the hour (the :59 marker smashes, then all cascade).
                     --at 11:59:57 --duration 4.5   (cascade after 12:00:00)

Gotchas (learned the hard way):
  * The emulator renders kinetic at only ~10 fps, so sub-second clips (the
    falling block) come out choppy; the smash/cascade are long enough to read.
    For listing-quality smoothness, record real hardware instead.
  * PT2 (emery) and Round 2 (gabbro) emulators won't accept a clock-set, so this
    only works on basalt/chalk/aplite/diorite.
  * If the emulator wedges (captures go static / one distinct frame), `pebble
    wipe` and reinstall, then retry.
  * This writes full (un-optimized) frames on purpose. Do NOT post-process with
    `magick -layers optimize` -- its frame differencing leaves a pixel trail of
    the block's previous positions. Use `magick -coalesce` if you must re-encode.
  * To tighten a clip to just the motion, coalesce and rebuild from the frames
    that matter, e.g.:
        magick in.gif -coalesce f_%03d.png
        magick -loop 0 -delay 9 f_003.png ... f_010.png -delay 90 f_010.png out.gif
"""
import argparse
import datetime
import io
import time
import uuid

import png
from PIL import Image

from libpebble2.communication import PebbleConnection
from libpebble2.protocol.system import SetUTC, TimeMessage
from libpebble2.services.appmessage import AppMessageService, Int32
from pebble_tool.commands.screenshot import ScreenshotCommand
from pebble_tool.sdk import sdk_manager
from pebble_tool.sdk.emulator import ManagedEmulatorTransport

APP_UUID = uuid.UUID("4e10c644-2684-4604-aaf3-be90703b8d95")
KEY_KINETIC_ENABLED = 10019


def set_time(p, hh, mm, ss):
  now = datetime.datetime.now().replace(hour=hh, minute=mm, second=ss, microsecond=0)
  ts = int(now.timestamp())
  off = -time.altzone if time.localtime(ts).tm_isdst and time.daylight else -time.timezone
  tzm = off // 60
  p.send_packet(TimeMessage(message=SetUTC(unix_time=ts, utc_offset=tzm,
                                           tz_name="UTC%+d" % (tzm // 60))))


def grab(p):
  cmd = ScreenshotCommand()
  cmd.pebble = p
  rows = list(cmd._grab_processed_image(argparse.Namespace(no_correction=False, scale=1)))
  buf = io.BytesIO()
  png.from_array(rows, mode="RGBA;8").write(buf)
  buf.seek(0)
  return Image.open(buf).convert("RGB")


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--emulator", default="basalt")
  ap.add_argument("--at", required=True, help="HH:MM:SS to jump to before capturing")
  ap.add_argument("--duration", type=float, default=4.0)
  ap.add_argument("--out", required=True)
  ap.add_argument("--scale", type=int, default=2)
  ap.add_argument("--no-kinetic", action="store_true", help="skip enabling Kinetic mode")
  args = ap.parse_args()
  hh, mm, ss = (int(x) for x in args.at.split(":"))

  transport = ManagedEmulatorTransport(args.emulator, sdk_manager.get_current_sdk(), False)
  p = PebbleConnection(transport)
  p.connect()
  p.run_async()
  try:
    if not args.no_kinetic:
      AppMessageService(p).send_message(APP_UUID, {KEY_KINETIC_ENABLED: Int32(1)})
      time.sleep(0.5)
    set_time(p, hh, mm, ss)
    time.sleep(0.2)
    set_time(p, hh, mm, ss)

    frames, stamps = [], []
    start = time.time()
    while time.time() - start < args.duration:
      frames.append(grab(p))
      stamps.append(time.time())
    durs = [max(40, int((stamps[i + 1] - stamps[i]) * 1000)) if i + 1 < len(stamps) else 700
            for i in range(len(stamps))]
    if args.scale != 1:
      frames = [f.resize((f.width * args.scale, f.height * args.scale), Image.NEAREST)
                for f in frames]
    frames[0].save(args.out, save_all=True, append_images=frames[1:],
                   duration=durs, loop=0, optimize=False)
    print("wrote %s (%d frames over %.2fs)" % (args.out, len(frames), stamps[-1] - start))
  finally:
    ws = getattr(getattr(p, "transport", None), "ws", None)
    if ws is not None:
      try:
        ws.close()
      except Exception:
        pass


if __name__ == "__main__":
  main()
