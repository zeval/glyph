# AGENTS

This repository is a PSP Go-first EPUB reader. Keep work small, testable, and professional.

## Working Rules

- Review relevant local skills in `skills/` when they match the task.
- Read `spec/SPEC.md` and `spec/QUESTIONS.md` before changing behavior.
- Keep user-facing code/docs free of temporary milestone labels or planning language.
- Use explicit TODOs only when a known refactor or follow-up is intentionally deferred. Include owner/scope when useful.
- Do not introduce GPL/AGPL dependencies. Project code is MIT; normal dependencies must be MIT-compatible permissive licenses. SIL OFL is allowed for bundled fonts after audit.
- C++ policy: C++17, no exceptions, no RTTI, RAII encouraged, explicit `Result`/error values for fallible code.
- Development must remain native-friendly on Linux/Ubuntu Server and macOS.
- Prefer portable POSIX shell and Python stdlib scripts for repo tooling.
- Do not hand-edit generated build output. `build/` is disposable.

## Build And Test

Run what applies before handing work back:

```sh
make check-deps
make host
make test
make check-format
make run-ppsspp
```

For host UI smoke on a headless server:

```sh
timeout 2s env SDL_VIDEODRIVER=dummy ./build/host/glyph
```

PSP builds require PSPDEV on `PATH`:

```sh
export PSPDEV="$HOME/pspdev"
export PATH="$PATH:$PSPDEV/bin"
make psp
```

## Commit Style

Use concise Conventional Commits:

```text
type(scope): summary
```

Examples:

```text
feat(ui): add reader shell controls
fix(harness): detect snap PPSSPP command
docs(spec): record PSP Go storage paths
chore(build): add clang-format check
```

Allowed common types: `feat`, `fix`, `docs`, `test`, `refactor`, `chore`, `build`, `ci`.

Keep summary imperative, lowercase after type, no trailing period. Commit one logical change at a time.

## Repository Shape

- `src/`: app/runtime code.
- `tests/`: host tests.
- `tools/`: developer automation.
- `assets/`: bundled runtime assets and their licenses.
- `spec/`: planning, research, and decision records. Milestone language is acceptable here.

## Agent Coordination

Kandev subtasks are allowed when write scopes are disjoint. When delegating, specify owned paths and tell subtasks not to revert unrelated edits. Integrate returned work deliberately and re-run checks.
