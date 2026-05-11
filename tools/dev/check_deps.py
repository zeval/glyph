#!/usr/bin/env python3
"""Check glyph development dependencies without installing anything."""

from __future__ import annotations

import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class Tool:
  name: str
  command: str
  required_for: str


TOOLS = [
  Tool("cmake", "cmake", "host/PSP CMake builds"),
  Tool("make", "make", "top-level developer commands"),
  Tool("pkg-config", "pkg-config", "SDL2 dependency discovery"),
  Tool("python3", "python3", "developer helper scripts"),
  Tool("clang-format", "clang-format", "format/check-format"),
  Tool("psp-cmake", "psp-cmake", "PSP builds"),
]

PKG_CONFIG_MODULES = [
  ("sdl2", "host SDL2 app"),
  ("SDL2_image", "image decode path"),
  ("SDL2_ttf", "font rendering"),
]


def run(args: list[str]) -> tuple[int, str]:
  try:
    result = subprocess.run(args, check=False, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
  except OSError as exc:
    return 127, str(exc)
  return result.returncode, result.stdout.strip()


def install_hint() -> str:
  system = platform.system().lower()
  if system == "darwin":
    return "brew install cmake pkg-config clang-format sdl2 sdl2_image sdl2_ttf catch2"
  if system == "linux":
    return ("sudo apt install cmake make pkg-config clang-format "
            "libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev catch2")
  return "Install CMake, Make, pkg-config, clang-format, SDL2, SDL2_image, SDL2_ttf, Catch2."


def main() -> int:
  print("glyph dependency check")
  print(f"platform: {platform.platform()}")
  print("")

  missing_required = False
  for tool in TOOLS:
    path = shutil.which(tool.command)
    if path is None:
      optional = tool.command == "psp-cmake"
      status = "missing optional" if optional else "missing"
      print(f"[{status}] {tool.command}: {tool.required_for}")
      if not optional:
        missing_required = True
    else:
      print(f"[ok] {tool.command}: {path}")

  print("")
  pkg_config = shutil.which("pkg-config")
  if pkg_config is None:
    missing_required = True
    print("[missing] pkg-config modules cannot be checked")
  else:
    for module, purpose in PKG_CONFIG_MODULES:
      code, output = run([pkg_config, "--modversion", module])
      if code == 0:
        print(f"[ok] {module}: {output}")
      else:
        print(f"[missing] {module}: {purpose}")
        missing_required = True

  print("")
  print("install hint:")
  print(f"  {install_hint()}")
  print("")
  print("PSPDEV install docs: https://pspdev.github.io/installation.html")

  return 1 if missing_required else 0


if __name__ == "__main__":
  sys.exit(main())
