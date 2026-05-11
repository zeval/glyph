# Topic: EPUB Format And Rendering

## Decision

Build a constrained native EPUB reader. Do not port a full browser-like renderer for v1.

Long-term ambition: broad EPUB compatibility.

V1 target:

- DRM-free EPUB.
- EPUB 2 first.
- Simple reflowable EPUB 3 second.
- Mostly novels/text.
- Latin script.
- XHTML/XML-first parsing.
- Small CSS subset.
- Incremental chapter layout.

## EPUB Anatomy

An EPUB is a ZIP/OCF container. Important files:

- `mimetype`: identifies EPUB type.
- `META-INF/container.xml`: points to package document(s).
- `.opf` package document: metadata, manifest, spine, resources.
- EPUB 2 NCX: table of contents.
- EPUB 3 navigation document: XHTML `nav` table of contents.
- XHTML/SVG/image/font/CSS resources listed in manifest.

The `.opf` spine is the default reading order. `glyph` should use it as the source of sequential reading.

## MVP EPUB Support Matrix

| Feature | MVP |
|---|---|
| EPUB 2 OPF | Yes |
| EPUB 2 NCX | Yes |
| EPUB 3 OPF | Yes, simple cases |
| EPUB 3 navigation document | Yes, simple TOC |
| Reflowable XHTML | Yes |
| UTF-8 | Yes |
| XHTML malformed as HTML | Best-effort later; strict/error in MVP |
| CSS | Tiny subset |
| PNG/JPEG/GIF | Yes, bounded |
| SVG | No, except optional fallback/image later |
| Fixed layout | No |
| JavaScript/forms | No |
| DRM/encryption | No |
| Remote resources | No |
| Audio/video/media overlays | No |
| MathML | No |
| Large tables | Degrade or error |
| Embedded fonts | Post-MVP |

## Parsing Pipeline

### 1. Container Open

- Verify ZIP opens.
- Read `mimetype` if present.
- Read `META-INF/container.xml`.
- Pick the first package document rootfile by default.

### 2. Package Parse

Extract:

- EPUB version.
- Metadata: title, creator, language, identifier.
- Manifest: id, href, media type, properties.
- Spine: ordered itemrefs, linear flag, page progression direction.
- Cover resource if declared.

### 3. Navigation Parse

EPUB 2:

- Parse NCX nav points.
- Keep label, target href, nesting.

EPUB 3:

- Parse XHTML navigation document.
- Extract `nav epub:type="toc"` or equivalent.
- Keep link labels/targets.

### 4. Resource Resolution

- Resolve relative paths against OPF location.
- Normalize paths inside ZIP.
- Reject absolute/remote resource loads in MVP.
- Handle fragment IDs for intra-book links.

### 5. XHTML Parse

Start XML-first. Supported elements:

- `html`, `body`, `section`, `article`, `nav`
- `p`, `br`, `div`
- `h1`-`h6`
- `em`, `i`, `strong`, `b`
- `blockquote`
- `ul`, `ol`, `li`
- `a`
- `img`
- `span`

Discard or flatten unknown inline elements. Treat unknown block elements as `div` when safe.

### 6. CSS Subset

MVP CSS properties:

- `display: block | inline | none`
- `margin-top`, `margin-bottom`, `margin-left`, `margin-right`
- `text-align`
- `font-style`
- `font-weight`
- `font-size` with limited units
- `color` if theme handling remains simple

Ignore:

- floats
- positioning
- flex/grid
- transforms
- generated content
- media queries
- animations
- complex selectors

Selector subset:

- element
- `.class`
- `#id`
- simple descendant if cheap

User settings should override book CSS for font size, line spacing, and theme.

## Layout Model

Convert XHTML to small internal blocks:

```text
Document
  Block[]
    kind: paragraph/heading/list_item/image/quote/spacer
    Inline[]
      text run
      style flags
      link target
      image ref
```

Layout:

- Decode UTF-8 to codepoints.
- Normalize whitespace per HTML-ish rules.
- Apply basic line breaking.
- Shape/render with SDL2_ttf/FreeType first.
- Build lines into pages for 480x272 minus margins.
- Track anchors and source positions.

