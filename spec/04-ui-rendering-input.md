# Topic: UI, Rendering, Input

## Decision

Use SDL2 + SDL2_image + SDL2_ttf for the first implementation. Keep renderer/input abstractions narrow so a native PSP backend can replace hot paths later.

This is the most pragmatic route because `glyph` needs a lot of product behavior before it needs maximum graphics throughput.

## UI Principles For PSP

- Visual style: quiet utility. Dense but readable, restrained colors, minimal chrome, optimized for reading.
- Default reader theme: dark background with off-white text.
- First screen should be the library/file browser, not a landing page.
- Reader screen should dedicate most pixels to text.
- UI must be legible at 480x272.
- No tiny controls.
- No mouse/touch assumptions.
- PSP Go collapsed reading must work with L/R only.
- Minimize modal complexity.
- Prefer pages, lists, and menus over freeform panels.
- Settings must be reachable with Start and navigable by D-pad.

## Screens

### File Browser / Library

Purpose: choose book.

Elements:

- Path row: `ms0:/...` or `ef0:/...`.
- Scrollable list.
- File/folder icon.
- EPUB title/author if cached, filename otherwise.
- Recent/open marker.
- Footer status: storage, sort, selected item count maybe.

Controls:

- D-pad/analog: move.
- Cross: open folder/book.
- Circle: parent/back.
- Triangle: sort/filter.
- Start: app settings.

### Reader

Purpose: read with minimal chrome.

Elements:

- Text area.
- Optional top/bottom thin status line.
- Progress: percent or chapter/page.
- Battery/time optional.
- Bookmark indicator optional.

Controls:

- R click: scroll down within the current page; jump to next page at the page edge.
- L click: scroll up within the current page; jump to previous page at the page edge.
- R/L hold: no action in v1.
- D-pad right/left: next/previous page.
- D-pad up/down: scroll line/paragraph in scroll mode.
- Triangle: table of contents.
- Square: bookmark toggle.
- Start: reader menu/settings.
- Select: status overlay/details.
- Circle: back to library/menu.

### Table Of Contents

Purpose: jump to chapter.

Elements:

- Nested but flattened list with indentation.
- Current location marker.

Controls:

- D-pad/analog: move.
- Cross: jump.
- Circle/Triangle: return to reader.

### Reader Settings

Settings:

- Font size.
- Line spacing.
- Margin preset.
- Theme.
- Page mode vs scroll mode preference.
- Page turn direction if desired.
- CPU/performance mode optional.

Controls:

- D-pad: navigate/adjust.
- Cross: select.
- Circle: close.

### Error Screen

Purpose: explain unsupported/corrupt files.

Should show:

- Short reason.
- File path/name.
- Error code/details behind Select if needed.

## Rendering Backend: SDL2 First

Use:

- `SDL2` for window/renderer/input portability.
- `SDL2_image` for JPEG/PNG/GIF.
- `SDL2_ttf` for font rendering.

Benefits:

- Same app core can run on Linux/macOS/Windows host for development.
- PSPDEV has packages and examples.
- Easier to bring up UI quickly.

Risks:

- SDL2 may be slower than native `libGU`.
- Text rendering can be expensive if every glyph/string is re-rendered each page turn.

Mitigation:

- Cache rendered glyphs or line textures.
- Render static page only when content changes.
- Avoid per-frame text layout.
- Add native GU backend later if profiling proves need.

## Native PSP Backend: Later Fast Path

Potential native APIs:

- `libGU` for 2D sprites/rectangles/textures.
- `pspctrl` for controller input.
- `sceIo*` for file browser.
- `libpspvram` for VRAM allocation.
- `stb_image` for lightweight direct image decode.
- FreeType directly for glyph rasterization.

Reasons to add:

- Lower overhead.
- Better VRAM control.
- More predictable performance on real hardware.

Reasons not first:

- Slower product iteration.
- More PSP-specific rendering code.
- Need manual texture alignment/cache management.

## File Browser I/O

Use native `sceIo*` on PSP:

- `sceIoDopen`
- `sceIoDread`
- `sceIoDclose`
- `sceIoGetstat`

Wrap behind `FileSystem` interface with host implementation for tests.

## Font Options

### SDL2_ttf / FreeType

Recommended MVP.

Pros:

- Portable.
- Good quality.
- Packaged by PSPDEV.
- Easy desktop parity.

Cons:

- Need bundled font license.
- Must manage glyph cache for performance.

### libintraFont

Uses PSP internal PGF/BWFON fonts.

Pros:

- Native PSP look.
- No bundled TTF needed.

Cons:

- License/attribution/share-alike concern.
- PSP-specific.
- Less useful for host build.

### stb_truetype

Good fallback if SDL2_ttf is too heavy.

Pros:

- Single header.
- Permissive/public domain or MIT.

Cons:

- Less complete than FreeType.
- No shaping.

## UI Library Options

### Custom Immediate UI

Recommended for product UI.

`glyph` UI is simple: lists, reader view, TOC, settings. Custom UI avoids large dependencies and lets layout fit 480x272.

### Dear ImGui

Useful for debug tools, not recommended as final reader UI. PSPDEV packages ImGui, but product UX would need substantial styling/input work.

### OSLib

Avoid for v1. It is a PSP-focused 2D helper library, but GPL-2.0 conflicts with the MIT/permissive dependency policy.

## Input Abstraction

Internal actions:

```text
MoveUp
MoveDown
MoveLeft
MoveRight
PagePrev
PageNext
ScrollUpHeld
ScrollDownHeld
Accept
Back
OpenToc
ToggleBookmark
OpenMenu
ShowStatus
Quit
```

Map PSP buttons to actions at platform layer. Host build can map keyboard/gamepad to same actions.

L/R should be edge-triggered only for v1. Do not add hold-repeat behavior unless the control model is revisited.

## Sources

- PSPDEV basic programs: https://pspdev.github.io/basic_programs.html
- PSPSDK GU docs: https://pspdev.github.io/pspsdk/group__GU.html
- PSPSDK controller docs: https://pspdev.github.io/pspsdk/group__Ctrl.html
- PSPSDK file manager header docs: https://pspdev.github.io/pspsdk/pspiofilemgr_8h.html
- PSPDEV package index: https://pspdev.github.io/psp-packages/
- SDL PSP README: https://wiki.libsdl.org/SDL2/README-psp
- SDL2 package: https://pspdev.github.io/psp-packages/sdl2.html
- SDL2_image package: https://pspdev.github.io/psp-packages/sdl2-image.html
- SDL2_ttf package: https://pspdev.github.io/psp-packages/sdl2-ttf.html
- FreeType package: https://pspdev.github.io/psp-packages/freetype2.html
- libintraFont package: https://pspdev.github.io/psp-packages/libintrafont.html
- ImGui PSP package: https://pspdev.github.io/psp-packages/imgui.html
