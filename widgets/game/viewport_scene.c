// viewport_scene.c - Game Engine Viewport & Scene Widgets
// Part of Game Engine Widgets Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>


// Helper
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Scene Viewport Widget
// ============================================================================

void stygian_scene_viewport(StygianContext *ctx, StygianSceneViewport *state) {
  // Border/Frame
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.05f, 0.05f, 0.05f,
               1.0f);

  // If external texture provided, render it
  if (state->framebuffer_texture != 0) {
    stygian_image(ctx, state->framebuffer_texture, state->x + 2, state->y + 2,
                  state->w - 4, state->h - 4);
  } else {
    // Placeholder checkerboard or gradient
    stygian_rect(ctx, state->x + 2, state->y + 2, state->w - 4, state->h - 4,
                 0.1f, 0.1f, 0.15f, 1.0f);
  }

  // Overlays (Grid, Gizmo indicators would go here in real impl)
  // For now, just show labels if enabled
}

// ============================================================================
// Scene Hierarchy Widget
// ============================================================================

static void render_scene_node(StygianContext *ctx, StygianFont font,
                              StygianSceneHierarchy *state,
                              StygianSceneNode *node, int depth,
                              float *y_offset) {
  if (!node)
    return;

  float row_h = 20.0f;
  float indent = 16.0f;
  float x = state->x + 4 + (depth * indent);
  float y = state->y + 24 + *y_offset - state->scroll_y;

  // Culling
  if (y + row_h < state->y + 24 || y > state->y + state->h) {
    *y_offset += row_h;
    return;
  }

  bool hovered = is_mouse_over(ctx, state->x, y, state->w, row_h);

  // Background
  if (node->selected) {
    stygian_rect(ctx, state->x, y, state->w, row_h, 0.2f, 0.3f, 0.5f, 1.0f);
  } else if (hovered) {
    stygian_rect(ctx, state->x, y, state->w, row_h, 0.15f, 0.15f, 0.15f, 1.0f);
  }

  // Visibility toggle (eye icon placeholder)
  float icon_x = x;
  stygian_rect(ctx, icon_x, y + 4, 12, 12, node->visible ? 0.8f : 0.3f, 0.8f,
               0.8f, 1.0f);

  // Name
  if (font) {
    stygian_text(ctx, font, node->name, icon_x + 16, y + 3, 13.0f, 0.9f, 0.9f,
                 0.9f, 1.0f);
  }

  *y_offset += row_h;

  // Render children recursively
  if (node->children) {
    render_scene_node(ctx, font, state, node->children, depth + 1, y_offset);
  }

  // Next sibling
  if (node->next) {
    render_scene_node(ctx, font, state, node->next, depth, y_offset);
  }
}

bool stygian_scene_hierarchy(StygianContext *ctx, StygianFont font,
                             StygianSceneHierarchy *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Scene", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  float y_offset = 0;
  if (state->root) {
    render_scene_node(ctx, font, state, state->root, 0, &y_offset);
  }

  stygian_panel_end(ctx);
  return false; // Selection logic would go here
}

// ============================================================================
// Inspector Widget
// ============================================================================

bool stygian_inspector(StygianContext *ctx, StygianFont font,
                       StygianInspector *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    const char *title = state->object_name ? state->object_name : "Inspector";
    stygian_text(ctx, font, title, state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  float row_h = 24.0f;
  float content_y = state->y + 28.0f;
  bool changed = false;

  for (int i = 0; i < state->property_count; i++) {
    float cur_y = content_y + (i * row_h) - state->scroll_y;

    if (cur_y + row_h < content_y)
      continue;
    if (cur_y > state->y + state->h)
      break;

    StygianProperty *prop = &state->properties[i];

    // Property name (left)
    if (font) {
      stygian_text(ctx, font, prop->name, state->x + 8, cur_y + 4, 13.0f, 0.7f,
                   0.7f, 0.7f, 1.0f);
    }

    // Property value (right) - simplified, just display
    if (font) {
      float name_w = stygian_text_width(ctx, font, prop->name, 13.0f);
      stygian_text(ctx, font, prop->value, state->x + 16 + name_w, cur_y + 4,
                   13.0f, 0.9f, 0.9f, 0.9f, 1.0f);
    }
  }

  stygian_panel_end(ctx);
  return changed;
}
