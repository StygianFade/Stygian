// output_panel.c - Output & Diagnostic Widgets for Stygian
// Part of IDE Panels Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>

// Helper (duplicated for now, should be in common utils)
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Output Panel Widget
// ============================================================================

void stygian_output_panel(StygianContext *ctx, StygianFont font,
                          StygianOutputPanel *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background (Terminal black)
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.08f, 0.08f, 0.08f,
               1.0f);

  // Header
  float header_h = 24.0f;
  stygian_rect(ctx, state->x, state->y, state->w, header_h, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, state->title ? state->title : "Output",
                 state->x + 8, state->y + 4, 14.0f, 0.8f, 0.8f, 0.8f, 1.0f);
  }

  // Content area
  float content_y = state->y + header_h + 4;
  float row_h = 18.0f;

  // Simple line rendering
  if (state->text_buffer && font) {
    const char *ptr = state->text_buffer;
    const char *line_start = ptr;
    float cur_y = content_y - state->scroll_y;

    while (*ptr) {
      if (*ptr == '\n' || *ptr == 0) {
        int len = (int)(ptr - line_start);
        if (len > 0) {
          char line[256];
          if (len >= 256)
            len = 255;
          memcpy(line, line_start, len);
          line[len] = 0;

          // Simple visibility check
          if (cur_y + row_h > content_y && cur_y < state->y + state->h) {
            // TODO: Parse ANSI color codes here
            // For now, render plain white/grey
            stygian_text(ctx, font, line, state->x + 8, cur_y, 14.0f, 0.8f,
                         0.8f, 0.8f, 1.0f);
          }
        }
        cur_y += row_h;
        line_start = ptr + 1;
      }
      if (*ptr == 0)
        break;
      ptr++;
    }
  }

  stygian_panel_end(ctx);
}

// ============================================================================
// Problems Panel Widget
// ============================================================================

bool stygian_problems_panel(StygianContext *ctx, StygianFont font,
                            StygianProblemsPanel *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  float header_h = 24.0f;
  stygian_rect(ctx, state->x, state->y, state->w, header_h, 0.18f, 0.18f, 0.18f,
               1.0f);
  if (font) {
    char title[64];
    snprintf(title, sizeof(title), "Problems (%d)", state->problem_count);
    stygian_text(ctx, font, title, state->x + 8, state->y + 4, 14.0f, 0.9f,
                 0.9f, 0.9f, 1.0f);
  }

  float content_y = state->y + header_h;
  float row_h = 24.0f;
  bool item_clicked = false;

  for (int i = 0; i < state->problem_count; i++) {
    float cur_y = content_y + (i * row_h) - state->scroll_y;

    // Culling
    if (cur_y + row_h < content_y)
      continue;
    if (cur_y > state->y + state->h)
      break;

    StygianProblem *p = &state->problems[i];

    // Hover/Selection
    bool hovered = is_mouse_over(ctx, state->x, cur_y, state->w, row_h);
    bool selected = (i == state->selected_index);

    if (selected) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.3f, 0.5f,
                   1.0f);
    } else if (hovered) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.2f, 0.2f,
                   1.0f);
      StygianWindow *win = stygian_get_window(ctx);
      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        state->selected_index = i;
        item_clicked = true;
      }
    }

    // Icon/Severity Color
    float r = 0.8f, g = 0.8f, b = 0.8f;
    const char *icon = "i";
    if (p->severity == 2) {
      r = 0.9f;
      g = 0.3f;
      b = 0.3f;
      icon = "x";
    } // Error
    else if (p->severity == 1) {
      r = 0.9f;
      g = 0.8f;
      b = 0.2f;
      icon = "!";
    } // Warning

    if (font) {
      // Icon
      stygian_text(ctx, font, icon, state->x + 8, cur_y + 4, 14.0f, r, g, b,
                   1.0f);

      // Message
      stygian_text(ctx, font, p->message, state->x + 30, cur_y + 4, 14.0f, 0.9f,
                   0.9f, 0.9f, 1.0f);

      // File location (right aligned or after message)
      char loc[64];
      snprintf(loc, sizeof(loc), "%s:%d", p->file, p->line);
      float loc_w = stygian_text_width(ctx, font, loc, 14.0f);
      stygian_text(ctx, font, loc, state->x + state->w - loc_w - 8, cur_y + 4,
                   14.0f, 0.5f, 0.5f, 0.5f, 1.0f);
    }
  }

  stygian_panel_end(ctx);
  return item_clicked;
}
