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

- `backends/stygian_opengl.c` - OpenGL 4.3+
- Vulkan (planned)

## TRIAD Runtime (Current)

Core runtime scaffolding for `.triad` packs now lives in `src` (not benchmark
tools):

- `src/stygian_triad.c`
- `src/stygian_triad.h`

Public API in `include/stygian.h`:

- `stygian_triad_mount`
- `stygian_triad_unmount`
- `stygian_triad_is_mounted`
- `stygian_triad_get_pack_info`
- `stygian_triad_lookup`
- `stygian_triad_hash_key`

Current scope:

- mount and index `TRIAD01` files
- hash+lookup by `glyph_hash`
- return payload metadata for next-stage decode/upload integration

Dedicated build script:

- `build_triad_runtime.bat` builds `build/triad_runtime_probe.exe`

Live demo:

- `build_chat_emoji_demo.bat` builds `build/chat_emoji_demo.exe`
- Chat input accepts `:shortcode:` (example `:emoji_u1f600:`)
- Demo resolves shortcode from mounted `.triad`, reads SVG payload, rasterizes,
  caches texture, and renders inline in chat list

## License

MIT
