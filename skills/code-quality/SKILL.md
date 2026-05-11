---
name: code-quality
description: Keep glyph code clean, portable, and maintainable. Use before editing runtime/build/tooling code.
---

# Code Quality Skill

## General

- Keep implementation scoped to the requested behavior.
- Prefer simple modules and explicit ownership.
- Avoid temporary milestone labels in product code, README, tools, or runtime assets.
- Planning/milestone language belongs in `spec/` only.
- Add TODOs only for concrete deferred work, not vague cleanup.
- Keep code ASCII unless a file already requires otherwise.

## C++

- C++17.
- No exceptions.
- No RTTI.
- RAII is encouraged.
- Use explicit error/Result values for fallible code.
- Avoid hidden heap-heavy abstractions in PSP hot paths.
- Keep PSP-specific code behind platform seams where practical.

## Frontend/UI

- Quiet utility style.
- 480x272 PSP screen is the constraint.
- Dark default theme.
- Collapsed PSP Go reading must work with L/R:
  - click: previous/next page
  - hold: scroll up/down

## Tooling

- Linux/Ubuntu Server and macOS are first-class.
- Prefer CMake underneath and top-level Make targets for user commands.
- Prefer Python stdlib for helper scripts.
- Keep PPSSPP harness optional unless emulator is installed.

## Licenses

- Project code is MIT.
- Dependencies must be MIT-compatible permissive unless explicitly approved.
- Bundled fonts may use SIL OFL after audit.
- Keep third-party licenses next to assets where practical.
