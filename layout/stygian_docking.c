// stygian_docking.c - Advanced Docking & Layout Widgets
// Part of Layout System (alongside stygian_layout.c)

#include "../layout/stygian_tabs.h"
#include "../layout/stygian_tabs_internal.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
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
// Tab Bar Widget
// ============================================================================

int stygian_tab_bar(StygianContext *ctx, StygianFont font,
                    StygianTabBar *state) {
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.12f, 0.12f, 0.12f,
               1.0f);

  float tab_w = 120.0f;
  float cur_x = state->x + 2.0f;
  int clicked_tab = -1;

  for (int i = 0; i < state->tab_count; i++) {
    StygianTabItem *tab = &state->tabs[i];
    bool active = (i == state->active_tab);
    bool hovered = is_mouse_over(ctx, cur_x, state->y, tab_w, state->h);

    // Tab background
    float r = 0.15f, g = 0.15f, b = 0.15f;
    if (active) {
      r = 0.2f;
      g = 0.25f;
      b = 0.35f;
    } else if (hovered) {
      r = 0.18f;
      g = 0.18f;
      b = 0.18f;
    }

    stygian_rect_rounded(ctx, cur_x, state->y + 2, tab_w - 4, state->h - 2, r,
                         g, b, 1.0f, 4.0f);

    // Title
    if (font) {
      stygian_text(ctx, font, tab->title, cur_x + 8,
                   state->y + (state->h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    // Close button (if closable)
    if (tab->closable) {
      float close_x = cur_x + tab_w - 20;
      float close_y = state->y + (state->h - 12) / 2;
      stygian_rect(ctx, close_x, close_y, 12, 12, 0.8f, 0.3f, 0.3f, 1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      clicked_tab = i;
    }

    cur_x += tab_w;
  }

  return clicked_tab;
}

// ============================================================================
// Split Panel Widget
// ============================================================================

bool stygian_split_panel(StygianContext *ctx, StygianSplitPanel *state,
                         float *out_left_x, float *out_left_y,
                         float *out_left_w, float *out_left_h,
                         float *out_right_x, float *out_right_y,
                         float *out_right_w, float *out_right_h) {

  float splitter_size = 4.0f;
  bool changed = false;

  if (state->vertical) {
    // Vertical split (left/right)
    float split_pos = state->x + (state->w * state->split_ratio);

    // Left panel
    *out_left_x = state->x;
    *out_left_y = state->y;
    *out_left_w = split_pos - state->x;
    *out_left_h = state->h;

    // Splitter
    float splitter_x = split_pos;
    bool hovered = is_mouse_over(ctx, splitter_x - splitter_size / 2, state->y,
                                 splitter_size, state->h);

    stygian_rect(ctx, splitter_x - splitter_size / 2, state->y, splitter_size,
                 state->h, hovered ? 0.3f : 0.2f, hovered ? 0.3f : 0.2f,
                 hovered ? 0.3f : 0.2f, 1.0f);

    // Drag logic (simplified)
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      int mx, my;
      stygian_mouse_pos(win, &mx, &my);
      state->split_ratio = (mx - state->x) / state->w;
      if (state->split_ratio < 0.1f)
        state->split_ratio = 0.1f;
      if (state->split_ratio > 0.9f)
        state->split_ratio = 0.9f;
      changed = true;
    }

    // Right panel
    *out_right_x = split_pos + splitter_size;
    *out_right_y = state->y;
    *out_right_w = state->x + state->w - *out_right_x;
    *out_right_h = state->h;

  } else {
    // Horizontal split (top/bottom)
    float split_pos = state->y + (state->h * state->split_ratio);

    // Top panel
    *out_left_x = state->x;
    *out_left_y = state->y;
    *out_left_w = state->w;
    *out_left_h = split_pos - state->y;

    // Splitter
    float splitter_y = split_pos;
    bool hovered = is_mouse_over(ctx, state->x, splitter_y - splitter_size / 2,
                                 state->w, splitter_size);

    stygian_rect(ctx, state->x, splitter_y - splitter_size / 2, state->w,
                 splitter_size, hovered ? 0.3f : 0.2f, hovered ? 0.3f : 0.2f,
                 hovered ? 0.3f : 0.2f, 1.0f);

    // Drag logic
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      int mx, my;
      stygian_mouse_pos(win, &mx, &my);
      state->split_ratio = (my - state->y) / state->h;
      if (state->split_ratio < 0.1f)
        state->split_ratio = 0.1f;
      if (state->split_ratio > 0.9f)
        state->split_ratio = 0.9f;
      changed = true;
    }

    // Bottom panel
    *out_right_x = state->x;
    *out_right_y = split_pos + splitter_size;
    *out_right_w = state->w;
    *out_right_h = state->y + state->h - *out_right_y;
  }

  return changed;
}

// ============================================================================
// Menu Bar Widget
// ============================================================================

void stygian_menu_bar(StygianContext *ctx, StygianFont font,
                      StygianMenuBar *state) {
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  float cur_x = state->x + 8.0f;

  for (int i = 0; i < state->menu_count; i++) {
    const char *label = state->menu_labels[i];
    float label_w = font ? stygian_text_width(ctx, font, label, 14.0f) : 60.0f;
    float item_w = label_w + 16.0f;

    bool hovered = is_mouse_over(ctx, cur_x, state->y, item_w, state->h);

    // Highlight
    if (hovered || i == state->open_menu) {
      stygian_rect(ctx, cur_x, state->y, item_w, state->h, 0.2f, 0.2f, 0.2f,
                   1.0f);
    }

    // Label
    if (font) {
      stygian_text(ctx, font, label, cur_x + 8,
                   state->y + (state->h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    cur_x += item_w;
  }
}

// ============================================================================
// Toolbar Widget
// ============================================================================

int stygian_toolbar(StygianContext *ctx, StygianFont font,
                    StygianToolbar *state) {
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.12f, 0.12f, 0.12f,
               1.0f);

  float btn_size = state->h - 4.0f;
  float cur_x = state->x + 4.0f;
  int clicked_tool = -1;

  for (int i = 0; i < state->tool_count; i++) {
    bool active = (i == state->active_tool);
    bool hovered = is_mouse_over(ctx, cur_x, state->y + 2, btn_size, btn_size);

    // Button background
    float r = 0.15f, g = 0.15f, b = 0.15f;
    if (active) {
      r = 0.3f;
      g = 0.5f;
      b = 0.8f;
    } else if (hovered) {
      r = 0.25f;
      g = 0.25f;
      b = 0.25f;
    }

    stygian_rect_rounded(ctx, cur_x, state->y + 2, btn_size, btn_size, r, g, b,
                         1.0f, 4.0f);

    // Icon (simplified - just text)
    if (font && state->tool_icons[i]) {
      stygian_text(ctx, font, state->tool_icons[i], cur_x + 8,
                   state->y + (state->h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      clicked_tool = i;
    }

    cur_x += btn_size + 4.0f;
  }

  return clicked_tool;
}
