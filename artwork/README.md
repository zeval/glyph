# glyph artwork

This folder contains original project artwork source material for `glyph`.

## Logo Source

- Source: `logo-concept.html`
- Format: static HTML/CSS with inline SVG.
- Direction: large-pixel lowercase `g`, dark reader UI base, blue/cyan signal
  color, centered silhouette, and a descender that stays away from the icon
  border.

## Exported Assets

- `../assets/branding/glyph-mark.svg`: standalone icon source.
- `../assets/branding/glyph-mark.png`: PNG icon export.
- `../assets/branding/glyph-wordmark.svg`: transparent README wordmark source.
- `../assets/branding/glyph-wordmark.png`: PNG wordmark export.

## Palette

- Reader black: `#0c0e10`
- Panel dark: `#181c20`
- Glyph cyan: `#62a4a8`
- Edge light: `#7ee7f2`
- Reader text: `#e8e6de`

## Export Notes

Use `glyph-mark.svg` as the canonical standalone icon and
`glyph-wordmark.svg` as the README/public presentation asset. The wordmark keeps
the icon tile plus the off-white lowercase `glyph` text from the HTML concept,
without a surrounding banner frame. Useful raster target sizes are 512, 256,
128, 64, 48, 32, and 16 px. PSP-facing exports should be checked on dark
launcher backgrounds and in PPSSPP before being promoted to runtime assets.

## Licensing

The logo concept is original project artwork created for `glyph`. Unless the
project owner chooses a separate art license later, treat it as covered by the
repository's MIT license. No third-party images, fonts, or network assets are
used.
