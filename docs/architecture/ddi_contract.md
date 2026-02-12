# DDI Contract

This is the hard contract for Stygian correctness.

## Repaint Causes

Allowed repaint causes:
- state mutation
- timer or animation tick
- async completion (decode/load)
- explicit forced repaint

No other source can trigger render submit.

## Input Rules

- Pointer-only input must not directly force redraw.
- Pointer edges may request eval-only work for focus/active transitions.
- Scroll only mutates when clamped model value actually changes.

## Scope Rules

- Dirty scopes rebuild.
- Clean scopes replay from cached ranges.
- Scope invalidation is the unit of render causality.

## Upload Rules

- Upload only dirty ranges per SoA buffer/chunk.
- Dense fallback is allowed only when partial range precision is unavailable.

## Idle Rules

- No busy loop while no repaint is pending.
- Wait timeout must use repaint scheduler (`next_repaint_wait_ms`).

## Observability Rules

Every rendered frame should be attributable by reason flags and source tags:
- mutation
- timer/animation
- async
- forced

Per-scope dirty provenance must be queryable.
