# Contributing

`glyph` is a PSP Go-first EPUB reader. Keep changes small, testable, and aligned
with the current constrained-reader scope.

## Before You Start

- Read [README.md](README.md), [spec/SPEC.md](spec/SPEC.md), and
  [spec/QUESTIONS.md](spec/QUESTIONS.md).
- Check existing issues and pull requests before opening a duplicate.
- Prefer focused issues and pull requests over broad rewrites.
- Do not include copyrighted EPUB samples in issues, tests, or pull requests.
  Use minimal fixtures or public-domain books.

## Development Setup

Install the host dependencies listed in the README, then run:

```sh
make check-deps
make host
make test
make check-format
```

PSP builds require PSPDEV/PSPSDK on `PATH`:

```sh
export PSPDEV="$HOME/pspdev"
export PATH="$PATH:$PSPDEV/bin"
make psp
```

For PSP-facing changes, also run:

```sh
make package-psp
make run-ppsspp
```

## Project Constraints

- C++17.
- No exceptions and no RTTI.
- Use RAII and explicit error/result values for fallible behavior.
- Keep Linux and macOS host development working.
- Keep PSP Go storage paths and 480x272 UI constraints in mind.
- Keep PSP memory and CPU limits in mind.
- Use `clang-format` through `make format` or `make check-format`.
- Do not commit generated build output or private EPUB files.

## Dependencies And Licenses

Project code is MIT licensed. New dependencies must be compatible with the
project license and documented clearly.

Allowed dependency licenses include MIT, BSD-2-Clause, BSD-3-Clause, Zlib, ISC,
and Apache-2.0. Bundled fonts may use SIL OFL after audit. Do not add GPL or
AGPL dependencies without an explicit project decision.

## Documentation

- Keep public docs direct and current.
- Avoid short-lived planning labels and private workflow details in public docs.
- Use explicit TODOs only for intentionally deferred follow-up work with clear
  scope.
- Do not hand-edit generated build output.

## Pull Requests

Before opening a pull request:

```sh
make check-deps
make host
make test
make check-format
```

For PSP-facing changes, also run:

```sh
make psp
make package-psp
make run-ppsspp
```

If a relevant check cannot run, explain why in the pull request.

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

Common types are `feat`, `fix`, `docs`, `test`, `refactor`, `chore`, `build`,
and `ci`. Keep the summary imperative, lowercase after the type, and without a
trailing period.

## Security

Please report security issues privately. See [SECURITY.md](SECURITY.md).
