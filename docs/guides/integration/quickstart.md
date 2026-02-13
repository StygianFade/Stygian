# Quickstart

## 1. Create a Window

Use `stygian_window_create` or `stygian_window_create_simple`.

## 2. Create Context

Build `StygianConfig` with:
- backend (`OPENGL` or `VULKAN`)
- max elements
- max textures
- window pointer
- optional shader dir and allocator

Call `stygian_create`.

## 3. Frame Loop Pattern

1. Collect events.
2. Feed widget/input processing.
3. Decide frame intent:
- render frame if mutation/repaint/async/first frame
- eval-only frame for request-eval only
4. `stygian_begin_frame_intent(...)`
5. Build scopes and widgets.
6. `stygian_end_frame(...)`
7. Wait with `stygian_next_repaint_wait_ms(...)` when idle.

## 4. Shutdown

- destroy context
- destroy window

## Build Entrypoints

Manifest source of truth:
- `compile/targets.json`

Cross-platform runners:
- Windows: `compile/run.ps1 -Target text_editor_mini`
- Linux/macOS: `compile/run.sh --target text_editor_mini`

Windows convenience wrappers in repo root:
Windows wrapper scripts:
- `compile/windows/build_text_editor_mini.bat`
- `compile/windows/build_calculator_mini.bat`
- `compile/windows/build_calendar_mini.bat`
- VK variants with `_vk.bat` in `compile/windows/`
- matrix build: `compile/windows/build_mini_apps_all.bat`
