# Runtime Model

Stygian runtime follows a strict frame pipeline:

1. Collect
- Window/input events are collected.
- Widget/event code classifies impact (pointer-only, eval request, mutation, repaint request).
- Command buffers may be produced by multiple producers.

2. Commit
- Command producers publish into per-thread/per-producer queues.
- At frame start, core flips epoch and commits only the frozen epoch.
- Commit thread is the only SoA mutator.
- Deterministic merge order applies before mutation writes.

3. Evaluate
- Scope replay and widget state logic run.
- Frame intent selects either render frame or eval-only frame.
- Eval-only frame runs logic and stats, but no draw/submit/swap.

4. Render or Skip
- Render when there is actual work (mutation/timer/async/forced).
- Skip when all scopes are clean and no repaint is pending.

## Invariants

- Rendering is invalidation-driven, never input-driven.
- Clean scope replay must not mutate clean GPU ranges.
- Upload bytes and ranges must scale with dirty scope/chunk size.
- Idle path must deep-wait and not busy-spin.

## Frame Intent

- `STYGIAN_FRAME_RENDER`: normal render-capable frame.
- `STYGIAN_FRAME_EVAL_ONLY`: logic-only frame, no backend submit/swap.

Use eval-only to keep input/focus transitions responsive without forcing GPU work.
