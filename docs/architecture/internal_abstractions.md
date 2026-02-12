# Internal Abstractions Map

This page documents key internal abstractions from `src/stygian_internal.h`.

## Context Core

`StygianContext` owns:
- SoA storage and chunk metadata
- free-list allocator state for element IDs
- repaint scheduler state
- scope cache and replay cursors
- command producer queues and merge buffers
- winner ring and error ring

## SoA and Chunk Tracking

- `StygianSoAHot`
- `StygianSoAAppearance`
- `StygianSoAEffects`
- `StygianBufferChunk`

Each chunk stores version and dirty min/max for hot/appearance/effects.

## Command Runtime

- `StygianCmdRecord`
- `StygianCmdProducerQueue`
- `StygianCmdQueueEpoch`
- `StygianCmdBuffer`

Design goals:
- multi-producer ingestion
- deterministic merge
- single-thread commit mutation

## Provenance Runtime

- `StygianScopeCacheEntry` for dirty reason/source/frame
- `StygianWinnerRecord` ring for deterministic winner introspection
- `StygianContextErrorRecord` ring for bounded error telemetry
