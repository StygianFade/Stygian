// dock_test.c - Test application for Stygian Docking System
// Tests: panel creation, splits, tabs, splitter drag

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/stygian.h"
#include "../layout/stygian_dock.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"

// ============================================================================
// Sample Panel Render Callbacks
// ============================================================================

void render_viewport_panel(StygianDockPanel *panel, StygianContext *ctx,
                           StygianFont font, float x, float y, float w,
                           float h) {
  // Blue viewport background
  stygian_rect(ctx, x + 2, y + 2, w - 4, h - 4, 0.1f, 0.15f, 0.25f, 1.0f);

  // Grid lines
  for (float gx = x + 50; gx < x + w; gx += 50) {
    stygian_rect(ctx, gx, y, 1, h, 0.2f, 0.25f, 0.35f, 0.5f);
  }
  for (float gy = y + 50; gy < y + h; gy += 50) {
    stygian_rect(ctx, x, gy, w, 1, 0.2f, 0.25f, 0.35f, 0.5f);
  }

  if (font) {
    stygian_text(ctx, font, "3D Viewport", x + 10, y + 10, 16.0f, 0.6f, 0.7f,
                 0.9f, 1.0f);
  }
}

void render_hierarchy_panel(StygianDockPanel *panel, StygianContext *ctx,
                            StygianFont font, float x, float y, float w,
                            float h) {
  // Dark background
  stygian_rect(ctx, x + 2, y + 2, w - 4, h - 4, 0.1f, 0.1f, 0.12f, 1.0f);

  if (font) {
    stygian_text(ctx, font, "Scene Hierarchy", x + 10, y + 10, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);

    // Fake tree items
    float item_y = y + 35;
    const char *items[] = {"Root",     "  Camera", "  Light",
                           "  Player", "    Mesh", "    Collider"};
    for (int i = 0; i < 6; i++) {
      stygian_text(ctx, font, items[i], x + 10, item_y, 12.0f, 0.7f, 0.7f, 0.7f,
                   1.0f);
      item_y += 18;
    }
  }
}

void render_inspector_panel(StygianDockPanel *panel, StygianContext *ctx,
                            StygianFont font, float x, float y, float w,
                            float h) {
  // Slightly blue tint
  stygian_rect(ctx, x + 2, y + 2, w - 4, h - 4, 0.1f, 0.1f, 0.14f, 1.0f);

  if (font) {
    stygian_text(ctx, font, "Inspector", x + 10, y + 10, 14.0f, 0.8f, 0.8f,
                 0.8f, 1.0f);

    // Fake properties
    float prop_y = y + 40;
    stygian_text(ctx, font, "Transform", x + 10, prop_y, 13.0f, 0.5f, 0.7f,
                 0.9f, 1.0f);
    prop_y += 22;
    stygian_text(ctx, font, "  Position: (0, 0, 0)", x + 10, prop_y, 12.0f,
                 0.6f, 0.6f, 0.6f, 1.0f);
    prop_y += 18;
    stygian_text(ctx, font, "  Rotation: (0, 0, 0)", x + 10, prop_y, 12.0f,
                 0.6f, 0.6f, 0.6f, 1.0f);
    prop_y += 18;
    stygian_text(ctx, font, "  Scale: (1, 1, 1)", x + 10, prop_y, 12.0f, 0.6f,
                 0.6f, 0.6f, 1.0f);
  }
}

