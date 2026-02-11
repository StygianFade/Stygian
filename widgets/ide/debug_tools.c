// debug_tools.c - Debugging Widgets for Stygian
// Part of IDE Panels Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>


// Helper (duplicated)
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Debug Toolbar Widget
// ============================================================================

void stygian_debug_toolbar(StygianContext *ctx, StygianFont font,
                           StygianDebugToolbar *state) {
  // Toolbar background (floating pill shape usually, but here just a panel)
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.2f, 0.2f,
                       0.2f, 1.0f, 4.0f);

  const char *icons[] = {
      state->is_paused ? ">" : "||", // Continue/Pause
      "->",                          // Step Over
      "v",                           // Step Into
      "^",                           // Step Out
      "X"                            // Stop
  };

  const char *tooltips[] = {"Continue", "Step Over", "Step Into", "Step Out",
                            "Stop"};

  float btn_w = 32.0f;
  float btn_h = state->h - 4.0f;
  float cur_x = state->x + 4.0f;
  float cur_y = state->y + 2.0f;

  for (int i = 0; i < 5; i++) {
    // Simple button logic inline for custom styling
    bool hovered = is_mouse_over(ctx, cur_x, cur_y, btn_w, btn_h);
    float r = 0.25f, g = 0.25f, b = 0.25f;

    if (hovered) {
      r = 0.35f;
      g = 0.35f;
      b = 0.35f;
      StygianWindow *win = stygian_get_window(ctx);
      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        r = 0.15f;
        // Callback
        if (state->on_action)
          state->on_action(i);
      }
    }

    // Color coding for Stop button
    if (i == 4) {
      r += 0.1f;
      b -= 0.1f;
      g -= 0.1f;
    } // Reddish
    // Color for Play
    if (i == 0 && state->is_paused) {
      g += 0.2f;
    } // Greenish

    stygian_rect_rounded(ctx, cur_x, cur_y, btn_w, btn_h, r, g, b, 1.0f, 4.0f);

    if (font) {
      float text_w = stygian_text_width(ctx, font, icons[i], 16.0f);
      stygian_text(ctx, font, icons[i], cur_x + (btn_w - text_w) / 2,
                   cur_y + (btn_h - 16) / 2 + 2, 16.0f, 0.9f, 0.9f, 0.9f, 1.0f);
    }

    cur_x += btn_w + 4.0f;
  }
}

// ============================================================================
// Call Stack Widget
// ============================================================================

bool stygian_call_stack(StygianContext *ctx, StygianFont font,
                        StygianCallStack *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font)
    stygian_text(ctx, font, "Call Stack", state->x + 8, state->y + 4, 14.0f,
                 0.8f, 0.8f, 0.8f, 1.0f);

  float row_h = 20.0f;
  float content_y = state->y + 24.0f;
  bool clicked = false;

  for (int i = 0; i < state->frame_count; i++) {
    float cur_y = content_y + i * row_h;
    if (cur_y > state->y + state->h)
      break;

    StygianStackFrame *f = &state->frames[i];
    bool selected = (i == state->selected_frame);
    bool hovered = is_mouse_over(ctx, state->x, cur_y, state->w, row_h);

    if (selected) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.3f, 0.2f,
                   1.0f); // Greenish highlight
    } else if (hovered) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.2f, 0.2f,
                   1.0f);
      StygianWindow *win = stygian_get_window(ctx);
      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        state->selected_frame = i;
        clicked = true;
      }
    }

    if (font) {
      char buf[128];
      snprintf(buf, sizeof(buf), "%s  %s:%d", f->function, f->file, f->line);
      stygian_text(ctx, font, buf, state->x + 8, cur_y + 3, 13.0f, 0.9f, 0.9f,
                   0.9f, 1.0f);
    }
  }

  stygian_panel_end(ctx);
  return clicked;
}
