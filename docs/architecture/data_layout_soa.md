# Data Layout and SoA

Stygian uses a three-buffer SoA split for hot rendering paths.

## Buffers

- Hot (`StygianSoAHot`): bounds, color, texture id, type/mode, flags, z.
- Appearance (`StygianSoAAppearance`): border, radius, uv, control points.
- Effects (`StygianSoAEffects`): shadow, gradient, hover/blend/blur/glow.

## Chunk Versioning

Per chunk, each SoA buffer tracks:
- version counter
- dirty min/max element indices

Backend compares CPU chunk versions against GPU chunk versions and uploads only changed ranges.

## Why this layout

- Minimizes bandwidth for common primitives that only need hot fields.
- Keeps cold/effect fields out of every fragment path.
- Supports predictable dirty range uploads and replay.

## Element Identity

- Stable IDs with contiguous storage.
- Free-list reuse for allocation and deterministic replay indexing.

## DoD rules

- SoA for hot iteration paths.
- AoS only for control metadata and cold runtime bookkeeping.
- No hidden object graph driving render decisions.
