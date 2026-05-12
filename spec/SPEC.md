# glyph Project Spec

`glyph` is a PlayStation Portable homebrew e-reader focused on DRM-free EPUB files. The project is feasible, but it should be treated as a constrained embedded reader rather than a standards-complete desktop EPUB engine.

The recommended path is a native C/C++ PSP homebrew app using the current PSPDEV/PSPSDK toolchain, CMake via `psp-cmake`, SDL2 for the first rendering/input backend, and a custom EPUB pipeline that supports reflowable EPUB 2 plus a simple EPUB 3 subset.

Project license: MIT. Dependency policy: MIT-compatible permissive dependencies are acceptable after audit, including MIT, BSD-2-Clause, BSD-3-Clause, Zlib, ISC, and Apache-2.0. Avoid GPL/AGPL dependencies unless explicitly approved later. Bundled fonts may use SIL OFL after audit.

Development environments must support the current agent machine, Linux/Ubuntu Server, and macOS. Tooling should avoid Linux-only assumptions unless there is an equivalent macOS path.

## Goals

- Run on PSP Go as the primary hardware target.
- Open DRM-free `.epub` files from PSP Go internal storage and Memory Stick Micro.
- Render readable, paginated text on the PSP's 480x272 display.
- Support common novel/text EPUBs in v1, while keeping the long-term project goal broad EPUB compatibility.
- Provide a controller-first reader UX: file browser, table of contents, reading progress, bookmarks, recent books, font size/theme settings.
- Support PSP Go collapsed reading: L/R alone must be enough for page-edge jumps and fine scrolling.
- Keep the code portable enough to run a host build for parser/layout tests.

## Non-Goals For MVP

- DRM, Adobe ACSM, Readium LCP, Kindle/AZW, PDF, CBZ, MOBI, DJVU, CHM.
- Full EPUB 3 conformance.
- JavaScript, audio/video, media overlays, MathML, complex SVG, interactive content.
- Fixed-layout EPUB/comics as first-class content.
- Full CSS layout engine or browser-grade HTML recovery.
- Complete Unicode shaping for Arabic, Indic scripts, complex OpenType features, or CJK typography.
- Network catalogues, sync, Wi-Fi downloads, cloud features.

## Feasibility Verdict

Feasible if scope is tight.

PSP homebrew tooling is active in 2026. PSPDEV has current releases and packaged libraries including SDL2, SDL2_image, SDL2_ttf, FreeType, HarfBuzz, libzip/minizip, expat, stb, ImGui, and PSP-native graphics/input APIs. The hard part is not producing an `EBOOT.PBP`; it is building an EPUB parser/layout/rendering stack that stays within PSP memory and performance limits.

The baseline PSP Go has 64 MB RAM, 16 GB internal storage exposed as `ef0:/`, and a 480x272 3.8 inch screen. This lifts the PSP-1000 32 MB constraint, allowing larger chapter buffers, glyph caches, and metadata/layout caches. Modern EPUBs can still include large images, embedded fonts, complex CSS, broken XHTML, and features that assume browser engines. `glyph` should therefore define an explicit EPUB subset and degrade unsupported features cleanly.

## Recommended Technical Direction

### Language

Use C++17 with C libraries where useful. C is also viable, but C++ gives useful ownership and layout abstractions. Runtime policy: no exceptions and no RTTI for PSP predictability; use RAII plus explicit `Result`/error values.

Alternatives:

- Rust via `rust-psp`: promising, but smaller PSP ecosystem and harder interop with PSPDEV packages.
- Zig-PSP: promising, but niche.
- Lua: useful for experiments, not for an EPUB reader core.

See [01-toolchain-and-language.md](01-toolchain-and-language.md).

### Toolchain

Use PSPDEV/PSPSDK with CMake:

```sh
mkdir build
cd build
psp-cmake ..
make
```

The output `EBOOT.PBP` is copied to PSP Go internal storage:

```text
ef0:/PSP/GAME/glyph/EBOOT.PBP
```

Start development on custom firmware and PPSSPP. Add real PSP Go testing early, using PSPLINK for crash/debug loops when hardware is attached.

