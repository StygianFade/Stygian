# Stygian - GPU-Resident UI Library

**Stygian** is a high-performance, GPU-resident UI library designed for real-time applications. It renders all UI elements using Signed Distance Fields (SDF) in a single draw call.

## Features

- **Single Draw Call** - All UI rendered via instanced SSBO
- **Resolution Independent** - SDF graphics scale perfectly at any DPI
- **GPU-Resident Elements** - 256-byte elements live in VRAM
- **MTSDF Text** - Crisp text at any size using multi-channel SDF
- **Metaball Effects** - Smooth organic transitions between shapes

## Quick Start

```c
#include "include/stygian.h"

int main() {
    StygianConfig cfg = {
        .backend = STYGIAN_BACKEND_OPENGL,
        .max_elements = 1024
    };
    
    StygianContext *ctx = stygian_create(&cfg);
    
    // Main loop
    while (!should_close) {
        stygian_begin_frame(ctx, width, height);
        
        // Draw a rounded rectangle
        stygian_rect_rounded(ctx, 10, 10, 200, 100, 
                             0.2f, 0.3f, 0.4f, 1.0f, 8.0f);
        
        stygian_end_frame(ctx); // Single GPU draw call
    }
    
    stygian_destroy(ctx);
}
```

## Directory Structure

```
stygian/
├── include/stygian.h      # Public API
├── src/                   # Core implementation
├── backends/              # OpenGL, Vulkan (planned)
├── widgets/               # Button, Slider, TextInput
├── layout/                # Flexbox-style layout engine
├── examples/              # Demo programs
├── docs/                  # Documentation
└── tests/                 # Unit tests
```

## Requirements

- OpenGL 4.3+ (for SSBO support)
- Windows 10+ (Win32 backend)

## Building

```batch
build.bat
```

## License

MIT License

## Benchmarking

- TRIAD benchmark protocol:
  - `means/native/stygian/docs/triad_benchmark_protocol.md`
