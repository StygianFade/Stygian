# DDI and DoD Non-Negotiables

*(DDI = Data-Driven Immediate)*

## DDI

- Rendering is invalidation-driven.
- Pointer-only input does not force redraw.
- Repaint causes are mutation/timer/async/forced only.
- Clean scopes replay; dirty scopes rebuild.
- Idle waits deeply with no busy loop.

## DoD/Data Layout

- SoA on hot traversal and per-frame loops.
- AoS only for cold metadata.
- Stable IDs and contiguous storage.
- No hidden object graphs driving draw decisions.

## Memory

- no raw heap in runtime hot paths
- allocator/arena/pool usage for owned runtime state
- exceptions must be explicit and justified

## Backend Parity

- GL and VK must expose equivalent semantics for:
  - upload accounting
  - gpu timing
  - eval-only no-submit path
