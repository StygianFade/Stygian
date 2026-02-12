# Stygian Unicode + ICC Implementation Plan (TRIAD Deferred)

Status:
- TRIAD path: deferred for later research.
- Active emoji path: SGC packed SVG -> runtime raster -> atlas upload.
- Active text path: MTSDF.

Objective:
- Build a production Unicode text and emoji pipeline that merges with MTSDF text rendering.
- Add ICC-aware color management for text + emoji rendering.
- Keep all backend-facing color logic backend-agnostic (OpenGL/Vulkan share core behavior).

Non-goals (this phase):
- No TRIAD decode integration.
- No new compression research in core runtime.

---

## Phase 0: Baseline and Contracts

Deliverables:
- Freeze current behavior as baseline metrics and screenshots.
- Define public/internal API contracts for:
  - UTF-8 decode
  - Grapheme segmentation
  - Emoji shortcode parsing
  - Color transform hooks

Tasks:
1. Add baseline perf capture for chat emoji demo:
   - startup mount
   - first decode
   - batch decode
   - upload
2. Write API contract notes in headers for future module boundaries.
3. Add runtime feature flags:
   - `STYGIAN_UNICODE_ENABLE`
   - `STYGIAN_ICC_ENABLE`
   - `STYGIAN_EMOJI_SGC_ENABLE`

Acceptance:
- Baseline metrics logged in console and documented.
- Feature flags compile cleanly on current build.

---

## Phase 1: Unicode Core Module

New files:
- `src/stygian_unicode.h`
- `src/stygian_unicode.c`
- `tests/stygian_unicode_tests.c` (or local test harness in tools if no test target yet)

Core data structures:
- `StygianCodepoint`
- `StygianGrapheme`
- `StygianTextSpan`
- `StygianEmojiToken`

Core functions:
- `stygian_utf8_next(...)`
- `stygian_grapheme_next(...)`
- `stygian_shortcode_normalize(...)`
- `stygian_text_tokenize(...)`

Initial behavior:
- Correct UTF-8 decode and invalid-sequence fallback.
- Grapheme handling for:
  - combining marks
  - ZWJ
  - variation selectors
  - regional indicator pairs
  - skin tone modifiers
- Normalize emoji ids to canonical `emoji_u...` form.

Acceptance:
- Unit tests pass for known edge cases.
- No regression for existing ASCII-only text paths.

---

## Phase 2: MTSDF Integration (Unicode-Aware Text Layout)

Current gap:
- MTSDF path uses mostly ASCII-era assumptions (`glyphs[256]` fast path).

Tasks:
1. Split text layout into run generation:
   - text runs (MTSDF)
   - emoji runs (SGC)
2. Route codepoints through Unicode core instead of byte iteration.
3. Keep ASCII fast path for speed when applicable.
4. Add fallback chain:
   - MTSDF glyph
   - emoji token
   - replacement glyph

Acceptance:
- Mixed text + emoji strings render in one logical line layout.
- Existing MTSDF text remains stable.

---

## Phase 3: Emoji Inline Composition (SGC Runtime Path)

Tasks:
1. Convert chat demo logic into reusable runtime helpers:
   - pack mount
   - lookup
   - decode
   - atlas slot assignment
2. Add per-frame decode/upload budgets and queueing.
3. Improve cache policy:
   - LRU slot eviction
   - hot reuse for repeated emoji
4. Add debug counters:
   - cache hit/miss
   - decode fail
   - atlas evictions

Acceptance:
- No pink fallback from slot pressure under normal usage.
- Multi-emoji messages remain stable without text corruption.

---

## Phase 4: ICC Color Management Foundation

New files:
- `src/stygian_color.h`
- `src/stygian_color.c`
- `src/stygian_icc.h`
- `src/stygian_icc.c`

Design:
- Parse ICC profile metadata needed for transform setup.
- Runtime transform path:
  - source profile -> linear working space -> target profile
- Default profiles:
  - source: sRGB
  - target: display profile (if available) else sRGB

Tasks:
1. Implement profile descriptor and transform context objects.
2. Add API:
   - `stygian_color_profile_load(...)`
   - `stygian_color_transform_create(...)`
   - `stygian_color_transform_rgba8(...)`
3. Add strict fallback:
   - if ICC unavailable/fails, use sRGB path.

Acceptance:
- Color pipeline can be toggled on/off cleanly.
- No crash/regression when profile loading fails.

---

## Phase 5: ICC Wiring in Text + Emoji Rendering

Tasks:
1. Apply color transform hook to emoji raster upload path (CPU-side RGBA transform).
2. Apply transform hook for text color input path (shader uniform or CPU-prepared linear color).
3. Keep backend abstraction intact:
   - no OpenGL/Vulkan-specific color logic in high-level Unicode/emoji code.
4. Expose config in `StygianConfig`:
   - `enable_icc`
   - `display_profile_path` (optional)

Acceptance:
- Emoji and text color both follow the same target color policy.
- Fallback to sRGB remains visually stable.

---

## Phase 6: Performance and Quality Gates

Bench suites:
- Unicode parser throughput
- Grapheme segmentation correctness + speed
- Emoji decode/upload timings
- ICC transform overhead

Gates:
- Startup mount within expected envelope for selected codec.
- First emoji decode bounded by budgeted ms.
- No frame stalls from unbounded decode work.
- ICC overhead bounded and measurable.

Acceptance:
- Metrics exported in repeatable format.
- Regressions visible in console + docs.

---

## Phase 7: Documentation and Rollout

Docs to update:
- `docs/unicode_research.md` (mark TRIAD as deferred runtime path)
- `docs/triad_benchmark_protocol.md` (research track only for now)
- New runtime docs:
  - Unicode API usage
  - Emoji SGC pipeline usage
  - ICC setup and fallback behavior

Rollout:
1. Default on:
   - Unicode core
   - SGC emoji pipeline
2. Default off (opt-in):
   - ICC until validated on multiple devices
3. Keep TRIAD behind explicit research flag only.

Acceptance:
- Build + run instructions are complete.
- User can enable/disable Unicode/ICC features without code edits.

---

## Immediate Execution Order

1. Implement `stygian_unicode.{h,c}` and tests.
2. Integrate Unicode run segmentation into existing text path.
3. Refactor chat emoji demo helpers into reusable core helper layer.
4. Add ICC module skeleton with sRGB fallback path.
5. Wire ICC into emoji upload + text color flow.
6. Add quality/perf gates and update docs.

