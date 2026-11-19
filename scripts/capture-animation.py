#!/usr/bin/env python3
"""Render a kinetic GIF at exact millisecond times, independent of simulator speed.

Builds an isolated temporary PBW with MINUTE_BLOCKS_CAPTURE. Hidden keys 20000
and 20001 select the displayed second and millisecond; real ticks and animation
timers are disabled while this override is active. The production PBW is untouched.
Each frame is acknowledged, allowed to finish rendering, and read from the watch
framebuffer before advancing the clock. Slow captures cannot drop motion frames.

Examples (run with the Pebble SDK's Python):
  scripts/capture-animation.py --at 10:25:57 --duration 4 --scale 1 --out /tmp/fall.gif
  scripts/capture-animation.py --emulator emery --large --no-complications \\
      --at 11:59:57 --duration 5.5 --scale 1 --out /tmp/cascade.gif

The emulator is shut down on completion, failure, or interruption.
"""
import argparse
import datetime
import os
from pathlib import Path
import queue
import shutil
import signal
import subprocess
import tempfile
import time
import uuid

from PIL import Image
from libpebble2.communication import PebbleConnection
from libpebble2.services.appmessage import AppMessageService, Int32
from pebble_tool.commands.screenshot import ScreenshotCommand
from pebble_tool.sdk import sdk_manager
from pebble_tool.sdk.emulator import ManagedEmulatorTransport

APP_UUID = uuid.UUID("4e10c644-2684-4604-aaf3-be90703b8d95")
CAPTURE_SECONDS_KEY = 20000
CAPTURE_MILLISECONDS_KEY = 20001


