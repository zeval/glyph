# Topic: Risks And Open Research

## Highest Risks

### Emulator/PSP Go Mismatch

PPSSPP is the right automation target, but it is not real PSP Go hardware. It can hide storage timing, VRAM/cache behavior, suspend/resume quirks, CFW differences, and input timing problems.

Mitigation:

- Use PPSSPP for automated smoke/regression tests.
- Use PSP Go hardware for release signoff.
- Keep a hardware-only test checklist.
- Log app state/errors to `ef0:/PSP/GAME/glyph/logs/` or another predictable path.

### 64 MB Memory Still Finite

PSP Go lifts the PSP-1000 32 MB limit, but the app still runs on a constrained device.

Mitigation:

- Prefer current + next chapter caching, not full-book DOM.
- Bounded glyph/image caches.
- Host memory tests with 64 MB-ish artificial caps.
- Real PSP Go memory/performance pass before beta.

### EPUB Complexity

Real EPUBs often contain complex CSS, malformed XHTML, huge images, embedded fonts, vendor-specific markup, or fixed layouts.

Mitigation:

- Publish explicit support matrix.
- Reject unsupported features cleanly.
- Keep fixtures for each known failure.
- Add compatibility incrementally based on real books.

### Text Rendering And Unicode

Good typography becomes hard fast. UTF-8 decode is easy; shaping, bidi, line breaking, font fallback, hyphenation, and CJK are not.

Mitigation:

- MVP: Latin-script books and simple Unicode punctuation.
- Later: HarfBuzz/FriBidi spike if user needs Arabic/Hebrew/Indic.
- Later: CJK font/memory spike if needed.

### SDL2 Performance

SDL2 simplifies development, but may be slower than native PSP GU.

Mitigation:

- Cache page render output.
- Avoid per-frame rerendering.
- Profile on real hardware.
- Keep renderer abstraction small.
- Add native GU backend only if needed.

### Image-Heavy Books

Modern EPUBs can include full-page/high-resolution images that exceed PSP memory.

Mitigation:

- Decode one image at a time.
- Downsample early.
- Enforce max decoded pixels.
- Placeholder on failure.
- No fixed-layout/comic support in MVP.

### Storage Reliability And Speed

PSP Go internal storage is better than relying only on removable media, but `ef0:/` and `ms0:/` paths both need testing. Slow random I/O can make open/page operations feel broken.

Mitigation:

- Sequential reads.
- Small metadata/layout caches.
- Atomic cache writes.
- Cache invalidation by book size/mtime/id.
- Never require cache for correctness.

### Licensing

Some attractive PSP libraries have licenses that may constrain the project.

Examples:

- OSLib: GPL-2.0.
- libintraFont: CC BY-SA 3.0.
- MuPDF: AGPL/commercial.
- Fonts: must be audited separately.

Mitigation:

- Prefer permissive/Zlib/MIT/BSD dependencies for v1.
- Generate license bundle with PSPDEV tools.
- Track dependency list in repo.

Decision: `glyph` is MIT-licensed and v1 should use MIT-compatible permissive dependencies. GPL/AGPL dependencies are out of scope unless explicitly approved later.

## Open Research Items

### R1: Real SDL2 Performance On PSP Go

Build a page rendering prototype:

- 480x272 text page.
- TTF render/glyph cache.
- Page turn.
- Theme switch.

Measure:

- page render time.
- memory usage.
- input latency.
- PSP Go behavior.

Exit criteria:

- If cached page turns are responsive and chapter layout is acceptable, stay SDL2.
- If not, investigate native GU text rendering.

### R2: EPUB Parser Library Choice

Compare:

- PSPDEV `libzip` or `minizip`.
- bundled `miniz`.
- `expat`.
- `yxml`.
- `pugixml`.

Criteria:

- PSP build friction.
- binary size.
- memory use.
- error handling.
- host test ergonomics.

### R3: Font Choice

Find bundled font with license suitable for distribution.

Candidates:

- Atkinson Hyperlegible or Atkinson Hyperlegible Next under SIL OFL.
- DejaVu Sans/Serif as fallback.
- Liberation fonts.
- Noto subset, but full Noto is too large.

Need test:

- readability on PSP screen.
- glyph coverage.
- file size.
- render speed.
- exact upstream package and license.

### R4: Custom Firmware Baseline

Decision: custom firmware is assumed for v1. The user's PSP Go currently runs 6.61 PRO-C. Unmodded/official firmware support is not a project goal.

Implication:

- Do not spend implementation time on encrypted PRX/signing support.
- Release docs should clearly state CFW requirement.
- If firmware-specific bugs appear, test whether ARK-4 or another current CFW changes behavior.

### R5: Host Build Scope

Approved. Invest early in a host GUI/CLI/test harness for parser/layout tests, even if PSP UI remains separate.

### R6: Existing Engine Spike

Only if custom layout proves too slow to build:

- CoolReader/crengine port review.
- MuPDF PSP feasibility/license review.

Likely outcome: custom remains better for license/size/control.

### R7: Agent-Driven PPSSPP Harness

Approved. Research result: feasible enough to plan. PPSSPP supports command-line launch, config overrides through `--appendconfig`, and a WebSocket debugger API for logs, status, input, screenshots, memory reads, breakpoints, and GPU stats.

Need implementation spike:

- Install or build `PPSSPPSDL`/`PPSSPPHeadless` in the agent environment.
- Generate isolated PPSSPP memstick/config directory.
- Enable remote debugger via config.
- Launch `EBOOT.PBP`.
- Connect WebSocket client.
- Send controller input.
- Capture screenshot via `gpu.buffer.screenshot`.
- Collect logs/events.
- Kill emulator on timeout.

See [07-emulator-agent-harness.md](07-emulator-agent-harness.md).

## Risk Matrix

| Risk | Probability | Impact | Current stance |
|---|---|---:|---|
| PPSSPP differs from PSP Go | Medium | High | Automate PPSSPP, sign off on hardware |
| 64 MB memory still tight | Medium | High | Bound caches and test with caps |
| EPUB subset too narrow | High | Medium | Be explicit; expand with test books |
| Unicode beyond Latin needed | Unknown | High | Ask user; defer advanced shaping |
| SDL2 too slow | Medium | Medium | Prototype early |
| License issue | Low | High | MIT project; MIT-compatible permissive dependencies only for v1 |
| Slow storage hurts UX | Medium | Medium | Cache carefully on `ef0:/` and `ms0:/` |

## Sources

- PSP hardware: https://www.psdevwiki.com/psp/Hardware
- PSP model matrix: https://www.psdevwiki.com/psp/SKU_Models
- PSPDEV package index: https://pspdev.github.io/psp-packages/
- PSPDEV debugging: https://pspdev.github.io/debugging.html
- W3C EPUB 3.3 core: https://w3c.github.io/epub-specs/epub33/core/
- W3C EPUB Reading Systems 3.3: https://w3c.github.io/epub-specs/epub33/rs/
- MuPDF docs/license entry point: https://mupdf.readthedocs.io/en/1.26.1/
- Bookr PSP prior art: https://www.gamebrew.org/wiki/Bookr_PSP_by_gtzampanakis
