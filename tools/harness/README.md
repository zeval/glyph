# Glyph Harnesses

## Host UI Snapshots

The host SDL app can render one deterministic frame and write it as a PNG. This
is the fastest way to review PSP-sized UI changes on a headless machine.

Capture the standard review set:

```sh
make ui-snapshots
```

Outputs:

```text
build/ui/browser.png
build/ui/settings.png
build/ui/reader.png
build/ui/reader-select.png
build/ui/index.html
```

The target builds the host app, generates `build/fixtures/tiny.epub`, captures
the screenshots with SDL's dummy video driver, and writes a small HTML contact
sheet.

Capture one screen manually:

```sh
env SDL_VIDEODRIVER=dummy ./build/host/glyph --snapshot reader-select --book build/fixtures/tiny.epub --out build/ui/reader-select.png
```

Available snapshot names:

- `browser`
- `settings`
- `reader`
- `reader-select`

## PPSSPP Harness

This directory contains the PPSSPP harness for glyph. It currently detects an
existing emulator install and can run a bounded smoke launch. It does not build,
download, or install PPSSPP.

## Detection

Run detection only:

```sh
python3 tools/harness/run_ppsspp_smoke.py
```

The script searches `PATH` for these command names:

- `ppsspp`
- `PPSSPPSDL`
- `PPSSPPQt`
- `ppsspp-emu.ppsspp-sdl`

If none are found, detect mode prints a clear unavailable message and exits `0`.
That lets CI and local setup checks report emulator availability without failing
normal builds.

## Smoke Launch

Run the first detected PPSSPP candidate with a timeout:

```sh
python3 tools/harness/run_ppsspp_smoke.py --run
```

Without `--eboot`, the smoke command passes `--help` to the emulator. With an
EBOOT or other PSP load target, that path is passed directly:

```sh
python3 tools/harness/run_ppsspp_smoke.py --run --eboot path/to/EBOOT.PBP
```

The process is terminated after the timeout, which defaults to 10 seconds. A
still-running emulator at timeout is treated as a successful launch smoke.

Override the executable when PPSSPP is not on `PATH`:

```sh
python3 tools/harness/run_ppsspp_smoke.py --ppsspp /path/to/PPSSPPSDL --run
```

Adjust the timeout:

```sh
python3 tools/harness/run_ppsspp_smoke.py --run --timeout 3
```

## Test EPUB Input

Generate a deterministic EPUB fixture when parser or reader checks need a small
book file:

```sh
make fixture-epub
```

The generated file defaults to `build/fixtures/tiny.epub`. It is not required for
PPSSPP detection, and the harness still launches the emulator only when `--run`
is passed.

## Install And Build Hooks

Install PPSSPP separately using the platform's normal packaging route before
running `--run`. Examples include a system package, a local PPSSPP build whose
binary directory is added to `PATH`, or a manually supplied executable via
`--ppsspp`.

The `ppsspp-emu.ppsspp-sdl` command is the name currently exposed by the
`ppsspp-emu` snap package.

Flatpak is documented as a future hook. The expected app id is
`org.ppsspp.PPSSPP`; future harness work can add a launcher such as:

```sh
flatpak run org.ppsspp.PPSSPP path/to/EBOOT.PBP
```

The harness intentionally avoids building PPSSPP from source.
