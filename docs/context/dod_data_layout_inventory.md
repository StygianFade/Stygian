# Stygian DoD Data Layout Inventory

## Purpose
This document defines the current Data-Oriented Design baseline for Stygian and
the required migration path to strict DDI + DoD execution. For the full
architecture (DDI contract, SoA/AoSoA, arena/pool, overlay policy), see
`ddi_dod_architecture.md`.

## Current Layout Classification

### AoS (present)
- `StygianGPUElement[]` in `src/stygian_internal.h`
  - Stored as one packed struct per element.
  - Good for shader upload simplicity, not ideal for CPU-side hot-path writes.
- `StygianScopeCacheEntry[]` in `src/stygian_internal.h`
  - Mixed fields (id, dirty flags, ranges, snapshots) in one struct.
- `StygianInlineEmojiCacheEntry[]` in `src/stygian_internal.h`
  - Cache metadata grouped per entry.
- `WidgetRegion[]` (`regions_prev`, `regions_curr`) in
  `widgets/stygian_widgets.c`
  - Rect and flags grouped per region.

### SoA (present)
- `dirty_bitmap`, `dirty_list`, `free_list` in `StygianContext`
  - Split storage for dirty tracking and free-slot tracking.
- Kerning O(1) tables:
  - `kerning_table[256][256]`
  - `kerning_has[256][256]`
  - Data-parallel lookup path, no linear scan on hot path.

### AoSoA (present)
- No explicit AoSoA containers are currently used in core runtime.

## Allocator Baseline (this tranche)

Core context allocation now routes through `StygianConfig.persistent_allocator`
with fallback to a built-in CRT allocator.

### Applied to
- `StygianContext` object allocation and destroy path.
- Core pools owned by `StygianContext`:
  - `elements`
  - `dirty_bitmap`
  - `dirty_list`
  - `free_list`
  - `clips`
  - `fonts`
- Dynamic font tables in `src/stygian.c`:
  - `glyph_entries`
  - `glyph_hash`
  - `kerning_pairs`

### Now converted (allocator from StygianAPConfig)
- Backend-owned persistent buffers in `backends/stygian_ap_gl.c` and
  `backends/stygian_ap_vk.c`: submit_buffer, dirty_scratch, and (GL/VK) shader
  load buffers use `config->allocator` when set; otherwise CRT fallback.
- Core passes `ctx->allocator` into `StygianAPConfig.allocator` at create.

### Not yet converted
- Third-party or module-owned buffers that require module-specific free paths,
  e.g. `mtsdf.pixels`.
- Widget-local allocations in `widgets/*`.
- TRIAD/ICC loaders (heap blobs; need `_ex` allocator or header-before-payload).

## Strict DoD Rules

1. Repaint decisions are data-driven only:
   - state mutation
   - timer deadline
   - animation tick
   - async completion
2. Pointer-only events are never render reasons by themselves.
3. New hot-path containers must default to SoA.
4. AoS is allowed only when:
   - upload ABI requires struct packing, or
   - code path is cold and measured impact is negligible.
5. Any new heap allocation in hot paths must use context allocator hooks or
   arena/pool allocators.
6. Any fallback to full-buffer upload must be measured and logged.

## Migration Plan: AoS -> SoA/AoSoA

### Stage 1 (safe, low risk)
- Keep `StygianGPUElement` as upload ABI.
- Add CPU-side split write buffers for hot fields:
  - bounds, color, type, flags, z
- Repack to AoS only at submit boundary.

### Stage 2 (targeted SoA)
- Convert scope cache internals to SoA arrays:
  - `scope_id[]`
  - `scope_dirty[]`
  - `scope_dirty_next[]`
  - `scope_range_start[]`
  - `scope_range_count[]`

### Stage 3 (AoSoA for large scenes)
- For heavy element counts, introduce block storage:
  - Block size: 32 or 64 entries
  - SoA fields inside each block
  - Improves cache locality for range scans and SIMD-friendly updates.

## Validation Requirements

- No regression in input correctness.
- No regression in single-pass draw-call behavior for base UI.
- Dirty uploads scale with damaged region size.
- Idle CPU and GPU remain low under no-mutation conditions.

## See also
- `ddi_dod_architecture.md` — DDI contract, SoA/AoSoA, arena/pool, overlay policy.