def build_capture_pbw(directory, platform):
  """Compile the current source in isolation, with private capture controls."""
  import json
  root = Path(__file__).resolve().parent.parent
  for name in ("src", "resources"):
    shutil.copytree(root / name, directory / name)
  shutil.copy2(root / "wscript", directory / "wscript")
  manifest = json.loads((root / "package.json").read_text())
  manifest["pebble"]["targetPlatforms"] = [platform]
  (directory / "package.json").write_text(json.dumps(manifest))
  env = dict(os.environ, MINUTE_BLOCKS_CAPTURE="1")
  env.pop("CC", None)
  env.pop("CXX", None)
  result = subprocess.run(["pebble", "build"], cwd=directory, env=env,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
  if result.returncode:
    raise RuntimeError("Capture build failed:\n" + result.stdout)
  return next((directory / "build").glob("*.pbw"))



def trim_static_padding(frames, frame_ms, lead_ms=200, tail_ms=700):
  """Keep a short lead-in and final hold without altering motion or wind-up."""
  first = frames[0].tobytes()
  first_change = next((i for i, frame in enumerate(frames) if frame.tobytes() != first), None)
  if first_change is None:
    return frames[:1], [max(frame_ms, lead_ms + tail_ms)]
  last = frames[-1].tobytes()
  last_change = next(i for i in range(len(frames) - 1, -1, -1)
                     if frames[i].tobytes() != last)
  start = max(0, first_change - (lead_ms + frame_ms - 1) // frame_ms)
  result = frames[start:min(len(frames), last_change + 2)]
  durations = [frame_ms] * len(result)
  durations[-1] = max(frame_ms, tail_ms)
  return result, durations


def main():
  ap = argparse.ArgumentParser(description=__doc__)
  ap.add_argument("--emulator", default="basalt",
                  choices=("aplite", "basalt", "chalk", "diorite", "emery", "flint", "gabbro"))
  ap.add_argument("--at", required=True, help="first rendered time, HH:MM:SS")
  ap.add_argument("--duration", type=float, default=5.5)
  ap.add_argument("--fps", type=int, choices=(20, 25, 50), default=50,
                  help="output GIF rate; each frame is rendered separately")
  ap.add_argument("--frame-settle-seconds", type=float, default=0.08,
                  help="wait after each acknowledged clock override before reading pixels")
  ap.add_argument("--lead-in-ms", type=int, default=200, help="trim idle lead-in to this duration")
  ap.add_argument("--tail-ms", type=int, default=700, help="final static hold")
  ap.add_argument("--out", required=True)
  ap.add_argument("--scale", type=int, default=2)
  ap.add_argument("--large", action="store_true")
  ap.add_argument("--no-complications", action="store_true")
  ap.add_argument("--no-kinetic", action="store_true")
  args = ap.parse_args()
  if args.duration <= 0 or args.scale < 1 or args.frame_settle_seconds < 0 or args.lead_in_ms < 0 or args.tail_ms < 20:
    ap.error("duration and scale must be positive; frame settle must be nonnegative")
  try:
    hour, minute, second = (int(part) for part in args.at.split(":"))
    target = datetime.datetime.now(datetime.timezone.utc).replace(
      hour=hour, minute=minute, second=second, microsecond=0)
  except ValueError:
    ap.error("--at must be a valid HH:MM:SS")
  def interrupted(signum, frame):
    raise KeyboardInterrupt
  signal.signal(signal.SIGTERM, interrupted)

  sdk = sdk_manager.get_current_sdk()
  pebble = None
  try:
    with tempfile.TemporaryDirectory(prefix="minute-blocks-capture-") as directory:
      pbw = build_capture_pbw(Path(directory), args.emulator)
      subprocess.run(["pebble", "install", str(pbw), "--emulator", args.emulator], check=True)
      pebble = PebbleConnection(ManagedEmulatorTransport(args.emulator, sdk, False))
      pebble.connect()
      pebble.run_async()
      # Allow phone-side startup settings and the launch animation to finish.
      time.sleep(5)
      service = AppMessageService(pebble)
      replies = queue.Queue()
      service.register_handler("ack", lambda tid, *rest: replies.put((tid, True)))
      service.register_handler("nack", lambda tid, *rest: replies.put((tid, False)))
      def send(values):
        tid = service.send_message(APP_UUID, {key: Int32(value) for key, value in values.items()})
        deadline = time.monotonic() + 5
        while True:
          received, accepted = replies.get(timeout=max(0, deadline - time.monotonic()))
          if received == tid:
            if not accepted:
              raise RuntimeError("Watch rejected the capture frame")
            return
      settings = {10019: int(not args.no_kinetic), 10020: int(args.large),
                  10018: 0, 10013: 1, 10006: 0}
      if args.no_complications:
        settings.update({key: 0 for key in range(10014, 10018)})
      send(settings)
      command = ScreenshotCommand()
      command.pebble = pebble
      screenshot_args = argparse.Namespace(no_correction=False, scale=1)
      frames = []
      frame_count = max(1, round(args.duration * args.fps))
      frame_ms = 1000 // args.fps
      base_ms = int(target.timestamp()) * 1000
      print("Rendering %d frames, advancing the hidden clock by %d ms per frame" %
            (frame_count, frame_ms), flush=True)
      for index in range(frame_count):
        seconds, milliseconds = divmod(base_ms + index * frame_ms, 1000)
        send({CAPTURE_SECONDS_KEY: seconds, CAPTURE_MILLISECONDS_KEY: milliseconds})
        time.sleep(args.frame_settle_seconds)
        rows = list(command._grab_processed_image(screenshot_args, show_progress=False))
        frame = Image.frombytes("RGBA", (len(rows[0]) // 4, len(rows)),
                                bytes(value for row in rows for value in row))
        if args.scale != 1:
          frame = frame.resize((frame.width * args.scale, frame.height * args.scale), Image.Resampling.NEAREST)
        frames.append(frame)
        if (index + 1) % args.fps == 0:
          print("Rendered %d/%d" % (index + 1, frame_count), flush=True)
      frames, durations = trim_static_padding(frames, frame_ms, args.lead_in_ms, args.tail_ms)
      frames[0].save(args.out, save_all=True, append_images=frames[1:],
                     duration=durations, loop=0, optimize=False)
      print("Wrote %s (%d frames at %d fps)" % (args.out, len(frames), args.fps))
  finally:
    try:
      if pebble is not None:
        ws = getattr(pebble.transport, "ws", None)
        if ws is not None:
          ws.close()
    finally:
      ScreenshotCommand._shutdown_platform_emulator(args.emulator, sdk)


if __name__ == "__main__":
  main()
