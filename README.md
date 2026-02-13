# Stygian

GPU-accelerated SDF UI rendering library.

## Features

- Signed Distance Field (SDF) based rendering
- MTSDF text with subpixel accuracy
- Custom window chrome and titlebar
- Hardware-accelerated via OpenGL 4.3+
- Single-header friendly

## Quick Start

```c
#include "stygian.h"

StygianContext* ctx = stygian_create(window_handle);
stygian_load_font(ctx, "font.png", "font.json");

// Render loop
stygian_begin_frame(ctx, width, height);
stygian_draw_rect(ctx, 10, 10, 200, 100, color, radius, STYGIAN_RECT);
stygian_draw_text(ctx, "Hello", 20, 50, 16.0f, white);
stygian_end_frame(ctx);

stygian_destroy(ctx);
```

## Backends

- `backends/stygian_ap_gl.c` - OpenGL access point
- `backends/stygian_ap_vk.c` - Vulkan access point

## Build Targets

Manifest-driven build metadata lives in:

- `compile/targets.json`

Platform runners:

- Windows: `compile/windows/build.ps1`
- Linux: `compile/linux/build.sh`
- macOS: `compile/macos/build.sh`

Windows convenience wrappers remain:

- `build_text_editor_mini.bat`
- `build_calculator_mini.bat`
- `build_calendar_mini.bat`
- `build_perf_pathological_suite.bat`
- `build_mini_apps_all.bat`

Perf gate command:

- `run_perf_gates.bat`

## License

MIT
