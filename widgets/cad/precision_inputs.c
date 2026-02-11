// precision_inputs.c - CAD Precision Inputs for Stygian
// Part of CAD Widgets Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>


// Helper to check hover
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Coordinate Input Widget (X, Y, Z)
// ============================================================================

bool stygian_coordinate_input(StygianContext *ctx, StygianFont font,
                              StygianCoordinateInput *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.15f,
                       0.15f, 0.15f, 1.0f, 4.0f);

  float padding = 4.0f;
  float label_w = 0.0f;

  if (state->label && font) {
    label_w = stygian_text_width(ctx, font, state->label, 14.0f) + 8.0f;
    stygian_text(ctx, font, state->label, state->x + padding,
                 state->y + (state->h - 14) / 2 + 2, 14.0f, 0.8f, 0.8f, 0.8f,
                 1.0f);
  }

  // Calculate width for each field (3 fields)
  float field_gap = 4.0f;
  float avail_w = state->w - label_w - (padding * 2);
  float field_w = (avail_w - (field_gap * 2)) / 3.0f;

  bool changed = false;
  const char *labels[] = {"X", "Y", "Z"};
  float *values[] = {&state->x_val, &state->y_val, &state->z_val};
  float colors[][3] = {
      {0.8f, 0.3f, 0.3f}, {0.3f, 0.8f, 0.3f}, {0.3f, 0.3f, 0.8f}}; // R, G, B

  float cur_x = state->x + label_w + padding;
  float cur_y = state->y + padding;
  float field_h = state->h - (padding * 2);

  for (int i = 0; i < 3; i++) {
    // Axis label background
    float axis_w = 16.0f;
    stygian_rect_rounded(ctx, cur_x, cur_y, axis_w, field_h, colors[i][0],
                         colors[i][1], colors[i][2], 1.0f, 2.0f);

    if (font) {
      stygian_text(ctx, font, labels[i], cur_x + 4,
                   cur_y + (field_h - 12) / 2 + 2, 12.0f, 0.1f, 0.1f, 0.1f,
                   1.0f);
    }

    // Value field background
    float input_x = cur_x + axis_w;
    float input_w = field_w - axis_w;

    // Simple drag logic (similar to slider but infinite)
    bool hovered = is_mouse_over(ctx, input_x, cur_y, input_w, field_h);

    // Visual state
    float r = 0.1f, g = 0.1f, b = 0.1f;
    if (hovered) {
      r = 0.2f;
      g = 0.2f;
      b = 0.2f;
    }

    stygian_rect_rounded(ctx, input_x, cur_y, input_w, field_h, r, g, b, 1.0f,
                         2.0f);

    // Render Value
    if (font) {
      char val_str[32];
      snprintf(val_str, sizeof(val_str), "%.2f", *values[i]);
      stygian_text(ctx, font, val_str, input_x + 4,
                   cur_y + (field_h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    // Interaction (Simple increment on click for demo)
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      // Very simple click-to-increment for demo purposes
      // Real CAD input would have text entry + drag
      *values[i] += 0.01f;
      changed = true;
    }

    cur_x += field_w + field_gap;
  }

  stygian_panel_end(ctx);
  return changed;
}

// ============================================================================
// Snap Settings Widget
// ============================================================================

bool stygian_snap_settings(StygianContext *ctx, StygianFont font,
                           StygianSnapSettings *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f,
                       0.1f, 1.0f, 4.0f);

  // Title
  if (font) {
    stygian_text(ctx, font, "Snapping", state->x + 8, state->y + 24, 14.0f,
                 0.8f, 0.8f, 0.8f, 1.0f);
  }

  float padding = 8.0f;
  float cur_y = state->y + 32.0f;
  bool changed = false;

  // Grid Snap Toggle
  if (stygian_checkbox(ctx, font, "Grid Snap", state->x + padding, cur_y,
                       &state->grid_snap))
    changed = true;
  cur_y += 24.0f;

  // Angle Snap Toggle
  if (stygian_checkbox(ctx, font, "Angle Snap", state->x + padding, cur_y,
                       &state->angel_snap))
    changed = true;
  cur_y += 24.0f;

  // Object Snap Toggle
  if (stygian_checkbox(ctx, font, "Object Snap", state->x + padding, cur_y,
                       &state->object_snap))
    changed = true;

  stygian_panel_end(ctx);
  return changed;
}
