#!/usr/bin/env python3
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def deep_merge(base, override):
  result = dict(base)
  for key, value in override.items():
    if isinstance(value, dict) and isinstance(result.get(key), dict):
      result[key] = deep_merge(result[key], value)
    else:
      result[key] = value
  return result


def copy_project(source, target):
  ignored = shutil.ignore_patterns(
    ".git",
    "build",
    "store-assets",
    ".DS_Store",
    ".pycache-check",
    "__pycache__",
  )
  shutil.copytree(source, target, ignore=ignored)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("variant", nargs="?", default="minute-blocks")
  args = parser.parse_args()

  repo = Path(__file__).resolve().parents[1]
  variant_path = repo / "variants" / "{}.json".format(args.variant)
  if not variant_path.exists():
    print("Unknown variant: {}".format(args.variant), file=sys.stderr)
    print("Expected {}".format(variant_path), file=sys.stderr)
    return 2

  base_package = json.loads((repo / "package.json").read_text())
  variant_config = json.loads(variant_path.read_text())
  build_config = variant_config.pop("build", {})
  merged_package = deep_merge(base_package, variant_config)

  output = repo / build_config.get("output", "build/{}.pbw".format(merged_package["name"]))
  output.parent.mkdir(parents=True, exist_ok=True)

  env = os.environ.copy()
  env.pop("CC", None)
  env.pop("CXX", None)
  for key, value in build_config.get("env", {}).items():
    env[key] = str(value)

  with tempfile.TemporaryDirectory(prefix="{}-".format(args.variant)) as temp:
    temp_repo = Path(temp) / "project"
    copy_project(repo, temp_repo)
    (temp_repo / "package.json").write_text(
      json.dumps(merged_package, indent=2, sort_keys=False) + "\n"
    )

    subprocess.run(["pebble", "build"], cwd=temp_repo, env=env, check=True)

    pbws = sorted((temp_repo / "build").glob("*.pbw"))
    if len(pbws) != 1:
      raise RuntimeError("Expected one PBW, found {}".format([str(path) for path in pbws]))

    shutil.copy2(pbws[0], output)
    print("Wrote {}".format(output))


if __name__ == "__main__":
  main()
