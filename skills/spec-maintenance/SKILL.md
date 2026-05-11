---
name: spec-maintenance
description: Update glyph specs and decision records cleanly. Use when changing project scope or architecture decisions.
---

# Spec Maintenance Skill

## Rules

- Update `spec/SPEC.md` for current project direction.
- Update `spec/QUESTIONS.md` when a user decision is resolved.
- Keep historical research in topic files, but make current decisions easy to find.
- Milestone language is acceptable in `spec/`; avoid leaking it into product code/docs/tools.
- Include source links for researched claims.
- Keep open questions concrete and actionable.

## Decision Format

Prefer:

```markdown
Decision: short statement.

Implications:

- Concrete effect.
- Concrete effect.
```

Avoid vague notes like "maybe later" without a trigger or owner.
