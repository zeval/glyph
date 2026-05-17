<p align="center">
  <img src="assets/branding/glyph-wordmark.svg" alt="glyph" width="360">
</p>

<p align="center">
  <a href="https://github.com/zeval/glyph/actions/workflows/ci.yml">
    <img
      src="https://github.com/zeval/glyph/actions/workflows/ci.yml/badge.svg?branch=main"
      alt="ci status">
  </a>
  <a href="https://github.com/zeval/glyph/actions/workflows/psp-build.yml">
    <img
      src="https://github.com/zeval/glyph/actions/workflows/psp-build.yml/badge.svg?branch=main"
      alt="psp-build status">
  </a>
</p>

`glyph` is a PlayStation Portable homebrew EPUB reader, designed first for PSP
Go and DRM-free reflowable books.

The project is in active early development. The repository currently provides a
native C++17 SDL2 app, host Linux/macOS builds, PSP EBOOT builds, a PPSSPP smoke
harness, EPUB metadata/text extraction, simple pagination, progress/settings
storage, bundled reader fonts, and a sample EPUB fixture. EPUB support is
narrow: expect simple novel-style EPUBs to work best, not a complete desktop
EPUB engine.

## Status

- Primary device: PSP Go on custom firmware.
- Primary storage: `ef0:/PSP/GAME/glyph/`.
- Book folder: `ef0:/PSP/GAME/glyph/books/`.
- Host development: Linux and macOS.
- Emulator smoke tests: PPSSPP when available.
- License: MIT for project code; SIL OFL for bundled Atkinson Hyperlegible Next
  font files.

`glyph` does not support DRM, Kindle/AZW, PDF, fixed-layout EPUB, scripting, or
browser-grade CSS/layout.

## Controls

PSP:

| Control | Action |
| --- | --- |
| D-pad up/down | Move through lists or scroll one reader line |
| D-pad left/right | Previous/next reader page; adjust settings |
| Cross | Select, open, or jump |
| Circle | Back; leave reader and save progress |
| L/R | Scroll within the current page, then move to previous/next page at the edge |
| Triangle or Select | Open the chapter/page jump overlay |
| Start | Open or close settings |

Host keyboard:

| Key | Action |
| --- | --- |
| Arrow keys | D-pad equivalent |
| Enter | Cross |
| Esc | Circle |
| PageUp or Q | L |
| PageDown or E | R |
| T | Triangle |
| Tab | Select |
| S | Start/settings |
| Digits | Enter a page number in the jump overlay |
| Backspace | Delete a page digit |

## Dependencies

Ubuntu:

```sh
sudo apt install cmake make pkg-config clang-format \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev zlib1g-dev catch2
```

macOS:

```sh
brew install cmake pkg-config clang-format sdl2 sdl2_image sdl2_ttf catch2 zlib
```

PSP builds require PSPDEV/PSPSDK:

```sh
export PSPDEV="$HOME/pspdev"
export PATH="$PATH:$PSPDEV/bin"
```

## Build

Host app and tests:

```sh
make sample-book
make host
make test
make check-format
```

Run the host app:

```sh
make run-host
./build/host/glyph books/glyph-sample.epub
```

For a headless host smoke test:

```sh
timeout 2s env SDL_VIDEODRIVER=dummy ./build/host/glyph
```

PSP:

```sh
make psp
make package-psp
make run-ppsspp
```

Check local dependencies:

```sh
make check-deps
```

## Install On PSP Go

After `make package-psp`, copy the contents of `build/package/glyph/` to:

```text
ef0:/PSP/GAME/glyph/
```

Expected layout:

```text
ef0:/PSP/GAME/glyph/EBOOT.PBP
ef0:/PSP/GAME/glyph/assets/fonts/
ef0:/PSP/GAME/glyph/books/
ef0:/PSP/GAME/glyph/saves/
ef0:/PSP/GAME/glyph/cache/
ef0:/PSP/GAME/glyph/logs/
```

Place DRM-free `.epub` files in `books/`. Settings and reading progress are
stored under `saves/`.

## Repo Layout

- `src/`: app, EPUB parsing, storage, layout, and platform code.
- `tests/`: host tests.
- `tools/`: dependency checks, fixtures, and harness scripts.
- `assets/`: bundled runtime assets and license files.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Keep changes small, testable, and
compatible with the PSP Go-first scope. Avoid GPL/AGPL dependencies unless the
project explicitly changes its dependency policy.

## Acknowledgements

`glyph` benefits from a lot of excellent open-source work:

- [PSPDEV](https://pspdev.github.io/) and
  [PSPSDK](https://github.com/pspdev/pspsdk) for the modern PSP homebrew
  toolchain.
- [SDL2](https://www.libsdl.org/), SDL2_image, and SDL2_ttf for portable
  rendering, input, image loading, and font rendering.
- [FreeType](https://freetype.org/) and [zlib](https://zlib.net/) for text
  rasterization and EPUB ZIP container support.
- [Catch2](https://github.com/catchorg/Catch2) for host-side tests.
- [PPSSPP](https://www.ppsspp.org/) for emulator-based smoke testing.
- [Atkinson Hyperlegible Next](https://www.brailleinstitute.org/freefont/) from
  Braille Institute for the bundled reader font.

## License

Project code is MIT licensed. See [LICENSE](LICENSE).

Bundled Atkinson Hyperlegible Next font files are redistributed under the SIL
Open Font License 1.1. See [assets/fonts/README.md](assets/fonts/README.md) and
[assets/fonts/OFL.txt](assets/fonts/OFL.txt).

Normal dependencies must remain MIT-compatible permissive licenses such as MIT,
BSD, Zlib, ISC, or Apache-2.0. GPL and AGPL dependencies are not accepted
without an explicit project decision.
