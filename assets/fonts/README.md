# Font Assets

glyph bundles Atkinson Hyperlegible Next as the primary EPUB reading font.
DejaVu remains the fallback font family for coverage or renderer compatibility
gaps.

## Bundled Fonts

- Family: Atkinson Hyperlegible Next
- Format: static and variable TrueType fonts
- Variable axis: `wght` 200-800
- License: SIL Open Font License 1.1, kept in `OFL.txt`
- Copyright: Copyright 2020-2024 The Atkinson Hyperlegible Next Project Authors

Atkinson Hyperlegible Next was selected over the original Atkinson Hyperlegible
family because Braille Institute recommends Next for everyday readers, and it
adds more weights and broader language coverage while staying small enough for
the PSP asset budget.

## Source

Upstream project:
https://github.com/googlefonts/atkinson-hyperlegible-next

Pinned upstream commit:
`7925f50f649b3813257faf2f4c0b381011f434f1`

The PSP runtime prefers the static Regular TTF because PSP FreeType/SDL_ttf
renders it more predictably than the variable font build. Variable fonts remain
bundled as a fallback and for later typography experiments.

Bundled files:

| Local file | Upstream file | Size | SHA-256 |
| --- | --- | ---: | --- |
| `AtkinsonHyperlegibleNext-Regular.ttf` | `fonts/ttf/AtkinsonHyperlegibleNext-Regular.ttf` | 65,068 bytes | `88ed5c31a71584c7772963b02d04bef1eb7e3d2e9c8b9cb204339b1f82cf432c` |
| `AtkinsonHyperlegibleNext-Italic.ttf` | `fonts/ttf/AtkinsonHyperlegibleNext-Italic.ttf` | 69,812 bytes | `b7f8f03ceb28ebadb2de2332b2e869a00e8dbfb5c8e00fdbb06385770c8227e7` |
| `AtkinsonHyperlegibleNext[wght].ttf` | `fonts/variable/AtkinsonHyperlegibleNext[wght].ttf` | 114,552 bytes | `5a455d1cfa099b601ab70751bb9673e8fe1854dc4500c80e1a220d0d75e31745` |
| `AtkinsonHyperlegibleNext-Italic[wght].ttf` | `fonts/variable/AtkinsonHyperlegibleNext-Italic[wght].ttf` | 123,916 bytes | `ce9cffed32742ad2d9238c561a93220385e5934cdc02b8eb4097a50efa957dc6` |
| `OFL.txt` | `OFL.txt` | 4,431 bytes | `aca6a428580965d2297d1b718042dd427c2a9443ece3b0d02d758e161e0c4030` |

The Google Fonts collection also maps Atkinson Hyperlegible Next to this
upstream repository and commit in its `ofl/atkinsonhyperlegiblenext/METADATA.pb`
entry.

## License Notes

The project code may remain MIT licensed, but these font files are not MIT
licensed. They are redistributed under SIL OFL 1.1 as the font-specific
license exception. Keep `OFL.txt` next to the font binaries in source and
redistribution packages.

The OFL permits bundling, use, study, copying, merging, embedding,
redistribution, and modification under its terms. Modified versions must follow
the OFL requirements, including the reserved font name rules.
