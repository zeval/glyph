# Topic: Toolchain And Language

## Decision

Use C/C++ with PSPDEV/PSPSDK and CMake. This is the lowest-risk path for `glyph`.

Language/runtime decision:

- Use C++17 with C libraries where useful.
- No exceptions.
- No RTTI.
- RAII is encouraged for resource cleanup.
- Functions that can fail should return explicit `Result`/error values.
- Avoid hidden heap-heavy abstractions in PSP hot paths.

The modern PSPDEV ecosystem provides:

- Allegrex MIPS GCC toolchain.
- PSPSDK headers/stubs/runtime.
- `psp-cmake`.
- `psp-pkg-config`.
- `psp-pacman`.
- `create_pbp_file` CMake helper.
- Packaged SDL2/FreeType/image/XML/compression libraries.
- Debugging through PSPLINK.

## Practical Setup

Development environments must work on:

- Linux/Ubuntu Server, including the current agent machine.
- macOS, including Apple Silicon and Intel where PSPDEV supports both.

Decision: native PSPDEV setup is first-class on both Linux and macOS. Docker remains optional for CI/reproducible Linux builds, not the canonical local development path.

Optional reproducible setup for Linux/CI:

```sh
docker pull pspdev/pspdev:latest
docker run -ti -v "$PWD:/source" pspdev/pspdev:latest
cd /source
```

Native setup should also be supported because PSPDEV publishes macOS install instructions and prebuilt SDK archives. macOS setup uses Homebrew dependencies, `PSPDEV="$HOME/pspdev"`, `PATH="$PATH:$PSPDEV/bin"`, and may require clearing Gatekeeper quarantine with `xattr -rd com.apple.quarantine $HOME/pspdev`.

Repository scripts should be written as portable POSIX shell where practical, with explicit fallbacks for macOS differences such as BSD `sed` vs GNU `sed`.

Host dependencies use system packages:

- Ubuntu: apt.
- macOS: Homebrew.
- Avoid vcpkg/vendor complexity unless a dependency becomes hard to install natively.

Build system decision:

- CMake is the underlying build system.
- A top-level `Makefile` provides simple commands such as `make host`, `make psp`, `make test`, `make run-host`, and `make run-ppsspp`.
- Pure Make is avoided because host+PSP dual builds and SDL2/pkg-config integration are easier with CMake.

Project build:

```sh
mkdir build
cd build
psp-cmake ..
make
```

Output:

```text
build/EBOOT.PBP
```

Deploy to:

```text
ms0:/PSP/GAME/glyph/EBOOT.PBP
```

## Minimal PSP Program Requirements

Every app needs:

- `PSP_MODULE_INFO(...)`.
- `PSP_MAIN_THREAD_ATTR(...)`.
- Exit callback so Home button can exit.
- `create_pbp_file(...)` in CMake for an `EBOOT.PBP`.

PSPDEV examples link simple text output with:

- `pspdebug`
- `pspdisplay`
- `pspge`

SDL2 examples use `pkg_search_module` and link SDL2 packages.

## Language Options

### C

Pros:

- Closest to PSPSDK APIs.
- Easy C library integration.
- Small runtime surface.

Cons:

- EPUB/layout state management becomes error-prone.
- Harder ownership semantics.

### C++14/C++17

Recommended.

Pros:

- Good fit for parser/layout abstractions.
- RAII can contain resource leaks.
- Better data structures for document model and caches.
- Still integrates with PSPSDK C APIs.

Constraints:

- No exceptions.
- No RTTI.
- Use explicit allocation strategies for big buffers.
- Keep templates modest to control binary size/build time.

### Rust

Viable research option only. `rust-psp` exists and can produce EBOOTs with allocation support, but it is not the canonical PSPDEV path and may complicate packaged C library use.

### Zig

Viable research option only. Zig-PSP exists, but the ecosystem is smaller than PSPDEV C/C++.

### Lua

Not recommended for `glyph` core. Lua players are useful for simple homebrew and prototypes, but an EPUB reader needs tight control over I/O, memory, text rendering, and layout.

## Dependency Management

Use PSPDEV packages where practical:

```sh
psp-pacman -Syu
psp-pacman -Sl
psp-pacman -S <package>
```

For a release, generate bundled license files:

```sh
psp-create-license-directory sdl2 sdl2-image sdl2-ttf
```

Extend the list with every linked package.

Project license is MIT. Dependency policy for v1:

- Accept MIT-compatible permissive licenses after audit: MIT, BSD-2-Clause, BSD-3-Clause, Zlib, ISC, Apache-2.0.
- Accept SIL OFL for bundled fonts after audit.
- Avoid GPL, LGPL, AGPL, CC BY-SA, and unclear/custom licenses unless explicitly approved later.
- Build tools such as GCC do not make `glyph` GPL; linked/runtime libraries still need audit.

Approved dependency:

- `nlohmann/json` for small settings/progress/bookmark files. License: MIT. Header-only. Monitor PSP binary size.

## Version Pinning

PSPDEV has frequent releases. Pin toolchain version in project docs/CI once implementation starts. At research time, GitHub showed PSPDEV release `v20260501` as latest on May 10, 2026.

Suggested repo artifact later:

- `docs/toolchain.md` with exact PSPDEV release.
- Optional Docker digest for reproducibility.
- `scripts/build-psp.sh`.

## Recommended Initial CMake Shape

```cmake
cmake_minimum_required(VERSION 3.11)

project(glyph LANGUAGES C CXX)

add_executable(glyph
    src/main.cpp
)

find_package(PkgConfig REQUIRED)
pkg_search_module(SDL2 REQUIRED sdl2)
pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image)
pkg_search_module(SDL2_TTF REQUIRED SDL2_ttf)

target_include_directories(glyph PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(glyph PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
)

if(PSP)
    create_pbp_file(
        TARGET glyph
        ICON_PATH NULL
        BACKGROUND_PATH NULL
        PREVIEW_PATH NULL
        TITLE glyph
        VERSION 00.01
    )
endif()
```

## Sources

- PSPDEV home: https://pspdev.github.io/
- PSPDEV macOS install: https://pspdev.github.io/installation/macos.html
- PSPDEV Docker install: https://pspdev.github.io/installation/docker.html
- PSPDEV how-to-use guide: https://pspdev.github.io/how_to_use.html
- PSPDEV basic examples: https://pspdev.github.io/basic_programs.html
- PSPDEV package index: https://pspdev.github.io/psp-packages/
- PSPSDK repository: https://github.com/pspdev/pspsdk
- PSPDEV releases: https://github.com/pspdev/pspdev/releases
- Rust PSP: https://github.com/overdrivenpotato/rust-psp
- Zig PSP: https://github.com/zPSP-Dev/Zig-PSP
