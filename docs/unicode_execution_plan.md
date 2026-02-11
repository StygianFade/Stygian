# Stygian Unicode Execution Plan (Current)

This plan tracks what is implemented in core and what remains before "full Unicode + color-managed emoji chat" is production-ready.

## Scope Baseline

- Keep TRIAD as research path (not default runtime dependency for chat demo).
- Use MTSDF for text rendering.
- Use Unicode-aware parsing for UTF-8/graphemes/shortcodes.
- Use ICC-driven color profile transforms where practical.

## Implemented In This Pass

1. Public Unicode API exposure
- Added public headers:
  - `include/stygian_unicode.h`
  - `include/stygian_color.h`
  - `include/stygian_icc.h`
- `include/stygian.h` now includes those headers directly.

2. Core color profile plumbing
- `StygianContext` now stores:
  - output color profile
  - glyph source color profile
  - transform-enabled state
- Added runtime APIs:
  - `stygian_set_output_color_space`
  - `stygian_set_output_icc_profile`
  - `stygian_get_output_color_profile`
  - `stygian_set_glyph_source_color_space`
  - `stygian_get_glyph_source_color_profile`

3. MTSDF upload color transform integration
- `stygian_font_load` now color-transforms atlas pixels from glyph source profile -> output profile before texture upload when profiles differ.

4. UTF-8-safe text traversal
- `stygian_text` now iterates UTF-8 codepoints instead of raw bytes.
- `stygian_text_width` now uses the same UTF-8 path.
- Newline handling now works in both draw and width calculation.
- Non-ASCII codepoints fallback to `?` until non-ASCII atlas path lands.

5. ICC matrix correctness fix
- Added `stygian_color_profile_init_custom` to build valid custom profiles with inverse matrix generation.
- ICC loader now uses that path instead of writing partial profile state.

6. Build wiring
- Updated:
  - `build.bat`
  - `build_chat_emoji_demo.bat`
  - `build_jit.bat`
- New source modules are linked consistently:
  - `src/stygian_unicode.c`
  - `src/stygian_color.c`
  - `src/stygian_icc.c`

## Still Not Done

1. Full Unicode shaping stack
- No HarfBuzz-style shaping yet.
- No BiDi engine integration yet.
- No full grapheme cluster rendering logic in `stygian_text` (parser exists, renderer still atlas-limited).

2. Astral-plane font atlas path (DONE in this pass)
- `MTSDFAtlas` now stores dynamic glyph entries + hash lookup for codepoints outside ASCII.
- `StygianFontAtlas` now stores dynamic glyph entries + hash lookup in core runtime.
- `stygian_text` and `stygian_text_width` now resolve glyphs by Unicode codepoint (ASCII fast path + dynamic fallback).
- Non-ASCII kerning fallback is now available through copied kerning pairs.

3. Emoji composition in core text renderer
- Chat demo resolves shortcodes externally; core renderer still lacks inline emoji composition as first-class text layout.

4. ICC end-to-end GPU path
- Current color conversion is CPU-side atlas transform at load time.
- No shader-side profile conversion chain for all texture/color inputs yet.

5. Quality and perf gates
- Need repeatable perf + quality test matrix for:
  - sRGB vs P3 profiles
  - iGPU vs dGPU
  - large message/emoji mixed content

## Recommended Next Sequence

1. Add grapheme cluster iteration path in `stygian_text` with cluster-level draw units.
2. Introduce emoji inline token resolver in core text layout (shortcode -> cached texture slot).
3. Add profile-aware shader uniforms for wide-gamut output path (optional runtime toggle).
4. Lock in benchmark harness: startup, p95 runtime, decode/upload latency, memory footprint.
