# TRIAD Competitive Position (Conservative)

This is a non-marketing assessment against common emoji delivery/rendering stacks.

## What TRIAD Already Demonstrates

- Low active memory for decoded working set relative to raw RGBA atlases.
- Fast batched decode on tested hardware when dispatches are grouped (not per-glyph).
- Viable startup decode path for moderate corpus sizes on tested devices.

## What Is Not Proven Yet

- Cross-machine stability across newer/older iGPU+dGPU drivers.
- End-to-end visual parity against production emoji references for all glyph classes.
- Runtime behavior at true 10k corpus scale under real UI interaction traces.
- Reliability under mixed workloads (decode + rendering + UI composition + text).

## Conservative Comparison

1. PNG/CDN sprite workflows (Discord/Slack-style)
- Strength: Mature tooling, predictable quality.
- Weakness: Larger payload/storage for multi-size support.
- TRIAD delta: Better memory efficiency potential; needs stronger validation on compatibility and quality gates.

2. SVG pipelines (Twemoji-like source + rasterization)
- Strength: Source flexibility, vector master assets.
- Weakness: Runtime rasterization cost if done frequently.
- TRIAD delta: Better for pre-batched GPU decode and cache-heavy runtime; still needs broad QA.

3. System emoji fonts (platform-provided)
- Strength: Integration, shaping, platform consistency.
- Weakness: Platform variance, limited control.
- TRIAD delta: More control over cache/LOD and memory budgets, less out-of-box portability until implementation matures.

## Engineering Conclusion

TRIAD is implementation-ready as an experimental production path behind feature flags, not as unconditional default yet.

Recommended rollout:

1. Keep existing fallback path as baseline.
2. Gate TRIAD with runtime capability checks and telemetry.
3. Promote TRIAD to default only after:
   - cross-vendor matrix passes,
   - visual parity thresholds pass,
   - long-run stability tests pass.
