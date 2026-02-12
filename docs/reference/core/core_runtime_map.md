# Core Runtime Map

This complements `stygian.h` with practical ownership mapping.

## Public vs Internal

- Public contracts live in `include/stygian.h`.
- Internal storage and execution details live in `src/stygian_internal.h`.

## Ownership Summary

- Context owns runtime memory and scheduler state.
- AP owns GPU objects and backend-specific submission path.
- Window owns native handles and platform event ingestion.

## Contract Boundary

- Core and widgets may request repaint/eval intent.
- Only AP submits GPU work.
- Command producers never mutate SoA directly.
