# Quickstart

This sample is `examples/quickwindow.c`.

## 1. Minimal Compilable Program

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

## 2. Windows Quick Test (copy/paste)

1. Build shader outputs:
   - `compile\windows\build_shaders.bat`
2. Build quickwindow using the novice default:
   - `compile\windows\build.bat`
3. Run:
   - `build\quickwindow.exe`

Optional tiny tests:

- Borderless window:
  - `compile\windows\build_quickwindow_borderless.bat`
  - `build\quickwindow_borderless.exe`
- Custom titlebar window:
  - `compile\windows\build_quickwindow_custom_titlebar.bat`
  - `build\quickwindow_custom_titlebar.exe`
- Custom titlebar window (Vulkan):
  - `compile\windows\build_quickwindow_custom_titlebar_vk.bat`
  - `build\quickwindow_custom_titlebar_vk.exe`

## 3. Windows Borderless Maximize Behavior

Borderless maximize on Windows is work-area maximize (taskbar remains visible),
not true fullscreen.

If you want fullscreen-style monitor coverage, that is a different mode and not
the default maximize behavior.

On Windows, OpenGL borderless main windows keep strict `WS_POPUP` semantics
with manual maximize/restore sizing in core Win32 code (`window/platform/stygian_win32.c`).

Vulkan borderless path remains unchanged in this iteration.

`quickwindow_custom_titlebar` uses an event-paced loop to avoid maximize hitch
behavior in unstable OpenGL vsync environments.

## 4. Titlebar Extensibility API (Win32-first)

Use the core window APIs for custom chrome behavior instead of app-local Win32
message hacks:

- `stygian_window_get_titlebar_hints`
- `stygian_window_set_titlebar_behavior`
- `stygian_window_begin_system_move`
- `stygian_window_titlebar_double_click`
- `stygian_window_get_titlebar_menu_actions`
- `stygian_window_apply_titlebar_menu_action`
- `stygian_window_set_fullscreen` / `stygian_window_is_fullscreen`

Default policy:

- Titlebar double-click = maximize/restore.
- Optional fullscreen toggle policy is configurable per window.

Platform status:

- Win32 implements the behavior.
- X11/Wayland/Cocoa currently expose fallback contract behavior until native
  backends are completed.

## 5. Backend Switch Rules (must all match)

1. `StygianConfig.backend` in source.
2. Window render flag in source (`STYGIAN_WINDOW_OPENGL` or
   `STYGIAN_WINDOW_VULKAN`).
3. Build target backend in `compile/targets.json` (`"gl"` or `"vk"`).

For quickwindow, switch by target:

- OpenGL:
   - `compile\windows\build_quickwindow.bat`
- Vulkan:
   - `compile\windows\build_quickwindow_vk.bat`

Changing only the backend enum in C source can still compile while remaining
mismatched at runtime.

## 6. Build Entrypoints

Manifest source of truth:
- `compile/targets.json`

Cross-platform runners:
- Windows: `compile/run.ps1 -Target quickwindow`
- Linux/macOS: `compile/run.sh --target quickwindow`

Windows convenience wrapper scripts:
- `compile/windows/build.bat` (default quickwindow)
- `compile/windows/build_quickwindow.bat`
- `compile/windows/build_quickwindow_vk.bat`
- `compile/windows/build_quickwindow_borderless.bat`
- `compile/windows/build_quickwindow_custom_titlebar.bat`
- `compile/windows/build_quickwindow_custom_titlebar_vk.bat`
- `compile/windows/build_quick_smoke.bat`
- `compile/windows/build_text_editor_mini.bat`
- `compile/windows/build_calculator_mini.bat`
- `compile/windows/build_calendar_mini.bat`
- VK variants with `_vk.bat` in `compile/windows/`
- matrix build: `compile/windows/build_mini_apps_all.bat`
