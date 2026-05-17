# Contributing

Thanks for helping improve `glyph`. The project is early, so small, focused
patches are easier to review and test than broad rewrites.

## Scope

`glyph` is a PSP Go-first EPUB reader for DRM-free books. Keep changes aligned
with that target unless a proposal has been discussed first.

Good first areas:

- EPUB parsing and graceful fallback for common novel/text books.
- Reader UX on the PSP Go's 480x272 screen.
- Progress, settings, bookmarks, and library behavior.
- Host tests for parser, layout, storage, and regression cases.
- PSP build, packaging, and PPSSPP harness reliability.

Out of scope unless explicitly approved:

- DRM support.
- GPL or AGPL dependencies.
- Browser-grade HTML/CSS, JavaScript, audio/video, or fixed-layout EPUB.
- Network sync/catalog features.

## Development

Install host dependencies listed in `README.md`, then run:

```sh
make check-deps
make host
make test
make check-format
```

For PSP-facing changes, also run:

```sh
export PSPDEV="$HOME/pspdev"
export PATH="$PATH:$PSPDEV/bin"
make package-psp
make run-ppsspp
```

`make run-ppsspp` only performs a detect-mode smoke unless a runnable EBOOT and
PPSSPP environment are available.

## Code Style

- C++17, no exceptions, no RTTI.
- Prefer RAII and explicit result/error values.
- Keep PSP memory and CPU limits in mind.
- Use `clang-format` through `make format` or `make check-format`.
- Do not commit generated build output or private EPUB files.

## Commits

Use concise Conventional Commits:

```text
type(scope): summary
```

Examples:

```text
feat(reader): add chapter jump overlay
fix(epub): skip unreadable spine items
docs(readme): document PSP Go install path
test(layout): cover long-word wrapping
```

## Pull Requests

Before opening a pull request:

- Rebase or merge current `main`.
- Run the checks that apply to your change.
- Explain user-visible behavior and PSP testing coverage.
- Note any known limitations or follow-up work.
