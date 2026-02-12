# Profiling Workflow

## What to Trust

Primary metrics:
- build ms
- submit ms
- present ms
- gpu ms
- upload bytes/ranges
- render/eval frame counts

## What to Deprioritize

Task Manager percentages alone are not sufficient for regressions.

## Procedure

1. Run scenario in pathological suite.
2. Capture `PERFCASE` and `PERF_HEALTH` lines.
3. Compare against previous run.
4. Attribute spikes by cause and dirty scope domain.

## Fast Triage Heuristics

- High build ms + high upload bytes: invalidation too broad.
- High present ms only: compositor/present pacing issue.
- High gpu ms with low upload: overdraw/shader/draw cost.
- High eval counts but low render: input bookkeeping pressure.