The user's PSP Go currently runs 6.61 PRO-C. Custom firmware is assumed for v1. Unmodded/official firmware support is not a project goal.

### Rendering/UI

Start with SDL2 + SDL2_image + SDL2_ttf. Keep a small internal renderer interface so the backend can later be replaced or supplemented with native `libGU` and `pspctrl`.

Reason:

- SDL2 gives faster iteration and desktop parity for UI/parser/layout development.
- PSPDEV packages include SDL2, SDL2_image, SDL2_ttf, FreeType, and HarfBuzz.
- Native `libGU` is likely faster and gives tighter VRAM control, but increases implementation cost.

See [04-ui-rendering-input.md](04-ui-rendering-input.md).

### EPUB Pipeline

Implement a minimal reader pipeline:

1. Open `.epub` as ZIP/OCF.
2. Read `META-INF/container.xml`.
3. Parse the package `.opf`.
4. Build manifest, spine, metadata, and navigation.
5. Load one spine item/chapter at a time.
6. Parse XHTML as XML-first content.
7. Convert supported elements into a small document model.
8. Apply a small CSS subset.
9. Lay out blocks and inline text into PSP-sized pages.
10. Cache pagination/progress per book/font-size/theme where useful.

Use PSP-packaged libraries first where possible:

- ZIP: `minizip`, `libzip`, or bundled `miniz` after prototype benchmarking.
- XML: `expat`, `yxml`, or `pugixml` for small metadata docs.
- Images: `SDL2_image` for MVP; downsample aggressively.
- Fonts: `SDL2_ttf`/FreeType for MVP; consider HarfBuzz only after basic reader works.
- M0 bundled font: Atkinson Hyperlegible or Atkinson Hyperlegible Next under SIL OFL, after auditing exact upstream license/package.

All dependencies must be license-audited against the MIT project license before release. GPL/AGPL libraries are out of scope for v1.

See [03-epub-format-and-rendering.md](03-epub-format-and-rendering.md).

## MVP Feature Set

### Library/File Browser

- Browse `ef0:/` by default on PSP Go, with `ms0:/` support for Memory Stick Micro and later 64 MB PSP models.
- Default library folder: `ef0:/PSP/GAME/glyph/books/`.
- Default app data folders: `saves/`, `cache/`, and `logs/` inside `ef0:/PSP/GAME/glyph/`.
- File browser may still access `ef0:/` and `ms0:/`.
- Filter `.epub`.
- Show filename, book title/author when metadata cache exists.
- Maintain recent books list.
- Handle missing/corrupt books with readable errors.

### Reader

- Page forward/back.
- Chapter forward/back.
- Table of contents.
- Progress indicator: chapter/page and approximate percentage.
- Bookmarks.
- Last position resume.
- Page mode and scroll mode.
- Font size selection.
- Light/dark/sepia-like themes.
- Margins/line spacing presets.
- Battery indicator optional for v1 if easy.

### EPUB Support

MVP supported:

- EPUB 2 OPF + NCX.
- Simple EPUB 3 OPF + navigation document.
- Reflowable XHTML chapters.
- UTF-8 text.
- Paragraphs, headings, line breaks, emphasis, strong, blockquotes, ordered/unordered lists.
- Links to local anchors/chapters.
- PNG/JPEG/GIF first frame, bounded to screen size.
- Simple CSS: margins, text alignment, font-style, font-weight, font-size, color if easy.

MVP unsupported or degraded:

- DRM/encryption.
- Scripting/forms.
- Fixed-layout EPUB.
- Audio/video/media overlays.
- MathML.
- Complex tables.
- SVG beyond maybe image fallback.
- Remote resources.
- Embedded fonts unless a test book proves it is cheap enough.

Unsupported or malformed EPUB content should degrade gracefully in v1: open what can be opened, skip unsupported pieces, surface clear warnings/errors, and never crash.

### Performance Budgets

Baseline target: PSP Go / 64 MB PSP class.