Pagination:

- Current chapter first.
- Optional previous/next chapter prefetch if memory allows.
- Cache page maps by book id + viewport + font settings.

## Font/Text Strategy

MVP:

- Bundle one permissive Latin font or use a PSPDEV-packaged test font while prototyping.
- M0 font choice: Atkinson Hyperlegible or Atkinson Hyperlegible Next, licensed under SIL OFL after exact package audit.
- Use SDL2_ttf/FreeType.
- Support UTF-8 decode plus basic Latin accents and punctuation.
- Fallback unsupported glyphs to tofu box.
- Ignore embedded EPUB fonts by default.

Post-MVP:

- Font fallback.
- CJK font option if memory permits.
- HarfBuzz shaping.
- FriBidi for RTL.
- Better line breaking/hyphenation.

Avoid embedded EPUB fonts in MVP because they can be large and create licensing/memory complexity.

## Images

MVP:

- Decode PNG/JPEG/GIF first frame through SDL2_image.
- Bound decoded dimensions to screen size before keeping.
- Show oversized image as scaled block.
- Skip image with placeholder if decode fails or memory budget exceeded.

Rules:

- Never decode multiple large images at once.
- Do not retain all chapter images.
- Cover thumbnails should be generated lazily and cached to small size.

## Existing Renderer Options

### MuPDF

Supports EPUB and reflowable layout, but it is large, AGPL/commercial, and not PSP-oriented. Useful reference, not recommended for v1.

### Readium SDK

Not recommended. It assumes browser/WebKit-style rendering and JavaScript/pagination architecture.

### CoolReader/crengine

Closest conceptual existing engine. Risks: GPLv2, old codebase, likely porting effort, unknown PSP fit. Could be a later research spike if custom layout proves too expensive.

### Bookr PSP

Existing PSP document reader, but not a direct EPUB answer. It supports PDF/text/HTML/DJVU/CHM/PalmDoc in a fork and has Unicode architecture limitations. Useful as prior-art warning: text encoding and document architecture matter.

## Error Handling

Reader should produce clear user-facing errors for:

- Not an EPUB/ZIP.
- Missing `container.xml`.
- Missing OPF.
- Empty spine.
- Unsupported encrypted/DRM book.
- Chapter too large.
- Image too large.
- Malformed XML if parser cannot recover.

Do not crash or hang on unsupported books.

V1 policy: graceful degradation. Open what can be opened, skip unsupported pieces, and show clear warnings/errors when content is omitted or a chapter/book cannot be rendered.

## Test Corpus

Create fixtures:

- Minimal EPUB 2 single chapter.
- Minimal EPUB 2 multi-chapter with NCX.
- Minimal EPUB 3 nav.
- Book with UTF-8 punctuation/accented Latin.
- Book with long chapter.
- Book with image.
- Book with unsupported fixed-layout marker.
- Book with missing optional metadata.
- Book with malformed XHTML.

Use EPUBCheck on fixtures during development where possible.

## Sources

- W3C EPUB 3.3 overview: https://www.w3.org/TR/epub-overview-33/
- W3C EPUB 3.3 core: https://w3c.github.io/epub-specs/epub33/core/
- W3C EPUB Reading Systems 3.3: https://w3c.github.io/epub-specs/epub33/rs/
- IDPF OPF 2.0: https://idpf.org/epub/20/spec/OPF_2.0_final_spec.html
- IDPF OPS 2.0: https://idpf.org/epub/20/spec/OPS_2.0_final_spec.html
- PSPDEV package index: https://pspdev.github.io/psp-packages/
- MuPDF docs: https://mupdf.readthedocs.io/en/1.26.1/
- FBReader status/SDK notes: https://fbreader.org/en
- Bookr PSP: https://www.gamebrew.org/wiki/Bookr_PSP_by_gtzampanakis
- FreeType: https://www.freetype.org/
- HarfBuzz: https://harfbuzz.github.io/
- stb: https://github.com/nothings/stb
