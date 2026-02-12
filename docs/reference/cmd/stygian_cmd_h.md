# stygian_cmd.h Reference

This page documents command-buffer APIs in `include/stygian_cmd.h`.

## Purpose

Command buffers decouple mutation production from SoA writes.
Producers emit commands; core applies at commit boundary.

## Types

- `StygianCmdBuffer` (opaque)
- `StygianCmdPropertyId` (property identity for deterministic merge)

## Lifecycle

- `stygian_cmd_begin(ctx, source_tag)`
- `stygian_cmd_submit(ctx, buffer)`
- `stygian_cmd_discard(buffer)`

Contract:
- A buffer is owned by the producer that created it.
- Submitted buffers are immutable.
- Submit during commit is rejected.

## Emit APIs

Core properties:
- `stygian_cmd_set_bounds`
- `stygian_cmd_set_color`
- `stygian_cmd_set_border`
- `stygian_cmd_set_radius`
- `stygian_cmd_set_type`
- `stygian_cmd_set_visible`
- `stygian_cmd_set_z`
- `stygian_cmd_set_texture`

Effects:
- `stygian_cmd_set_shadow`
- `stygian_cmd_set_gradient`
- `stygian_cmd_set_hover`
- `stygian_cmd_set_blend`
- `stygian_cmd_set_blur`
- `stygian_cmd_set_glow`

## Determinism Guarantees

Conflicts are resolved deterministically by commit ordering and property id.
Same input command stream must produce same final SoA state.

## Error and Overflow Behavior

- Queue overflow drops commands and logs errors.
- Invalid element/property writes are rejected and logged.
- Runtime continues; no hard process abort on command errors.
