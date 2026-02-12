# stygian.h Reference

This page documents the public runtime API in `include/stygian.h`.

## Core Types

- `StygianContext`
- `StygianElement`, `StygianTexture`, `StygianFont`
- `StygianScopeId`
- `StygianConfig`
- `StygianBackendType`, `StygianType`
- `StygianFrameIntent`
- `StygianRepaintReasonFlags`

## Lifecycle

- `stygian_create`
- `stygian_destroy`
- `stygian_wait_for_events`

Contract:
- `window` in config is required.
- Context owns runtime state, AP handle, scope cache, command queues.

## Frame APIs

- `stygian_begin_frame`
- `stygian_begin_frame_intent`
- `stygian_end_frame`
- `stygian_is_eval_only_frame`

Contract:
- Begin/end must be paired.
- `STYGIAN_FRAME_EVAL_ONLY` must not submit/draw/swap.

## Repaint Scheduler

- `stygian_request_repaint_hz`
- `stygian_request_repaint_after_ms`
- `stygian_has_pending_repaint`
- `stygian_next_repaint_wait_ms`
- `stygian_set_repaint_source`
- `stygian_get_repaint_source`
- `stygian_get_repaint_reason_flags`

## Scope and Layer APIs

- `stygian_scope_begin`, `stygian_scope_end`
- `stygian_scope_invalidate`, `stygian_scope_invalidate_now`, `stygian_scope_invalidate_next`
- `stygian_scope_is_dirty`, `stygian_scope_get_last_dirty_info`
- `stygian_layer_begin`, `stygian_layer_end`
- `stygian_overlay_scope_begin`, `stygian_overlay_scope_end`
- `stygian_request_overlay_hz`, `stygian_invalidate_overlay_scopes`

## Element APIs

Allocation:
- `stygian_element`
- `stygian_element_transient`
- `stygian_element_batch`
- `stygian_element_free`

Property writes:
- `stygian_set_bounds`, `stygian_set_color`, `stygian_set_border`, `stygian_set_radius`
- `stygian_set_type`, `stygian_set_visible`, `stygian_set_z`
- `stygian_set_texture`
- `stygian_set_shadow`, `stygian_set_gradient`
- `stygian_set_hover`, `stygian_set_blend`, `stygian_set_blur`, `stygian_set_glow`
- `stygian_set_clip`, `stygian_clip_push`, `stygian_clip_pop`

## Textures and Text

Textures:
- `stygian_texture_create`
- `stygian_texture_update`
- `stygian_texture_destroy`

Fonts/Text:
- `stygian_font_load`
- `stygian_font_destroy`
- `stygian_text`
- `stygian_text_width`

## Convenience Draw APIs

- `stygian_rect`
- `stygian_rect_rounded`
- `stygian_line`
- `stygian_wire`
- `stygian_image`
- `stygian_image_uv`

## Triad and Glyph APIs

- glyph policy/profile setters/getters
- triad mount/unmount/query/decode APIs
- output/glyph color profile APIs

## Metrics and Diagnostics

Frame metrics:
- draw calls, element/clip counts, upload bytes/ranges
- replay hits/misses/forced rebuild counts
- build/submit/present/gpu ms
- frame reason flags and eval-only marker

Capacity/state metrics:
- active elements, free elements, capacity
- font count, emoji cache count
- commit applied count, command drop count

## Error Channel APIs

- `stygian_context_set_error_callback`
- `stygian_set_default_context_error_callback`
- `stygian_context_get_recent_errors`
- `stygian_context_get_error_drop_count`

Errors are per-context first, with optional global fallback.
