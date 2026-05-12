# glyph

`glyph` is a PSP Go-first EPUB reader homebrew project. The repo currently
contains the project spec, native Linux/macOS tooling, a host SDL2 app shell, a
PSP EBOOT build, and a PPSSPP smoke harness.

## Status

- Target: PSP Go on custom firmware.
- MVP content: EPUB novels/text with Latin-script rendering.
- UI: quiet dark reader shell, PSP Go collapsed controls first.
- License: MIT. Bundled Atkinson Hyperlegible Next font is SIL OFL.

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

## PSP Layout

```text
ef0:/PSP/GAME/glyph/EBOOT.PBP
ef0:/PSP/GAME/glyph/assets/fonts/
ef0:/PSP/GAME/glyph/books/
ef0:/PSP/GAME/glyph/saves/
ef0:/PSP/GAME/glyph/cache/
ef0:/PSP/GAME/glyph/logs/
```

## Repo Layout

- `src/`: SDL2 app shell and PSP input.
- `tests/`: host tests.
- `tools/`: dependency checks and PPSSPP harness.
- `assets/`: bundled runtime assets and licenses.
- `spec/`: project spec, research, and open questions.
