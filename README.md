# glyph

`glyph` is a PSP Go-first EPUB reader homebrew project for DRM-free books. It
targets a quiet, controller-first reading experience on the PSP Go's 480x272
screen while keeping host builds available for fast Linux and macOS development.

## Status

- Target hardware: PSP Go on custom firmware.
- Current format support: simple reflowable EPUB novels/text.
- Current app: library picker, cover preview, reader, progress save, settings,
  jump overlay, sample EPUB fixture, PPSSPP smoke harness.
- License: MIT. Bundled Atkinson Hyperlegible Next font files are SIL OFL.

`glyph` does not support DRM, Kindle/AZW, PDF, fixed-layout EPUB, scripting, or
browser-grade CSS/layout.

## Controls

PSP controls:

- `Cross`: select or confirm.
- `Circle`: back or close the current overlay.
- `D-pad`: move selection, change settings, or move inside overlays.
- `L` / `R`: scroll within the current reader page, then move to the previous or
  next page at the edge.
- `Start`: open reader jump/help overlay where available.

Host keyboard controls mirror the PSP layout where practical:

- `Enter`: select.
- `Escape`: back.
- Arrow keys: navigation.
- `PageUp` / `PageDown`: reader movement.

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

Host app:

```sh
make sample-book
make host
make run-host
./build/host/glyph books/glyph-sample.epub
make test
make check-format
```

PSP:

```sh
make package-psp
make run-ppsspp
```

Check local dependencies:

```sh
make check-deps
```

## Install On PSP Go

After `make package-psp`, copy the packaged app folder to PSP Go internal
storage:

```text
ef0:/PSP/GAME/glyph/EBOOT.PBP
ef0:/PSP/GAME/glyph/assets/fonts/
ef0:/PSP/GAME/glyph/books/
ef0:/PSP/GAME/glyph/saves/
ef0:/PSP/GAME/glyph/cache/
ef0:/PSP/GAME/glyph/logs/
```

Place DRM-free `.epub` files in `ef0:/PSP/GAME/glyph/books/`.

## Repo Layout

- `src/`: app, EPUB parsing, storage, layout, and platform code.
- `tests/`: host tests and fixtures.
- `tools/`: dependency checks and PPSSPP harness.
- `assets/`: bundled runtime assets and licenses.
- `artwork/`: original project artwork concepts and source files.
- `spec/`: project spec, research, and open questions.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Keep changes small, testable, and
compatible with the PSP Go-first scope. Avoid GPL/AGPL dependencies unless the
project explicitly changes its dependency policy.

## License

Project code is licensed under the [MIT License](LICENSE). Bundled font files in
`assets/fonts/` are redistributed under the SIL Open Font License 1.1 included
beside them.
