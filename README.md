# Stygian

GPU-accelerated SDF UI rendering library.

## Features

- Signed Distance Field (SDF) based rendering
- MTSDF text with subpixel accuracy
- Custom window chrome and titlebar
- Hardware-accelerated via OpenGL 4.3+
- Single-header friendly

## Quick Start

This program is the exact source used in `examples/quickwindow.c`.

```c
#include "stygian.h"
#include "stygian_window.h"

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_QUICK_BACKEND STYGIAN_BACKEND_VULKAN
#define STYGIAN_QUICK_WINDOW_RENDER_FLAG STYGIAN_WINDOW_VULKAN
#else
#define STYGIAN_QUICK_BACKEND STYGIAN_BACKEND_OPENGL
#define STYGIAN_QUICK_WINDOW_RENDER_FLAG STYGIAN_WINDOW_OPENGL
#endif

int main(void) {
    StygianWindowConfig win_cfg = {
        .width = 1280,
        .height = 720,
        .title = "Stygian Quick Window",
        .flags = STYGIAN_WINDOW_RESIZABLE | STYGIAN_QUICK_WINDOW_RENDER_FLAG,
    };
    StygianWindow *window = stygian_window_create(&win_cfg);
    if (!window) return 1;

    StygianConfig cfg = {
        .backend = STYGIAN_QUICK_BACKEND,
        .window = window,
    };
    StygianContext *ctx = stygian_create(&cfg);
    if (!ctx) {
        stygian_window_destroy(window);
        return 1;
    }

    StygianFont font = stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");
    while (!stygian_window_should_close(window)) {
        StygianEvent event;
        while (stygian_window_poll_event(window, &event)) {
            if (event.type == STYGIAN_EVENT_CLOSE) stygian_window_request_close(window);
        }

        int width, height;
        stygian_window_get_size(window, &width, &height);
        stygian_begin_frame(ctx, width, height);
        stygian_rect(ctx, 10, 10, 200, 100, 0.2f, 0.3f, 0.8f, 1.0f);
        if (font) stygian_text(ctx, font, "Hello", 20, 50, 16.0f, 1, 1, 1, 1);
        stygian_end_frame(ctx);
    }

    if (font) stygian_font_destroy(ctx, font);
    stygian_destroy(ctx);
    stygian_window_destroy(window);
    return 0;
}
```

## Build and Run (Windows, Novice Path)

1. Build shader outputs (required once per shader change):
   - `compile\windows\build_shaders.bat`
2. Build the quick-start example (`examples/quickwindow.c`):
   - `compile\windows\build_quickwindow.bat`
3. Run it from repo root:
   - `build\quickwindow.exe`

## Tiny Smoke Tests (Windows)

1. Default quick test:
   - `compile\windows\build.bat`
   - `build\quickwindow.exe`
2. Borderless window:
   - `compile\windows\build_quickwindow_borderless.bat`
   - `build\quickwindow_borderless.exe`
3. Custom titlebar window:
   - `compile\windows\build_quickwindow_custom_titlebar.bat`
   - `build\quickwindow_custom_titlebar.exe`
4. Custom titlebar window (Vulkan):
   - `compile\windows\build_quickwindow_custom_titlebar_vk.bat`
   - `build\quickwindow_custom_titlebar_vk.exe`
5. Build all quick smoke tests at once:
   - `compile\windows\build_quick_smoke.bat`

## Windows Borderless Maximize Behavior

Borderless maximize on Windows is work-area maximize (taskbar remains visible),
not true fullscreen.

If you want fullscreen-style monitor coverage, that is a different mode and not
the default maximize behavior.

On Windows, OpenGL borderless main windows keep strict `WS_POPUP` semantics
with manual maximize/restore sizing in the core Win32 backend (`window/platform/stygian_win32.c`).

Vulkan borderless path remains unchanged in this iteration.

`quickwindow_custom_titlebar` uses an event-paced loop to avoid maximize hitch
behavior in unstable OpenGL vsync environments.

## Custom Titlebar API (Win32-first)

Custom titlebar behavior is now exposed through core window APIs:

