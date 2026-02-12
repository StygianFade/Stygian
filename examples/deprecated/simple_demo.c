// stygian_simple_demo.c - Simple Stygian Library Usage Demo
// Shows text rendering, animations, and metaball menu bar
#include "../include/stygian.h"
#include "../window/stygian_window.h"
#include <math.h>
#include <stdio.h>
#include <windows.h>

int main() {
  // Create window using Stygian window layer
  StygianWindow *win =
      stygian_window_create_simple(1024, 640, "Stygian Simple Demo");
  if (!win) {
    printf("Failed to create window\n");
    return 1;
  }

  // Create Stygian context with the window
  StygianConfig config = {.backend = STYGIAN_BACKEND_OPENGL,
                          .max_elements = 1024,
                          .max_textures = 64,
                          .window = win};

  StygianContext *ctx = stygian_create(&config);
  if (!ctx) {
    printf("Failed to create Stygian context\n");
    stygian_window_destroy(win);
    return 1;
  }

  // Load font (if atlas files exist)
  // StygianFont font = stygian_font_load(ctx, "assets/inter.png",
  // "assets/inter.json");
  StygianFont font = 0; // For now, no font

  float time = 0.0f;

  // Main loop - use Stygian window API
  while (!stygian_window_should_close(win)) {
    // Process events
    StygianEvent event;
    while (stygian_window_poll_event(win, &event)) {
      // Handle events if needed
    }

    int width, height;
    stygian_window_get_size(win, &width, &height);

    // Begin frame
    stygian_begin_frame(ctx, width, height);

    // Animate time
    time += 0.016f; // ~60 FPS

    // Window body with gradient border (type 5)
    StygianElement window_body = stygian_element(ctx);
    stygian_set_bounds(ctx, window_body, 0, 0, (float)width, (float)height);
    stygian_set_type(ctx, window_body, STYGIAN_WINDOW_BODY);
    stygian_set_color(ctx, window_body, 0.5f, 0.5f, 0.5f, 1.0f); // Body
    stygian_set_border(ctx, window_body, 0.235f, 0.259f, 0.294f,
                       1.0f); // Border
    stygian_set_radius(ctx, window_body, 10, 10, 10, 10);

    // Main panel (dark content area)
    stygian_rect_rounded(ctx, 10, 56, width - 20, height - 122, 0.10f, 0.10f,
                         0.11f, 1.0f, 8);

    // Metaball menu bar - ANIMATED
    float blend_anim = 13.0f + sinf(time * 1.5f) * 5.0f;
    StygianElement menu = stygian_element(ctx);
    stygian_set_bounds(ctx, menu, 80, 0, 350, 40);
    stygian_set_type(ctx, menu, STYGIAN_METABALL_LEFT);
    stygian_set_color(ctx, menu, 0.18f, 0.19f, 0.21f, 1.0f);
    stygian_set_radius(ctx, menu, 6, 6, 6, 6);
    stygian_set_blend(ctx, menu, blend_anim);

    // Control buttons (top right)
    int btn_y = 10;
    int btn_size = 25;
    int btn_spacing = 45;
    int base_x = width - 18;

    // Close button (red)
    stygian_rect_rounded(ctx, base_x - btn_size, btn_y, btn_size, btn_size,
                         0.95f, 0.3f, 0.3f, 1.0f, 4);

    // Maximize button (green)
    stygian_rect_rounded(ctx, base_x - btn_size - btn_spacing, btn_y, btn_size,
                         btn_size, 0.3f, 0.85f, 0.4f, 1.0f, 4);

    // Minimize button (yellow)
    stygian_rect_rounded(ctx, base_x - btn_size - btn_spacing * 2, btn_y,
                         btn_size, btn_size, 0.95f, 0.8f, 0.2f, 1.0f, 4);

    // Text rendering (if font loaded)
    if (font) {
      stygian_text(ctx, font, "File  Edit  View", 95, 12, 14, 0.9f, 0.9f, 0.9f,
                   1.0f);

      stygian_text(ctx, font, "Stygian Demo - GPU SDF UI", 25, 66, 12, 0.7f,
                   0.7f, 0.7f, 1.0f);

      char time_str[64];
      sprintf(time_str, "Time: %.2fs | Metaball breathing", time);
      stygian_text(ctx, font, time_str, 25, 86, 12, 0.6f, 0.6f, 0.6f, 1.0f);
    }

    // End frame (single draw call)
    stygian_end_frame(ctx);

    // Swap buffers
    stygian_window_swap_buffers(win);

    Sleep(16); // ~60 FPS
  }

  stygian_destroy(ctx);
  stygian_window_destroy(win);
  return 0;
}
