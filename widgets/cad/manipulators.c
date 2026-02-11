// manipulators.c - CAD Manipulators & Layers for Stygian
// Part of CAD Widgets Phase

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
// CAD Gizmo Controls
// ============================================================================

void stygian_cad_gizmo_controls(StygianContext *ctx, StygianFont font,
                                StygianCADGizmo *state) {
  // Panel background
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.15f,
                       0.15f, 0.15f, 1.0f, 4.0f);

  const char *labels[] = {"T", "R", "S"}; // Translate, Rotate, Scale
  float btn_w = 24.0f;
  float btn_h = 24.0f;
  float padding = 4.0f;
  float cur_x = state->x + padding;
  float cur_y = state->y + (state->h - btn_h) / 2;

  // Mode Buttons
  for (int i = 0; i < 3; i++) {
    bool selected = (state->mode == (StygianGizmoMode)i);
    bool hovered = is_mouse_over(ctx, cur_x, cur_y, btn_w, btn_h);

    float r = 0.25f, g = 0.25f, b = 0.25f;
    if (selected) {
      r = 0.3f;
      g = 0.5f;
      b = 0.8f;
    } else if (hovered) {
      r = 0.35f;
      g = 0.35f;
      b = 0.35f;
    }

    stygian_rect_rounded(ctx, cur_x, cur_y, btn_w, btn_h, r, g, b, 1.0f, 4.0f);

    if (font) {
      stygian_text(ctx, font, labels[i], cur_x + 8, cur_y + 6, 14.0f, 0.9f,
                   0.9f, 0.9f, 1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      state->mode = (StygianGizmoMode)i;
    }

    cur_x += btn_w + padding;
  }

  // Space Toggle (Global/Local)
  cur_x += padding * 2; // Separator
  float space_btn_w = 48.0f;
  bool space_hovered = is_mouse_over(ctx, cur_x, cur_y, space_btn_w, btn_h);

  stygian_rect_rounded(ctx, cur_x, cur_y, space_btn_w, btn_h,
                       space_hovered ? 0.35f : 0.25f, 0.25f, 0.25f, 1.0f, 4.0f);

  const char *space_label = state->local_space ? "LOCAL" : "GLOBAL";
  if (font) {
    stygian_text(ctx, font, space_label, cur_x + 6, cur_y + 6, 12.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  StygianWindow *win = stygian_get_window(ctx);
  if (space_hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
    // Toggle (needs debounce in real app, simplified here)
    // state->local_space = !state->local_space;
  }
}

// ============================================================================
// Layer Manager
// ============================================================================

bool stygian_layer_manager(StygianContext *ctx, StygianFont font,
                           StygianLayerManager *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Layers", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  float row_h = 24.0f;
  float content_y = state->y + 24.0f;
  bool changed = false;

  StygianLayer *layer = state->layers;
  int index = 0;

  while (layer) {
    float cur_y = content_y + (index * row_h) - state->scroll_y;

    // Culling
    if (cur_y + row_h < content_y) {
      layer = layer->next;
      index++;
      continue;
    }
    if (cur_y > state->y + state->h)
      break;

    bool selected = (state->active_layer_index == index);
    bool hovered = is_mouse_over(ctx, state->x, cur_y, state->w, row_h);

    // Row Background
    if (selected) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.3f, 0.4f,
                   1.0f);
    } else if (hovered) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.15f, 0.15f, 0.15f,
                   1.0f);
    }

    // Interaction (Selection)
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      state->active_layer_index = index;
    }

    float x_offset = state->x + 4.0f;

    // Visibility Toggle (Eye icon placeholder)
    stygian_rect(ctx, x_offset, cur_y + 6, 12, 12, layer->visible ? 0.8f : 0.3f,
                 0.8f, 0.8f, 1.0f); // Cyan if visible
    x_offset += 20.0f;

    // Lock Toggle (Lock icon placeholder)
    stygian_rect(ctx, x_offset, cur_y + 6, 12, 12, layer->locked ? 0.8f : 0.3f,
                 0.3f, 0.3f, 1.0f); // Red if locked
    x_offset += 20.0f;

    // Name
    if (font) {
      stygian_text(ctx, font, layer->name, x_offset, cur_y + 4, 14.0f, 0.9f,
                   0.9f, 0.9f, 1.0f);
    }

    layer = layer->next;
    index++;
  }

  stygian_panel_end(ctx);
  return changed;
}