void render_console_panel(StygianDockPanel *panel, StygianContext *ctx,
                          StygianFont font, float x, float y, float w,
                          float h) {
  // Dark console background
  stygian_rect(ctx, x + 2, y + 2, w - 4, h - 4, 0.05f, 0.05f, 0.07f, 1.0f);

  if (font) {
    stygian_text(ctx, font, "Console", x + 10, y + 10, 14.0f, 0.8f, 0.8f, 0.8f,
                 1.0f);

    // Fake log messages
    float log_y = y + 35;
    stygian_text(ctx, font, "[INFO] Application started", x + 10, log_y, 11.0f,
                 0.5f, 0.8f, 0.5f, 1.0f);
    log_y += 16;
    stygian_text(ctx, font, "[INFO] Loading scene...", x + 10, log_y, 11.0f,
                 0.5f, 0.8f, 0.5f, 1.0f);
    log_y += 16;
    stygian_text(ctx, font, "[WARN] Missing texture: diffuse.png", x + 10,
                 log_y, 11.0f, 0.9f, 0.8f, 0.3f, 1.0f);
    log_y += 16;
    stygian_text(ctx, font, "[INFO] Scene loaded (6 objects)", x + 10, log_y,
                 11.0f, 0.5f, 0.8f, 0.5f, 1.0f);
  }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("=== Stygian Docking Test ===\n\n");

  // Create window
  StygianWindowConfig win_cfg = {
      .title = "Stygian Docking Test",
      .width = 1400,
      .height = 800,
      .flags = STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_OPENGL,
  };

  StygianWindow *window = stygian_window_create(&win_cfg);
  if (!window) {
    fprintf(stderr, "Failed to create window\n");
    return 1;
  }

  // Create Stygian context with OpenGL backend
  StygianConfig cfg = {.backend = STYGIAN_BACKEND_OPENGL, .window = window};
  StygianContext *ctx = stygian_create(&cfg);
  if (!ctx) {
    fprintf(stderr, "Failed to create Stygian context\n");
    return 1;
  }

  // Load font
  StygianFont font =
      stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");
  if (!font) {
    printf("Warning: Font not loaded\n");
  }

  // Initialize dock space (heap allocated for ABI stability)
  StygianDockSpace *dock = stygian_dock_create(NULL, NULL);
  if (!dock) {
    fprintf(stderr, "Failed to create dock space\n");
    return 1;
  }

  // Register panels
  uint32_t viewport_id = stygian_dock_register_panel(
      dock, "Viewport", false, render_viewport_panel, NULL);
  uint32_t hierarchy_id = stygian_dock_register_panel(
      dock, "Hierarchy", true, render_hierarchy_panel, NULL);
  uint32_t inspector_id = stygian_dock_register_panel(
      dock, "Inspector", true, render_inspector_panel, NULL);
  uint32_t console_id = stygian_dock_register_panel(dock, "Console", true,
                                                    render_console_panel, NULL);

  // Build layout: Viewport left, right side split (Hierarchy/Inspector top,
  // Console bottom)
  StygianDockNode *root = stygian_dock_get_root(dock);

  // First split: left (viewport) / right (rest)
  StygianDockNode *left_node, *right_node;
  stygian_dock_split(dock, root, STYGIAN_DOCK_SPLIT_VERTICAL, 0.7f, &left_node,
                     &right_node);

  // Add viewport to left
  stygian_dock_add_panel_to_node(dock, left_node, viewport_id);

  // Split right: top (hierarchy + inspector tabs) / bottom (console)
  StygianDockNode *top_node, *bottom_node;
  stygian_dock_split(dock, right_node, STYGIAN_DOCK_SPLIT_HORIZONTAL, 0.6f,
                     &top_node, &bottom_node);

  // Add hierarchy and inspector as tabs in top
  stygian_dock_add_panel_to_node(dock, top_node, hierarchy_id);
  stygian_dock_add_panel_to_node(dock, top_node, inspector_id);

  // Add console to bottom
  stygian_dock_add_panel_to_node(dock, bottom_node, console_id);

  printf("\nLayout created:\n");
  printf("  Left: Viewport\n");
  printf("  Right-Top: Hierarchy | Inspector (tabs)\n");
  printf("  Right-Bottom: Console\n\n");
  printf("Controls:\n");
  printf("  - Drag splitters to resize\n");
  printf("  - Click tabs to switch panels\n");
  printf("  - ESC to exit\n\n");

  // Main loop
  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    stygian_widgets_begin_frame(ctx);
    while (stygian_window_poll_event(window, &event)) {
      stygian_widgets_process_event(ctx, &event);
      if (event.type == STYGIAN_EVENT_CLOSE) {
        stygian_window_request_close(window);
      }
    }

    int width, height;
    stygian_window_get_size(window, &width, &height);

    stygian_begin_frame(ctx, width, height);

    // Background
    stygian_rect(ctx, 0, 0, (float)width, (float)height, 0.08f, 0.08f, 0.08f,
                 1.0f);

    // Update and render dock system
    stygian_dock_update(ctx, font, dock, 0, 0, (float)width, (float)height);

    stygian_end_frame(ctx);
  }

  // Cleanup
  stygian_dock_destroy(dock);
  stygian_destroy(ctx);
  stygian_window_destroy(window);

  printf("\n=== Test Complete ===\n");
  return 0;
}
