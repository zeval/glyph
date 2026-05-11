# Topic: PSP Platform Constraints

## Decision

Design `glyph` for PSP Go first. This lifts the PSP-1000 32 MB RAM restriction and makes `ef0:/` internal storage the default book/cache location.

The baseline is now a 64 MB PSP-class device with a 480x272 3.8 inch display, no keyboard, no touch, and limited VRAM. PSP-1000 support is explicitly out of v1 scope unless reintroduced later.

## Hardware Baseline

### CPU

- Allegrex/MIPS R4000-class CPU.
- Official PSP-2000 specs list system clock frequency `1-333 MHz`.
- Use lower/normal clock while reading.
- Raise to 333 MHz for parsing, image decode, and repagination if needed.

### Memory

- PSP Go: 64 MB RAM.
- PSP-2000/3000/E1000: also 64 MB RAM and plausible later compatibility targets.
- PSP-1000: 32 MB RAM, not a v1 target.

Recommended MVP working target:

- Keep resident app/parser/layout/glyph/image state under 32-40 MB where practical.
- Leave several MB free for SDL2/PSPSDK, framebuffers, decode scratch, and fragmentation.
- Never keep whole book uncompressed in RAM.
- Load current chapter/spine item at minimum; prefetch next/previous chapter only after measuring memory.
- Decode one large image at a time.
- Make caches disposable and bounded.

### Display

- 480x272.
- 16:9 TFT LCD.
- 3.8 inch on PSP Go.

Implications:

- Use large, readable fonts.
- Minimize dense UI.
- Avoid multi-column reading.
- Avoid decorative chrome.
- Keep line length comfortable at about 42-65 Latin characters depending font size.
- Prefer page-turn UX over continuous scrolling.

### VRAM / Graphics Memory

PSP graphics memory is limited. A 480x272 32-bit framebuffer is about 522 KB; double buffering consumes about 1 MB. Texture/glyph atlases must stay small.

Recommendations:

- Prefer 16-bit surfaces/framebuffers if text quality remains acceptable.
- Use one or two glyph atlas pages, not many.
- Downsample images to viewport or smaller before keeping them.
- Avoid retaining cover + current page image + next image + large glyph caches simultaneously.

### Storage

- PSP Go has 16 GB internal flash storage exposed as `ef0:/`.
- PSP Go also supports Memory Stick Micro, exposed as `ms0:/` when present.

Path assumptions:

- `ef0:/` is primary app data/book/cache root for PSP Go.
- `ms0:/` is optional removable storage.

Storage constraints:

- Internal storage should be more predictable than random Memory Stick adapters, but still use bounded caches.
- Memory Stick Micro/adapters can be slow.
- Random small reads can be costly.
- Cache writes should be atomic where possible: write temp, then rename.
- Cache corruption must not prevent opening the source EPUB.

### Input

Available controls:

- D-pad.
- Action buttons: Triangle, Circle, Cross, Square.
- L/R shoulder buttons.
- Analog nub.
- Start/Select/Home.
- Volume/display/power controls.

Implications:

- No required typing.
- No touchscreen gestures.
- File browser and TOC must be controller-native.
- Search is post-MVP unless an on-screen keyboard is added.

### Battery

Text reading can be efficient, but screen brightness and CPU clock dominate perceived battery life.

Recommendations:

- Idle at normal/lower CPU.
- Avoid busy render loops when page is static.
- Re-render only on input, animations, or status changes.
- Offer simple brightness/theme guidance in docs, not inside UI.

## Model Compatibility

| Model | RAM | Notes |
|---|---:|---|
| PSP Go | 64 MB | Primary v1 target. Smaller 3.8 inch screen, `ef0:/` internal storage. |
| PSP-2000 | 64 MB | Later compatibility target. Different storage default, 4.3 inch screen. |
| PSP-3000 | 64 MB | Later compatibility target. Common model, 4.3 inch screen. |
| PSP-E1000 | 64 MB | Later compatibility target if desired. No Wi-Fi, mono audio. |
| PSP-1000 | 32 MB | Out of v1 scope. Would require stricter memory budgets. |

## Concrete Budgets For MVP

These are starting budgets, not proven measurements.

| Area | Budget |
|---|---:|
| Main executable/static data | under 4 MB preferred |
| Current decompressed XHTML chapter | under 4 MB preferred; reject/stream huge chapters |
| Parsed document model | under 6 MB |
| Layout page map current chapter | under 4 MB |
| Glyph atlas/cache | 1-4 MB |
| Image working buffer | one screen-sized image plus decode scratch |
| Metadata/book cache in RAM | under 2 MB |
| Free headroom | keep several MB free to avoid fragmentation crashes |

## Constraints For EPUB Design

- No full-book DOM.
- No full-browser CSS cascade.
- No full-book pagination on open unless cache/build time is acceptable.
- No preloading all images.
- No embedded font loading by default in v1.
- Avoid high-resolution cover thumbnails in library view.

## Sources

- PSP Developer Wiki hardware: https://www.psdevwiki.com/psp/Hardware
- PSP Developer Wiki model matrix: https://www.psdevwiki.com/psp/SKU_Models
- Sony PSP-2000 official specs PDF: https://www.sony.com/en/SonyInfo/IR/news/qfhh7c00000d2yu7-att/psp.pdf
- PPSSPP CPU notes: https://dev.ppsspp.org/docs/psp-hardware/cpu/
- PSPSDK docs: https://pspdev.github.io/pspsdk/
