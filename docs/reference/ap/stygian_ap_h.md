# stygian_ap.h Reference

This page documents backend abstraction contracts in `backends/stygian_ap.h`.

## Purpose

`stygian_ap` is the only layer allowed to touch GPU APIs.
Core never calls GL/VK directly.

## Types

- `StygianAP`
- `StygianAPConfig`
- `StygianAPType`
- `StygianAPAdapterClass`
- `StygianAPSurface`

## Lifecycle

- `stygian_ap_create`
- `stygian_ap_destroy`
- `stygian_ap_get_adapter_class`

## Frame Path APIs

- `stygian_ap_begin_frame`
- `stygian_ap_submit` (texture remap and bind preparation)
- `stygian_ap_submit_soa` (versioned chunk upload)
- `stygian_ap_draw`
- `stygian_ap_draw_range`
- `stygian_ap_end_frame`
- `stygian_ap_swap`

## Multi-Surface APIs

- create/destroy surface
- surface begin/submit/end/swap
- main surface getter

## Timing and Upload Metrics

- `stygian_ap_get_last_upload_bytes`
- `stygian_ap_get_last_upload_ranges`
- `stygian_ap_get_last_gpu_ms`
- `stygian_ap_gpu_timer_begin`
- `stygian_ap_gpu_timer_end`

Contract:
- Unsupported timers may report 0.
- Implementations should avoid stalls while measuring.

## Resource APIs

Textures:
- create/update/destroy/bind

Shaders:
- `stygian_ap_reload_shaders`
- `stygian_ap_shaders_need_reload`

Uniform-style state:
- `stygian_ap_set_font_texture`
- `stygian_ap_set_output_color_transform`
- `stygian_ap_set_clips`

## Parity Requirements

Both GL and VK must respect the same core-visible semantics:
- dirty range upload accounting
- eval-only frame no-submit behavior
- consistent timing metric meanings