- `stygian_window_get_titlebar_hints`
- `stygian_window_set_titlebar_behavior`
- `stygian_window_begin_system_move`
- `stygian_window_titlebar_double_click`
- `stygian_window_get_titlebar_menu_actions`
- `stygian_window_apply_titlebar_menu_action`
- `stygian_window_set_fullscreen` / `stygian_window_is_fullscreen`

Defaults:

- Double-click on titlebar toggles maximize/restore.
- Hover-menu behavior can be enabled/disabled per window via `StygianTitlebarBehavior`.
- Win32 provides native button-order hints (`right`) and snap/fullscreen action presets.

Current platform status:

- Win32: implemented.
- Linux/macOS backends: API contract is present with deterministic no-crash fallback stubs.

## Backend Switching Rules (Strict)

All three layers must match:

1. `StygianConfig.backend` in source.
2. Window render flag in source (`STYGIAN_WINDOW_OPENGL` or
   `STYGIAN_WINDOW_VULKAN`).
3. Build target backend in `compile/targets.json` (`"gl"` or `"vk"`).

For quickwindow, switch backend by target, not by manual constant edits:

- OpenGL path: `compile\windows\build_quickwindow.bat` -> `build\quickwindow.exe`
- Vulkan path: `compile\windows\build_quickwindow_vk.bat` -> `build\quickwindow_vk.exe`

Changing only `STYGIAN_BACKEND_OPENGL` to `STYGIAN_BACKEND_VULKAN` can compile
but still be mismatched at runtime if the target backend/flags are not aligned.

## Backends

- `backends/stygian_ap_gl.c` - OpenGL access point
- `backends/stygian_ap_vk.c` - Vulkan access point

## Build Targets

Manifest-driven build metadata lives in:

- `compile/targets.json`

Platform runners:

- Unified:
  - Windows: `compile/run.ps1`
  - Linux/macOS: `compile/run.sh`
- Windows: `compile/windows/build.ps1`
- Linux: `compile/linux/build.sh`
- macOS: `compile/macos/build.sh`

Windows convenience wrapper scripts:

- `compile/windows/build.bat` (default quick test)
- `compile/windows/build_quickwindow.bat`
- `compile/windows/build_quickwindow_vk.bat`
- `compile/windows/build_quickwindow_borderless.bat`
- `compile/windows/build_quickwindow_custom_titlebar.bat`
- `compile/windows/build_quickwindow_custom_titlebar_vk.bat`
- `compile/windows/build_quick_smoke.bat`
- `compile/windows/build_text_editor_mini.bat`
- `compile/windows/build_calculator_mini.bat`
- `compile/windows/build_calendar_mini.bat`
- `compile/windows/build_perf_pathological_suite.bat`
- `compile/windows/build_mini_apps_all.bat`

Perf gate command:

- `compile/windows/run_perf_gates.bat`

## Verification

Tiered runtime/safety checks:

- Tier 1: `tests/run_tier1_safety.ps1`
- Tier 2: `tests/run_tier2_runtime.ps1`
- Tier 3: `tests/run_tier3_misuse.ps1`
- All tiers: `tests/run_all.ps1`

Build-only CI check target list:

- `tier1_safety`
- `tier2_runtime`
- `tier3_misuse`
- `calculator_mini`

CI workflow:

- Workflow file: `.github/workflows/stygian-ci.yml`
- Automatic on `push`/`pull_request`: Windows build checks (no shader output requirement).
- Manual (`workflow_dispatch`, `run_runtime=true`):
  best-effort runtime tier execution via `tests/run_all.ps1`.

## Project Status and Limitations

- Stygian is still in its infancy and remains a work in progress.
- APIs, behavior, and internals may change as real-project usage hardens the system.
- Performance is improving, but it may not consistently match expectations across all
  workloads and hardware.
- The MIT license applies: software is provided as-is, with no warranty of fitness.
- Day-to-day central repo maintenance is limited; updates are occasional.
- Current primary focus is using Stygian in real products (IDE, game engine, web
  browser, and other tools), then upstreaming improvements when possible.
- If you want to partner or co-maintain, collaboration is welcome.

## License

MIT
