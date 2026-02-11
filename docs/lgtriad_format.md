# LGTRIAD v1 (Build Cache Container)

Purpose: avoid repeated high-cost SVG ingest by keeping a single append-only cache file.

## Goals

- One-file cache (`.lgtriad`) for large corpus ingest.
- Incremental updates: append new/changed glyph sources without rewriting full file.
- Incremental pack: export only pending entries for downstream TRIAD packing.

## v1 Record Model

Header:

- magic: `LGTRIAD1`
- version: `1`
- record_count

Append-only records:

1. `ADD` record
   - glyph hash/id
   - blob hash
   - source metadata
   - SVG blob bytes
2. `PACKED` record
   - glyph hash + blob hash pair
   - marks the matching `ADD` as emitted

Current `pack-triad` output format (`.triad`, v1):

- header (`TRIAD01`, version=1, encoding=1 for SVG-blob payload)
- fixed-size entry index table
- payload stream (glyph id bytes + SVG blob bytes)

This is a direct single-step `lgtriad -> triad` transport format.
TRIAD coefficient packing/reconstruction can replace payload encoding in-place in future versions.

State is reconstructed by replaying records.

## Why append-only

- Minimal disk seeks on HDD (sequential writes).
- No global index rewrite per add.
- Crash resilience is easier (journal style).

## CLI

Tool: `tools/wavelet_bench/lgtriad_suite.c`

- `init <lgtriad>`
- `add <lgtriad> <glyph_id> <svg_path> [source_tag]`
- `add-manifest <lgtriad> <manifest_csv>`
- `list <lgtriad> [pending|all]`
- `stats <lgtriad>`
- `pack <lgtriad> <out_dir> <out_manifest_csv> [limit]`
- `pack-triad <lgtriad> <out_dir> <base_name> [limit]`

`pack` exports pending SVG files.
`pack-triad` exports tier files (`_t0/_t1/_t2`) and appends `PACKED` records for emitted entries.

## Planned next steps

1. Replace SVG-export pack step with direct TRIAD tier emission from in-memory blobs.
2. Add stronger dedupe/index acceleration for 100k+ records.
3. Add optional compaction command to rewrite a clean snapshot file.
