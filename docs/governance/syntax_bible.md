# Syntax Bible

This is the documentation and coding language standard for Stygian docs.

## Document Style

- Use explicit contracts, not vague prose.
- State invariants and failure modes.
- Prefer short sections with concrete bullets.
- Keep naming aligned with headers and source symbols.

## API Documentation Rules

Each API page must include:
- purpose
- ownership and lifetime rules
- deterministic behavior rules
- error and edge-case behavior
- perf implications

## Runtime Terminology

Use these terms consistently:
- collect, commit, evaluate, render, skip
- mutation, repaint, eval-only, dirty scope, replay
- source tag, reason flags, winner record

## Prohibited Documentation Patterns

- claiming behavior not backed by source
- mixing historical context into active contract pages
- hiding critical rules in examples only
