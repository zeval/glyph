# Topic: Build, Debug, Test, Distribution

## Build Strategy

Maintain two build modes:

1. PSP build: produces `EBOOT.PBP`.
2. Host build: parser/layout/UI development and automated tests.

The host build should compile the non-PSP core:

- EPUB parsing.
- CSS subset parsing.
- document model.
- layout/pagination.
- settings/bookmark serialization.

The host build should also run the same SDL2 UI app shell on Linux/macOS with keyboard controls. This keeps UI iteration, screenshots, and agent-visible behavior fast before PSP hardware testing.

The PSP build links the platform layer and renders through SDL2 or native PSP backend.

This host build/test harness is approved for first implementation work.

## PSP Build Flow

```sh
mkdir build-psp
cd build-psp
psp-cmake ..
make
```

Expected output:

```text
build-psp/EBOOT.PBP
```

Install:

```text
ef0:/PSP/GAME/glyph/EBOOT.PBP
```

Also support removable Memory Stick Micro:

```text
ms0:/PSP/GAME/glyph/EBOOT.PBP
```

## Development Environment

Recommended:

- PSPDEV setup on Linux/Ubuntu Server and macOS.
- PSPDEV Docker image for reproducible Linux/CI builds where Docker is available.
- Native host compiler for tests on Linux and macOS.
- PPSSPP for quick launch loops.
- Real PSP Go with custom firmware for hardware checks.
- PSPLINK for deeper debugging.

Current user hardware:

- PSP Go.
- Firmware/custom firmware: 6.61 PRO-C.

No CFW change is required for early development unless a specific tool fails. ARK-4 is a current CFW option for PSP models on 6.60/6.61, but `glyph` should not require moving firmware just to start prototyping.

Cross-platform dev scripts should support both Linux and macOS. Avoid GNU-only shell assumptions unless the bootstrap script installs the needed tool through Homebrew on macOS.

## Debugging

### Basic Crash Loop

Use PSPLINK:

1. Build unencrypted PRX:

```sh
psp-cmake -DBUILD_PRX=1 ..
make
```

2. Run `usbhostfs_pc`.
3. Run `pspsh`.
4. Launch:

```text
./glyph.prx
```

5. Use `reset` between runs.
6. Map crash addresses:

```sh
psp-addr2line -e glyph <address>
```

Build with `-g` for useful symbols.

### GDB

Use `psp-gdb` through PSPLINK when `addr2line` is not enough.

## Test Strategy

### Unit Tests

Run on host:

- Catch2 via CTest.
- UTF-8 decoder.
- Path resolver.
- OPF parser.
- NCX parser.
- EPUB 3 nav parser.
- CSS subset parser.
- XHTML-to-document conversion.
- Layout line breaking.
- Pagination stability.
- Bookmark/progress serialization.

Required merge checks for early implementation:

- Linux host configure/build/tests.
- macOS host configure/build/tests.
- Formatting/linting once configured.

Formatting policy:

- Use clang-format from the start.
- Provide `make format`.
- Provide `make check-format`.
- Defer clang-tidy/cppcheck until after M0.

PSP cross-build CI is optional until PSPDEV automation is stable in CI. PSP builds remain required locally before PSP-facing changes are considered done.

### Fixture Tests

Keep small EPUB fixtures in a later `tests/fixtures/` directory:

- Minimal EPUB 2.
- EPUB 2 with NCX.
- EPUB 3 with navigation document.
- Missing metadata.
- Broken XHTML.
- Large single chapter.
- Image chapter.
- Unsupported fixed layout.

Expected outcomes should be explicit: open, degrade, or reject.

### Integration Tests On Host

CLI tool later:

```sh
glyph-inspect book.epub
glyph-layout book.epub --viewport 480x272 --font-size 16
```

These tools can dump:

- metadata.
- spine.
- TOC.
- page counts.
- parse/layout errors.

### PPSSPP Tests

Use for quick smoke tests:

- Launch.
- Navigate file browser.
- Open fixture EPUBs.
- Page turn.
- TOC jump.
- Settings change.
- Save/resume.

PPSSPP is not enough for release signoff.

Automated PPSSPP testing appears practical through command-line launch plus PPSSPP's WebSocket debugger API. See [07-emulator-agent-harness.md](07-emulator-agent-harness.md).

This emulator harness direction is approved.

M0 PPSSPP scope:

- Detect whether PPSSPP is installed.
- Run smoke command only if available.
- Document install/build hooks.
- Do not build PPSSPP from source in M0.

### Real Hardware Tests

Minimum hardware matrix:

- PSP Go.

Test:

- Cold boot.
- Repeated open/close books.
- Low-memory cases.
- Large image rejection.
- Slow Memory Stick behavior.
- Suspend/resume if possible.
- Home button exit.

## Distribution Strategy

Release package layout:

```text
glyph/
  EBOOT.PBP
  assets/
    fonts/
    icons/
  books/
  saves/
  cache/
  logs/
  licenses/
  README.txt
```

Install docs:

```text
Copy glyph/ to ef0:/PSP/GAME/glyph/
Copy EPUB files to ef0:/PSP/GAME/glyph/books/
```

For v1 development and release builds:

- unsigned normal homebrew is acceptable.
- 6.61 PRO-C on the user's PSP Go is acceptable.
- Custom firmware is assumed.

Unmodded/official firmware support is not a project goal. Do not spend v1 implementation time on encrypted PRX/signing support.

## Release Checklist

- Toolchain version documented.
- Third-party licenses collected.
- Font license included.
- Project license included: MIT.
- Dependency licenses audited against MIT/permissive policy.
- PSP Go memory/performance pass complete.
- Real hardware tested.
- PPSSPP tested.
- Automated emulator harness smoke test passes if available in the environment.
- Unsupported EPUB feature list documented.
- At least three public-domain test books verified.

## Sources

- PSPDEV how-to-use guide: https://pspdev.github.io/how_to_use.html
- PSPDEV macOS install guide: https://pspdev.github.io/installation/macos.html
- PSPDEV basic programs: https://pspdev.github.io/basic_programs.html
- PSPDEV debugging guide: https://pspdev.github.io/debugging.html
- PSPLINK repository: https://github.com/pspdev/psplinkusb
- PSPDEV tips/license tooling: https://pspdev.github.io/tips_tricks.html
- ARK-4 custom firmware: https://github.com/PSP-Archive/ARK-4
- PSPDEV Docker install: https://pspdev.github.io/installation/docker.html
- PPSSPP homebrew docs: https://www.ppsspp.org/docs/getting-started/how-to-get-demos-and-homebrew
