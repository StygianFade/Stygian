# Threading and Commit

Current model: multi-producer command generation with single-thread commit.

## Command Path

- Producers create `StygianCmdBuffer` with `stygian_cmd_begin`.
- Producers emit property mutations (`set_bounds`, `set_color`, etc).
- Producers submit buffer (`stygian_cmd_submit`).
- Core commit applies mutations at frame boundary.

## Determinism

Deterministic merge key is effectively ordered by:
- scope id
- element id
- property id
- operation priority
- submit sequence
- command index

Conflict policy is last-write-wins per property under deterministic ordering.

## Ownership

- Producers never mutate SoA directly.
- Commit thread is sole SoA writer.
- Render reads committed snapshot state.

## Safety

- Fixed-capacity queues and merge buffers.
- Overflow/drop is logged via per-context error ring.
- Submit during commit is rejected and reported.

## Debug provenance

- Scope dirty reason/source/frame fields.
- Winner ring tracks winning command metadata.
- Error ring tracks code/source/thread/frame/message.
