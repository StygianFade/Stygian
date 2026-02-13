# Performance Gate Policy

Gate runner:
- `compile/windows/run_perf_gates.bat`
- implementation: `tests/run_perf_gates.ps1`

## Profiles

- `baseline`
- `aggressive`

## Backend Modes

- `-Backend gl`
- `-Backend vk`
- `-Backend both`

## Device Class

- `auto`, `igpu`, `dgpu`

## Output Contracts

Per scenario line:
- `PERF_HEALTH ...`

Backend summary line:
- `PERF_HEALTH_SUMMARY ...`

Scenario feed line:
- `PERFCASE ...`

## Pass Criteria

- no `BAD` in selected profile
- optional strict mode: `-FailOnWarn`

## Recommended CI Usage

- run `gl` and `vk` gates separately for stability
- run `both` for convenience smoke checks
