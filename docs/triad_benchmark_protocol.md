# TRIAD Benchmark Protocol (Production Gate)

This document defines the required benchmark process, pass/fail gates, and CSV schema for TRIAD emoji decode benchmarking.

## Scope

- Target path: TRIAD v3.4+ decode (BC7 LL + BC4 index + sparse values)
- Targets:
  - dGPU class: GTX 860M or better
  - iGPU class: Intel HD 4600 or better
- Workloads:
  - Startup decode (full batch)
  - Runtime decode (paged/batched)
  - Stress behavior (cache misses, scrolling bursts)

## Current Architecture Decision

This is the currently approved direction for implementation and validation:

1. Unicode plan:
   - Astral-plane emoji/symbol assets use TRIAD compression/decode path.
   - Text shaping/rendering uses MTSDF/MSDF atlas path (not TRIAD).
2. Runtime policy:
   - TRIAD decode is used at startup predecode and on cache miss / LOD promotion.
   - Render path should hit cached atlas data in steady state.
3. Fallback ladder:
   - Primary: `v3.4a` (`BC4` index path).
   - Fallback A: `R8` index path.
   - Fallback B: existing MTSDF/MSDF atlas path.
4. Scaling target:
   - 10k+ glyph/emoji support is valid only with tiered paging + cache-hit targets.
   - Do not rely on per-glyph real-time decode in interaction loop.

### Runtime Control API

Stygian runtime policy is configured through `StygianConfig.glyph_feature_flags`.

Default profile:
- `STYGIAN_GLYPH_FEATURE_DEFAULT`

Core flags:
- `STYGIAN_GLYPH_TRIAD_PRIMARY`
- `STYGIAN_GLYPH_TRIAD_FALLBACK_R8`
- `STYGIAN_GLYPH_FALLBACK_MTSDF`
- `STYGIAN_GLYPH_PREDECODE_STARTUP`
- `STYGIAN_GLYPH_DECODE_ON_ZOOM`
- `STYGIAN_GLYPH_DECODE_ON_CACHE_MISS`
- `STYGIAN_GLYPH_CACHE_ENABLED`

## Test Matrix

Run all tests on both GPU classes.

1. Cold startup decode:
   - Decode all configured tiles in one batch.
   - Required sets: `192x256`, `256x256`, `1024x256` (if available).
2. Warm startup decode:
   - Repeat run without process restart.
3. Runtime paging:
   - Batch decode sizes: `8`, `16`, `32` tiles.
   - No single-tile dispatch in production profile.
   - Split policy:
     - `runtime_paging`: interactive runtime path (dGPU-facing)
     - `runtime_bg_decode`: background decode path (iGPU-facing)
4. Stress scroll:
   - Simulate page misses under continuous scroll.
   - At least 10 seconds sustained.
5. Quality sweep:
   - Render outputs at `16`, `24`, `32`, `64`, `128`, `256` px.

## Measurement Rules

1. Use stable clocks where possible (performance mode / plugged in).
2. Run 5 warmups before timed runs.
3. Run at least 50 measured iterations per test point.
4. For p95 stabilization runs, use `--stabilize-passes 3` (effective 150 samples).
5. Report `min`, `mean`, `p50`, `p95`, `p99`, `max`.
6. Record full environment:
   - CPU model, RAM speed, GPU model, driver version, OS build.
7. For GL runs:
   - Avoid `glFinish` in production timing path.
   - Use fences/barriers appropriate for measured stage.
8. Official `v3.4` production runs must include:
   - `--size-strict`
   - `--require-real-compression`
   - Optional `--triad-file <path>` for artifact-backed runs.

### Canonical Production Command (v3.4)

```bat
v34_bench.exe --csv triad_results.csv --run-id <id> --commit <sha> --gpu-type <igpu|dgpu> --path <startup_warm|runtime_paging|runtime_bg_decode> --batch-size <n> --size-strict --require-real-compression --quality-psnr-min 18 --quality-ssim-min 0.94 --triad-file triad_out\stygian_emoji_master_v5_t2.triad
```

### Canonical Production Matrix Launcher

Use this for release-style `v3.4` signoff runs:

```bat
run_triad_v34_prod.bat igpu triad_out\stygian_emoji_master_v5_t2.triad
run_triad_v34_prod.bat dgpu triad_out\stygian_emoji_master_v5_t2.triad
```

Behavior:
- Runs `startup_warm` (`batch=192`) and runtime batches (`8,16,32`)
- Uses `--stabilize-passes 3` for p95/p99 stability
- Enforces real compression (`BC7 + BC4`) via `--require-real-compression`
- Uses strict size accounting via `--size-strict`
- Validates only the current `run_id`
- Split runtime path:
  - `igpu`: `runtime_bg_decode`
  - `dgpu`: `runtime_paging`

## Production Gates

All gates must pass for release.

1. Runtime frame budget:
   - dGPU (`runtime_paging`): `p95 <= 1.0ms`.
   - iGPU (`runtime_bg_decode`):
     - batch `<=8`: `p95 <= 6.0ms`
     - batch `<=16`: `p95 <= 15.0ms`
     - batch `<=32`: `p95 <= 18.5ms`
