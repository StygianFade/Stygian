# TRIAD Emoji Dataset Plan

Purpose: provide a repeatable corpus for 10k-scale TRIAD validation without blocking on asset curation.

## Recommended Sources

Use one or more upstream emoji datasets as source material:

- Noto Emoji (`github.com/googlefonts/noto-emoji`)
- Twemoji (`github.com/twitter/twemoji`)
- OpenMoji (`github.com/hfg-gmuend/openmoji`)
- Unicode test lists for coverage planning (`unicode.org/Public/emoji/`)
- Platform screenshots/renders for internal QA only

Always verify upstream license terms before redistribution.

## Practical Test Strategy

1. Build a real source set (hundreds to low thousands of unique emoji images).
2. Duplicate deterministically to 10k for stress/load testing.
3. Keep a manifest mapping each generated test glyph to its original source.
4. Run matrix benches against:
   - real-only set (quality signal)
   - duplicated 10k set (scale/perf signal)

## 10k Corpus Guidance

- Prefer `>= 1000` unique emoji masters first.
- Duplicate deterministically to reach 10k for load tests.
- Keep duplication metadata in manifest for reproducibility.
- Do not use duplicated corpus alone for visual quality claims.

## Tooling

Use:

- `means/native/stygian/tools/wavelet_bench/prepare_emoji_dataset.ps1`

Example:

```powershell
powershell -ExecutionPolicy Bypass -File means/native/stygian/tools/wavelet_bench/prepare_emoji_dataset.ps1 -InputDir means/native/stygian/assets -OutputDir means/native/stygian/tools/wavelet_bench/emoji_corpus_10k -TargetCount 10000
```

Output:

- `emoji_corpus_10k/emoji_00000.png ...`
- `emoji_corpus_10k/emoji_manifest.csv`
