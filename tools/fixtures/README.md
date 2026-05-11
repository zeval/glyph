# Glyph Test Fixtures

Generate the tiny EPUB fixture into build output:

```sh
make fixture-epub
```

The fixture is deterministic and intentionally generated on demand instead of
stored in git. It contains:

- `META-INF/container.xml`
- `EPUB/package.opf`
- `EPUB/nav.xhtml`
- `EPUB/chapter.xhtml`
- `EPUB/toc.ncx`

To choose another output path:

```sh
python3 tools/fixtures/make_tiny_epub.py --output /tmp/tiny.epub --force --check --sha256
```

`make test` also generates and verifies the fixture in the host build directory.
