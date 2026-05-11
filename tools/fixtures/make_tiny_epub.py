#!/usr/bin/env python3
"""Generate a deterministic tiny EPUB for glyph tests and smoke checks."""

from __future__ import annotations

import argparse
import hashlib
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence
from xml.etree import ElementTree


FIXED_ZIP_TIME = (2020, 1, 1, 0, 0, 0)
MIMETYPE = b"application/epub+zip"


@dataclass(frozen=True)
class EpubEntry:
    path: str
    data: bytes


def text(value: str) -> bytes:
    lines = value.strip().splitlines()
    if len(lines) <= 1:
        return value.strip().encode("utf-8") + b"\n"

    indentation = min(len(line) - len(line.lstrip(" ")) for line in lines[1:] if line.strip())
    normalized = "\n".join([lines[0], *(line[indentation:] for line in lines[1:])])
    return normalized.encode("utf-8") + b"\n"


def epub_entries() -> list[EpubEntry]:
    return [
        EpubEntry(
            "META-INF/container.xml",
            text(
                """<?xml version="1.0" encoding="UTF-8"?>
                <container version="1.0"
                    xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
                  <rootfiles>
                    <rootfile full-path="EPUB/package.opf"
                        media-type="application/oebps-package+xml"/>
                  </rootfiles>
                </container>"""
            ),
        ),
        EpubEntry(
            "EPUB/package.opf",
            text(
                """<?xml version="1.0" encoding="UTF-8"?>
                <package version="3.0"
                    unique-identifier="bookid"
                    xmlns="http://www.idpf.org/2007/opf">
                  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
                    <dc:identifier id="bookid">urn:uuid:11111111-2222-3333-4444-555555555555</dc:identifier>
                    <dc:title>Glyph Tiny Fixture</dc:title>
                    <dc:creator>glyph tests</dc:creator>
                    <dc:language>en</dc:language>
                    <meta property="dcterms:modified">2020-01-01T00:00:00Z</meta>
                  </metadata>
                  <manifest>
                    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml"
                        properties="nav"/>
                    <item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>
                    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
                  </manifest>
                  <spine toc="ncx">
                    <itemref idref="chapter"/>
                  </spine>
                </package>"""
            ),
        ),
        EpubEntry(
            "EPUB/nav.xhtml",
            text(
                """<?xml version="1.0" encoding="UTF-8"?>
                <!DOCTYPE html>
                <html xmlns="http://www.w3.org/1999/xhtml"
                    xmlns:epub="http://www.idpf.org/2007/ops"
                    lang="en" xml:lang="en">
                  <head>
                    <title>Glyph Tiny Fixture</title>
                  </head>
                  <body>
                    <nav epub:type="toc" id="toc">
                      <h1>Contents</h1>
                      <ol>
                        <li><a href="chapter.xhtml">Start</a></li>
                      </ol>
                    </nav>
                  </body>
                </html>"""
            ),
        ),
        EpubEntry(
            "EPUB/chapter.xhtml",
            text(
                """<?xml version="1.0" encoding="UTF-8"?>
                <!DOCTYPE html>
                <html xmlns="http://www.w3.org/1999/xhtml"
                    lang="en" xml:lang="en">
                  <head>
                    <title>Start</title>
                  </head>
                  <body>
                    <h1>Start</h1>
                    <p>This tiny EPUB is generated deterministically for glyph tests.</p>
                    <p>It has one spine item, one nav entry, and one NCX entry.</p>
                  </body>
                </html>"""
            ),
        ),
        EpubEntry(
            "EPUB/toc.ncx",
            text(
                """<?xml version="1.0" encoding="UTF-8"?>
                <ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
                  <head>
                    <meta name="dtb:uid" content="urn:uuid:11111111-2222-3333-4444-555555555555"/>
                    <meta name="dtb:depth" content="1"/>
                    <meta name="dtb:totalPageCount" content="0"/>
                    <meta name="dtb:maxPageNumber" content="0"/>
                  </head>
                  <docTitle><text>Glyph Tiny Fixture</text></docTitle>
                  <navMap>
                    <navPoint id="navpoint-1" playOrder="1">
                      <navLabel><text>Start</text></navLabel>
                      <content src="chapter.xhtml"/>
                    </navPoint>
                  </navMap>
                </ncx>"""
            ),
        ),
    ]


def zip_info(path: str, compression: int) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(path, FIXED_ZIP_TIME)
    info.compress_type = compression
    info.create_system = 3
    info.external_attr = 0o644 << 16
    return info


def write_epub(output: Path, force: bool) -> None:
    if output.exists() and not force:
        raise FileExistsError(f"refusing to overwrite existing file: {output}")

    output.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(output, "w") as archive:
        archive.writestr(zip_info("mimetype", zipfile.ZIP_STORED), MIMETYPE)
        for entry in epub_entries():
            archive.writestr(zip_info(entry.path, zipfile.ZIP_STORED), entry.data)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_epub(path: Path) -> None:
    with zipfile.ZipFile(path, "r") as archive:
        names = archive.namelist()
        if not names:
            raise ValueError("EPUB archive is empty")
        if names[0] != "mimetype":
            raise ValueError("mimetype must be the first ZIP entry")

        mimetype_info = archive.getinfo("mimetype")
        if mimetype_info.compress_type != zipfile.ZIP_STORED:
            raise ValueError("mimetype must be stored without compression")
        if archive.read("mimetype") != MIMETYPE:
            raise ValueError("mimetype has unexpected content")

        required = {
            "META-INF/container.xml",
            "EPUB/package.opf",
            "EPUB/nav.xhtml",
            "EPUB/chapter.xhtml",
            "EPUB/toc.ncx",
        }
        missing = sorted(required.difference(names))
        if missing:
            raise ValueError("missing EPUB entries: " + ", ".join(missing))

        for xml_path in sorted(required):
            ElementTree.fromstring(archive.read(xml_path))


def parser() -> argparse.ArgumentParser:
    arg_parser = argparse.ArgumentParser(
        description="Generate glyph's deterministic tiny EPUB fixture.",
    )
    arg_parser.add_argument(
        "--output",
        required=True,
        metavar="PATH",
        help="where to write the generated .epub file",
    )
    arg_parser.add_argument(
        "--force",
        action="store_true",
        help="overwrite the output file if it already exists",
    )
    arg_parser.add_argument(
        "--check",
        action="store_true",
        help="verify the generated EPUB structure after writing it",
    )
    arg_parser.add_argument(
        "--sha256",
        action="store_true",
        help="print the generated file's SHA-256 digest",
    )
    return arg_parser


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    output = Path(args.output).expanduser()

    try:
        write_epub(output, args.force)
        if args.check:
            verify_epub(output)
    except (ElementTree.ParseError, OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"failed to generate EPUB fixture: {exc}", file=sys.stderr)
        return 1

    print(f"Wrote {output} ({output.stat().st_size} bytes)")
    if args.sha256:
        print(f"sha256 {sha256_file(output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
