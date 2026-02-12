# Migration from Deprecated Examples

Deprecated examples were moved under `examples/deprecated/`.

## New Acceptance Apps

Use:
- `examples/text_editor_mini.c`
- `examples/calculator_mini.c`
- `examples/calendar_mini.c`
- `examples/perf_pathological_suite.c`

## Migration Rules

- Replace old loop logic with frame-intent split.
- Replace broad invalidation with domain/scope invalidation.
- Use command buffers for producer-driven mutation paths.
- Keep perf overlay optional and isolated.

## Do Not Carry Forward

- Input-driven unconditional repaint.
- Full-scene rebuild on tiny mutation.
- Busy polling with no wait timeout.
