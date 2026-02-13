# Stygian v0.1.0 Release Checklist

## Scope
- Standalone Stygian repository only.
- OpenGL and Vulkan build parity via manifest runners.
- DDI/perf gates validated on current baseline.

## Release Gates
- [ ] `compile/windows/build_mini_apps_all.bat` succeeds.
- [ ] `compile/windows/run_perf_gates.bat -Backend both -Seconds 6 -Profile aggressive` passes.
- [ ] `compile/run.ps1 -Target text_editor_mini` succeeds.
- [ ] `compile/run.ps1 -Target text_editor_mini_vk` succeeds.
- [ ] `git status --short` is clean before tagging.

## Repo Hygiene
- [ ] No tracked `deprecated/`, `examples/deprecated/`, or `docs/context/`.
- [ ] No tracked binaries/build outputs (`.exe`, `build/`).
- [ ] Stygian-local `.gitignore` enforces standalone policy.

## Docs Sanity
- [ ] `README.md` points to `compile/run.ps1` and `compile/run.sh`.
- [ ] Integration quickstart uses unified compile entrypoints.
- [ ] Perf gate policy points to `compile/windows/run_perf_gates.bat`.

## First Release Tag
- [ ] Tag created: `v0.1.0`
- [ ] Tag annotation includes summary of:
  - DDI/eval-only frame split
  - idle/perf gate closure
  - manifest-driven compile runners

## Publish
- [ ] Push `main` to private GitHub repository.
- [ ] Push tags.
