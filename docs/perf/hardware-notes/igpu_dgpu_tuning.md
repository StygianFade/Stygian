# iGPU vs dGPU Notes

## iGPU Guidance

- prioritize strict dirty range uploads
- keep overlay cadence conservative (30 Hz default)
- avoid broad scope invalidation under scroll/drag
- ensure idle settles quickly after burst

## dGPU Guidance

- still enforce DDI contracts; do not rely on hardware brute force
- keep parity behavior with iGPU paths
- use aggressive profile to catch regressions early

## Shared Rules

- same cause-correct frame policy on both backends
- same AP metric contracts
- same command commit semantics
