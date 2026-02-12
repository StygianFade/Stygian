# Frame Loop Patterns

## Cause-Correct Loop

Inputs:
- `event_mutated`
- `event_eval_requested`
- `repaint_pending`
- `pending_async`

Decision:
- render intent if `first_frame || event_mutated || repaint_pending || pending_async`
- eval-only intent if `event_eval_requested` and no render cause
- otherwise wait/continue

## Idle Wait Strategy

Use `stygian_next_repaint_wait_ms(ctx, idle_wait_ms)` for event timeout.
This prevents busy-looping and still honors timer cadence.

## Scope Invalidation

Do not invalidate all scopes on generic event mutation.
Track changed domains and invalidate only affected scope ids.

## Resize/Move Continuity

Use platform tick events (`STYGIAN_EVENT_TICK`) during move/resize modal loops.
Treat tick as repaint request, not mutation by itself.