2. Startup:
   - dGPU: full startup decode <= `12ms`.
   - iGPU: full startup decode <= `60ms` and hidden before interaction.
3. Stability:
   - 24h soak test, zero crashes/asserts/corruption.
4. Cache behavior:
   - Runtime cache hit rate >= `95%` in representative session.
5. Quality:
   - Must pass objective and visual checks:
     - Legacy synthetic gate: PSNR >= `38 dB`, SSIM >= `0.97` at 64+ px
     - Real corpus TRIAD gate (`v3.4*`, `bc4/r8`): PSNR >= `18 dB`, SSIM >= `0.94`
     - No severe visible banding/halo at 16-32 px
6. Fallback:
   - Automatic fallback path must exist and pass basic parity tests:
     - `BC4 -> R8` (or equivalent safe mode)
7. Compression truth gate:
   - Production benchmark must fail if LL BC7 or index BC4 falls back to uncompressed mode.
   - Diagnostic-only runs may allow fallback, but must not be used for release signoff.

## Required Output Files

For each benchmark run, produce:

1. CSV metrics file using schema in:
   - `means/native/stygian/tools/wavelet_bench/triad_benchmark_schema.csv`
2. Metadata JSON (or text) containing:
   - git commit hash
   - compiler and flags
   - test date/time
   - host hardware/driver info
3. Optional quality artifacts:
   - Render captures for each size bucket
   - Difference maps vs baseline PNG/BC7
4. Suggested manifest file:
   - `means/native/stygian/tools/wavelet_bench/triad_run_manifest.template.json`

## Validation Tool

Use the CSV validator to enforce gates:

```powershell
powershell -ExecutionPolicy Bypass -File means/native/stygian/tools/wavelet_bench/validate_triad_benchmark.ps1 -CsvPath means/native/stygian/tools/wavelet_bench/triad_results.csv
```

## CSV Field Definitions

Use exactly these columns:

- `run_id`: unique benchmark run id
- `timestamp_utc`: ISO8601 UTC timestamp
- `commit`: short git commit id
- `version`: TRIAD version (`v3.4a`, etc.)
- `backend`: `opengl` or `vulkan`
- `gpu_type`: `dgpu` or `igpu`
- `gpu_name`: adapter model
- `driver_version`: graphics driver version
- `cpu_name`: CPU model string
- `ram_gb`: system memory in GB
- `os_name`: OS name
- `os_version`: OS build/version
- `tile_size`: source tile size (e.g. `256`)
- `num_tiles`: tiles decoded in test
- `batch_size`: runtime batch size (`8`,`16`,`32`, etc.)
- `path`: `startup_cold`, `startup_warm`, `runtime_paging`, `runtime_bg_decode`, `stress_scroll`
- `format_ll`: LL storage format (`bc7`, etc.)
- `format_index`: index storage format (`bc4`, `r8`, etc.)
- `format_sparse`: sparse value storage format
- `decode_us_min`: min decode time in microseconds
- `decode_us_mean`: mean decode time in microseconds
- `decode_us_p50`: p50 decode time in microseconds
- `decode_us_p95`: p95 decode time in microseconds
- `decode_us_p99`: p99 decode time in microseconds
- `decode_us_max`: max decode time in microseconds
- `per_tile_us_mean`: mean per-tile decode time
- `frame_budget_pct`: decode time as percentage of frame budget
- `cache_hit_rate_pct`: cache hit rate percent
- `page_faults`: page faults/misses in run
- `vram_mb_active`: active VRAM footprint
- `rss_mb`: process resident memory
- `psnr_db_64`: PSNR at 64 px
- `ssim_64`: SSIM at 64 px
- `visual_artifact_flag`: `0` pass, `1` fail
- `pass_fail`: `pass` or `fail`
- `notes`: free-form notes

## Release Decision Rule

TRIAD is release-ready only when:

1. All production gates pass on both target classes.
2. No critical regressions vs prior release in:
   - `decode_us_p95`
   - `cache_hit_rate_pct`
   - `vram_mb_active`
3. Fallback path validated on at least one problematic/older driver.

## Artifact Decode Path (Phase 4)

After source-manifest runs (phases 2/3), run artifact-backed decode:

1. Build `.triad` tiers with `lgtriad_suite`.
2. Run `triad_v34_bench` with:
   - `--triad-file <tier_file>`
   - `--stabilize-passes 3`
3. Record startup/runtime results and quality metrics in the same CSV schema.

## Runtime Artifact Layout

For Stygian runtime integration, place shipped TRIAD tiers under:

- `means/native/stygian/assets/triad/`

Recommended names:

- `stygian_emoji_t0.triad`
- `stygian_emoji_t1.triad`
- `stygian_emoji_t2.triad`

This keeps benchmark outputs (`tools/wavelet_bench/triad_out`) separate from
runtime assets used by applications.
