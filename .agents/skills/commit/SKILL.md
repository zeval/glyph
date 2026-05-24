---
name: commit
description: Commit changes professionally using concise Conventional Commits. Use when preparing or reviewing commits.
---

# Commit Skill

Goal: small, readable commits with clear intent.

## Rules

- Use Conventional Commits: `type(scope): summary`.
- Keep summary under ~72 chars when practical.
- Use imperative mood: `add`, `fix`, `record`, `remove`.
- No trailing period.
- One logical change per commit.
- Do not mix generated build output with source changes.
- Do not include unrelated user edits.

## Types

- `feat`: user-visible feature or new capability.
- `fix`: bug fix.
- `docs`: docs/spec/readme only.
- `test`: tests or fixtures.
- `refactor`: behavior-preserving code change.
- `chore`: maintenance that does not alter runtime behavior.
- `build`: build system/dependency changes.
- `ci`: CI configuration.

## Examples

```text
feat(ui): add PSP Go reader controls
fix(harness): detect snap PPSSPP command
docs(spec): record MIT dependency policy
build(cmake): add host SDL2 target
test(core): add EPUB metadata fixture
```

## Before Commit

Run relevant checks:

```sh
make check-format
make test
```

If a check cannot run, say why in the handoff.
