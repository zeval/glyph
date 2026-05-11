#!/usr/bin/env python3
"""Generate a tiny deterministic EPUB for local glyph smoke testing."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


TIMESTAMP = (2026, 1, 1, 0, 0, 0)


def zip_info(name: str, compression: int) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, TIMESTAMP)
    info.compress_type = compression
    info.external_attr = 0o644 << 16
    return info


def write_text(book: zipfile.ZipFile, name: str, text: str, compression: int) -> None:
    book.writestr(zip_info(name, compression), text)


def build_epub(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(path, "w") as book:
        write_text(book, "mimetype", "application/epub+zip", zipfile.ZIP_STORED)
        write_text(
            book,
            "META-INF/container.xml",
            """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OPS/package.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
""",
            zipfile.ZIP_DEFLATED,
        )
        write_text(
            book,
            "OPS/package.opf",
            """<?xml version="1.0" encoding="UTF-8"?>
<package version="2.0" xmlns="http://www.idpf.org/2007/opf">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>Glyph Sample Book</dc:title>
    <dc:creator>glyph project</dc:creator>
    <dc:language>en</dc:language>
    <dc:identifier>glyph-sample</dc:identifier>
  </metadata>
  <manifest>
    <item id="chapter-1" href="chapter-1.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine>
    <itemref idref="chapter-1"/>
  </spine>
</package>
""",
            zipfile.ZIP_DEFLATED,
        )
        write_text(
            book,
            "OPS/chapter-1.xhtml",
            """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml">
  <head><title>Glyph Sample Book</title></head>
  <body>
    <h1>Chapter One</h1>
    <p>This is a tiny EPUB generated for glyph. It proves that the PoC can open an EPUB container, read package metadata, follow the spine, extract XHTML text, and render wrapped pages.</p>
    <p>Use L and R on PSP Go, or Q and E on the host build, to move between pages. Hold the same controls to scroll by line.</p>
    <p>The parser is intentionally small. It targets simple novel-style EPUBs first, then can grow carefully as real books expose gaps.</p>
  </body>
</html>
""",
            zipfile.ZIP_DEFLATED,
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "output",
        nargs="?",
        default="books/glyph-sample.epub",
        help="output EPUB path",
    )
    args = parser.parse_args()
    build_epub(Path(args.output))
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
