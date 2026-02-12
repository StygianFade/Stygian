# Review Checklist

Use this checklist for every non-trivial Stygian change.

## Correctness

- Are repaint causes explicit and valid?
- Can pointer-only paths avoid render submit?
- Are eval-only frames free of submit/draw/swap?

## Invalidation

- Are scope invalidations narrow and domain-specific?
- Are clean scope replay paths preserved?

## Performance

- Do upload bytes/ranges scale with change size?
- Is idle settling fast after interaction bursts?
- Are perf gates green for relevant backends?

## Determinism

- Are command merges deterministic under producer concurrency?
- Are conflict resolutions traceable via winner/source metadata?

## Observability

- Are `PERFCASE` and `PERF_HEALTH` signals still intact?
- Are errors routed to context error ring/callback?
