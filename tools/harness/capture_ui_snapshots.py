#!/usr/bin/env python3
"""Capture deterministic host UI screenshots for review."""

from __future__ import annotations

import argparse
import html
import os
import subprocess
import sys
from pathlib import Path


SNAPSHOTS = (
    ("browser", "Library"),
    ("settings", "Settings"),
    ("reader", "Reader"),
    ("reader-select", "Reader Select"),
)


def run(command: list[str], cwd: Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def write_contact_sheet(output_dir: Path) -> None:
    cards = []
    for snapshot, title in SNAPSHOTS:
        image = f"{snapshot}.png"
        cards.append(
            f"""<section>
  <h2>{html.escape(title)}</h2>
  <img src="{html.escape(image)}" width="480" height="272" alt="{html.escape(title)} screenshot">
</section>"""
        )

    html_text = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>glyph UI snapshots</title>
  <style>
    body {
      margin: 24px;
      background: #101214;
      color: #e8e6de;
      font: 14px system-ui, sans-serif;
    }
    main {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(480px, 1fr));
      gap: 24px;
    }
    h1 {
      font-size: 18px;
      margin: 0 0 18px;
    }
    h2 {
      color: #94a0a4;
      font-size: 14px;
      font-weight: 500;
      margin: 0 0 8px;
    }
    img {
      border: 1px solid #263039;
      image-rendering: auto;
      max-width: 100%;
    }
  </style>
</head>
<body>
  <h1>glyph UI snapshots</h1>
  <main>
""" + "\n".join(cards) + """
  </main>
</body>
</html>
"""
    (output_dir / "index.html").write_text(html_text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--glyph", default="build/host/glyph", help="host glyph executable")
    parser.add_argument("--out", default="build/ui", help="output screenshot directory")
    parser.add_argument("--fixture", default="build/fixtures/tiny.epub", help="fixture EPUB path")
    args = parser.parse_args()

    repo = Path.cwd()
    glyph = Path(args.glyph)
    output_dir = Path(args.out)
    fixture = Path(args.fixture)
    output_dir.mkdir(parents=True, exist_ok=True)
    fixture.parent.mkdir(parents=True, exist_ok=True)

    run(
        [
            sys.executable,
            "tools/fixtures/make_tiny_epub.py",
            "--output",
            str(fixture),
            "--force",
            "--check",
        ],
        repo,
    )

    env = os.environ.copy()
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    for snapshot, _ in SNAPSHOTS:
        command = [
            str(glyph),
            "--snapshot",
            snapshot,
            "--out",
            str(output_dir / f"{snapshot}.png"),
        ]
        if snapshot.startswith("reader"):
            command.extend(["--book", str(fixture)])
        subprocess.run(command, cwd=repo, env=env, check=True)

    write_contact_sheet(output_dir)
    print(f"Wrote UI snapshots to {output_dir}")
    print(f"Open {output_dir / 'index.html'} for the contact sheet")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
