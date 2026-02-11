# Stygian DDI, DoD, SoA/AoSoA, and Arena/Pool Architecture

This document is the technical reference for Stygian’s data-driven and memory discipline. It expands on the rules summarized in the repo root `AGENTS.md` and aligns with `dod_data_layout_inventory.md`.

## 1. DDI (Data-Driven Invalidation)

### 1.1 North star

- **“100% DDI”** means: the GPU is resident, buffers are persistent, and **render/present only happens when data changes** (state mutation, timer, async, animation), not because input occurred.
- Performance target: near-idle GPU/CPU when nothing changes; overlay/diagnostics cost scales with overlay area and tick rate, not with the whole UI.

### 1.2 What DDI is (in Stygian terms)

- **Immediate-mode API**: you still “describe UI” each frame (same call pattern).
- **Data-driven runtime**: those calls write into **stable data slots/scopes**, not “draw now”.

Core invariants:

- Stable IDs exist (elements/scopes).
- Changes bump “versions” / dirty flags in data, not “widget objects”.
- Renderer consumes the **snapshot + dirty ranges**.
- If nothing is dirty and no timer is due: **no frame build, no submit, no present**.

This is distinct from:

- **Classic IMGUI**: rebuild + upload + present every frame by design.
- **Retained UI**: tree of objects; event-driven updates; invalidation bubbles in the widget tree.

### 1.3 Repaint drivers (only these may request repaint)

| Driver | When |
|--------|------|
| State mutation | Scroll offset changed, button toggled, focus, drag start/end, etc. |
| Timer / animation | `stygian_request_repaint_hz`, `stygian_request_repaint_after_ms` |
| Async completion | Decode/load done |
| Explicit forced | `stygian_request_repaint` (or equivalent) |
| First frame | One-time initial present |

**Pointer-only events** (mouse move/click/scroll that do not affect any interactive region) must **not** request repaint. The host should call `stygian_has_pending_repaint()` and only then run build/submit/present.

### 1.4 Scope replay (path to 100% DDI)

- `scope_begin(id)` records `range_start`.
- Building writes elements into a **persistent** element array.
- `scope_end()` records `range_count`.
- Next frame: if scope is **clean**, do not rewrite; advance cursor and keep the range (replay).
- If **nothing is dirty**: skip build entirely, skip upload, skip present.

Today `stygian_begin_frame` still resets element pool/free list globally; making scope replay real requires **not** doing that global reset when replaying clean scopes.

---

## 2. DoD (Data-Oriented Design) and layout

### 2.1 SoA vs AoS vs AoSoA

| Layout | Use when |
|--------|----------|
| **SoA** | Hot iteration, per-frame traversal, dirty/free lists (default for new hot-path data). |
| **AoS** | Upload ABI (e.g. `StygianGPUElement`), or cold/config metadata. |
| **AoSoA** | SIMD/vectorized work; data naturally blockable (e.g. blocks of 32–64). |

Rules:

- Prefer **SoA** for hot paths.
- **AoS** only for upload packing or cold, non-bottleneck data.
- **Stable IDs + contiguous storage** are required for replay and dirty tracking.

See `dod_data_layout_inventory.md` for the current layout classification and migration plan.

### 2.2 No hidden object graphs

- Avoid per-widget object trees and callback-driven “should I draw?” decisions.
- Data and dirty flags drive rendering; not pointer chasing.

---

## 3. Memory: Allocator, Arena, Pool

### 3.1 Rules (strict)

1. **Runtime UI/render paths** must not use raw `malloc` / `calloc` / `realloc` / `free`.
2. Use **Stygian allocator hooks** (`StygianAllocator` from `stygian_memory.h`) and/or **arena/pool** allocators for owned runtime memory.
3. **Per-frame hot paths** must be allocation-free.
4. **Exceptions** (with inline comment):
   - Third-party internals (e.g. `stb_*`).
   - Platform/backend **bootstrap** when unavoidable (e.g. first-time shader load).
   - Tooling/bench code outside runtime paths.

### 3.2 Where allocations go

| Lifetime | Mechanism |
|----------|-----------|
| **Persistent** (context, pools, AP buffers) | `ctx->allocator` or `config->persistent_allocator`; AP receives allocator via `StygianAPConfig.allocator`. |
| **Per-frame scratch** | Arena: allocate from arena, reset at frame end. |
| **Fixed-size blocks** (e.g. pool of nodes) | `StygianPool` from `stygian_memory.h`. |

Core and AP backends (GL/VK) now route **persistent** buffers through the config allocator; when `config->allocator` is NULL, backends fall back to CRT only for those bootstrap paths.

### 3.3 TRIAD / ICC

Loaders that return heap-owned blobs still use CRT unless they get an allocator (e.g. `_ex(..., allocator)` or a “header before payload” pattern). Converting them is part of the full “no CRT at runtime” tranche.

---

## 4. Overlay and diagnostics

- Perf widget / frame graph should live in a **dedicated overlay scope**.
- Overlay can tick at 30/60/120 Hz but must **not** invalidate the base UI scope.
- Two-rate suggestion: graph at 30 Hz (or 60 Hz in stress mode), text stats at 2–5 Hz.
- Prevents “tiny overlay tick” from causing full-scene rebuild/upload.

---

## 5. Upload and present

- **Dirty-range upload only** when possible; dense full-buffer upload as fallback (must be measured/logged).
- GL and VK backends already support dirty coalescing and range uploads; ensure overlay-only updates only upload overlay element slots.

---

## 6. Verification and instrumentation

To prove “tiny region tick doesn’t cause global work”, periodically log (e.g. every 10 s):

- Repaint reasons distribution
- Pointer-only vs mutation events
- Dirty scopes / elements / ranges
- Upload bytes per frame
- Build / submit / present CPU ms

---

## 7. References

- **Repo-wide rules**: `AGENTS.md` (Tachyon root).
- **Data layout and allocator baseline**: `dod_data_layout_inventory.md`.
- **Allocator API**: `include/stygian_memory.h` (arena, pool, `StygianAllocator`).