- RAM: app + current chapter + layout + glyph/image cache under 32-40 MB resident where practical, leaving headroom for SDL2/PSPSDK, framebuffers, image decode scratch, and fragmentation.
- VRAM: assume 2 MB usable graphics VRAM pressure; avoid large texture caches.
- CPU: idle reader loop at low/normal clock; use 333 MHz only for loading, image decode, parsing, or repagination.
- Storage: use `ef0:/` for app data/cache by default; cache files should remain bounded, recoverable, and safe to delete.
- Page turns: target under 100 ms when page already laid out.
- Chapter load/reflow: acceptable if seconds for large chapters, but show progress.

See [02-platform-constraints.md](02-platform-constraints.md).

## Architecture

```text
app/
  platform/
    psp_main, callbacks, clock, paths, file IO
    host stubs for tests
  ui/
    screens: browser, reader, toc, settings, error
    input mapping
  render/
    Renderer interface
    SDL2 backend first
    optional GU backend later
  epub/
    zip/ocf reader
    opf parser
    nav/ncx parser
    xhtml parser
    css subset parser
    resource loader
  layout/
    document model
    inline layout
    pagination
    anchors/progress
  text/
    utf8 decode
    font manager
    glyph cache
    line breaking
storage/
    metadata cache
    settings
    bookmarks/progress
```

On PSP Go, default persistent paths:

```text
ef0:/PSP/GAME/glyph/books/
ef0:/PSP/GAME/glyph/saves/
ef0:/PSP/GAME/glyph/cache/
ef0:/PSP/GAME/glyph/logs/
```

Settings, progress, and bookmarks are stored as small JSON files with explicit schema/version fields. Use `nlohmann/json` for these small files and monitor PSP binary size.

Keep the EPUB parser/layout independent from PSP APIs. That allows host tests and faster iteration without copying builds to a PSP.

Host build/test harness is approved for first implementation work. The host build should run the same SDL2 UI app on Linux/macOS with keyboard controls, while sharing parser/layout code with PSP builds. PPSSPP automation is also approved as the emulator harness direction.

## Data Model

### Book Metadata

- `book_id`: EPUB unique identifier if present, otherwise file path + size + mtime hash.
- `title`
- `authors`
- `language`
- `file_path`
- `file_size`
- `modified_time`
- `cover_resource` optional

### Reading State

- `book_id`
- `spine_index`
- `fragment_id` optional
- `layout_profile`: font id, font size, margin preset, line spacing, viewport.
- `page_index_in_layout`
- `byte_or_node_anchor` for recovery when layout cache changes.

### Layout Cache

- Book/layout profile key.
- Per-chapter page map.
- Anchor map for TOC links.
- Cache version so old files are ignored after format changes.

## Input Map

- `L` click: scroll up within the current page; jump to previous page at the page edge.
- `R` click: scroll down within the current page; jump to next page at the page edge.
- L/R hold: no action in v1.
- `D-pad left/right`: previous/next page.
- `D-pad up/down`: line/paragraph scroll in menus and reader scroll mode.
- `Analog`: accelerated list scroll.
- `Cross`: select/open.
- `Circle`: back.
- `Triangle`: table of contents.
- `Square`: bookmark.
- `Start`: reader menu/settings.
- `Select`: status/details.
- `Home`: normal PSP exit behavior.

Cross is accept and Circle is back for v1.

## Build And Test Strategy

### Host Tests

Build parser/layout as a desktop executable/library.

Test with:

- Synthetic EPUB fixtures.
- Valid EPUB 2 novel.
- Simple EPUB 3 novel.
- Broken but common XHTML cases.
- Large chapter.
- Large images.
- UTF-8 punctuation and accents.

CI/merge checks should run Linux and macOS host build/tests first. PSP cross-build CI can be added later once toolchain automation is stable.

### PSP/PPSSPP Tests

- Launch `EBOOT.PBP`.
- File browsing on `ms0:/`.
- Open/read/page through test books.
- Memory usage under 64 MB PSP profile.
- Font/theme switching.
- Save/resume state.

### Real Hardware Tests

Must test on PSP Go before v1. Optional compatibility testing can later cover PSP-2000/3000/E1000, and PSP-1000 only if support is reintroduced.

Use PSPLINK for crash traces and screenshots. PPSSPP is useful but not enough.

See [07-emulator-agent-harness.md](07-emulator-agent-harness.md) for automated PPSSPP test harness research.

See [05-build-debug-test-distribution.md](05-build-debug-test-distribution.md).

## Milestones

### M0: Toolchain Probe

- PSPDEV Docker/local setup.
- CMake project builds host app and PSP `EBOOT.PBP`.
- Same SDL2 app shell runs on Linux/macOS host and PSP.
- File browser UI.
- Reader placeholder page.
- Settings shell.
- L/R and keyboard input mapping.
- Initial PPSSPP harness stub that detects/runs PPSSPP if installed and documents install hooks.
- `.clang-format`, `make format`, and `make check-format`.
- M0 is complete when host app works, PSP `EBOOT.PBP` builds, and PPSSPP harness stub exists.
- Real PSP Go smoke test is follow-up, not required for M0 completion.

### M1: Desktop EPUB Core

- Open EPUB ZIP.
- Parse container/OPF/spine/NCX/nav.
- Extract metadata.
- Extract title/author/spine and show/list it in host tests.
- Convert simple XHTML to document model.
- Host tests.

### M2: PSP Text Viewer

- File browser.
- Open bundled/plain extracted chapter text.
- Render paginated text.
- Page controls.
- Settings shell.

### M3: EPUB MVP On PSP

- Open real EPUBs from storage.
- Basic CSS subset.
- TOC.
- Save/resume progress.
- Bookmarks.
- Error UI.

### M4: Polish And Compatibility

- Image support.
- Metadata cache/library view.
- Performance tuning.
- PSP Go memory/performance pass.
- License collection.
- Release package.

### M5: Optional Advanced Text

- Better Unicode line breaking.
- HarfBuzz/FriBidi experiment.
- Embedded font support.
- Native GU backend if SDL2 is too slow.

## Key Risks

- PSP Go hardware differences from PPSSPP.
- Modern EPUB complexity.
- Image-heavy books.
- Unicode shaping and font fallback.
- SDL2 performance on real hardware.
- Emulator/hardware mismatch.
- Licensing of helper libraries and bundled fonts.
- `ef0:/` vs `ms0:/` path differences and cache writes.

See [06-risks-and-open-research.md](06-risks-and-open-research.md).

Implementation starts in one workspace initially. Kandev subtasks remain available after architecture stabilizes or when work can be split cleanly.

## Open Questions

Open product/implementation questions are tracked in [QUESTIONS.md](QUESTIONS.md).

Most important:

- Linux/macOS development setup needs a concrete dependency/bootstrap script decision.
- PPSSPP harness needs an implementation spike once PPSSPP is installed/built in the agent environment.

## Primary Sources

- PSPDEV home: https://pspdev.github.io/
- PSPDEV install/use docs: https://pspdev.github.io/installation.html and https://pspdev.github.io/how_to_use.html
- PSPDEV unmodded PSP build tip: https://pspdev.github.io/tips_tricks.html
- PSPDEV package index: https://pspdev.github.io/psp-packages/
- PSPSDK repository: https://github.com/pspdev/pspsdk
- PSPDEV releases: https://github.com/pspdev/pspdev/releases
- PSP hardware wiki: https://www.psdevwiki.com/psp/Hardware
- PSP model matrix: https://www.psdevwiki.com/psp/SKU_Models
- Sony PSP-2000 specs PDF: https://www.sony.com/en/SonyInfo/IR/news/qfhh7c00000d2yu7-att/psp.pdf
- PPSSPP command-line docs: https://www.ppsspp.org/docs/reference/command-line/
- PPSSPP WebSocket API: https://www.ppsspp.org/docs/reference/websocket-api/
- W3C EPUB 3.3 overview: https://www.w3.org/TR/epub-overview-33/
- W3C EPUB 3.3 core: https://w3c.github.io/epub-specs/epub33/core/
- W3C EPUB Reading Systems 3.3: https://w3c.github.io/epub-specs/epub33/rs/
- IDPF OPF 2.0: https://idpf.org/epub/20/spec/OPF_2.0_final_spec.html
- MuPDF docs: https://mupdf.readthedocs.io/en/1.26.1/
- Bookr PSP page: https://www.gamebrew.org/wiki/Bookr_PSP_by_gtzampanakis
