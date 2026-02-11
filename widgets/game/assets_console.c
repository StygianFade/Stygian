// assets_console.c - Game Engine Assets & Console Widgets
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
// Asset Browser Widget
// ============================================================================

bool stygian_asset_browser(StygianContext *ctx, StygianFont font,
                           StygianAssetBrowser *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.08f, 0.08f, 0.08f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.12f, 0.12f, 0.12f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Assets", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  // Grid layout for assets (simplified to list for now)
  float item_h = 60.0f;
  float content_y = state->y + 28.0f;
  bool clicked = false;

  for (int i = 0; i < state->asset_count; i++) {
    float cur_y = content_y + (i * item_h) - state->scroll_y;

    if (cur_y + item_h < content_y)
      continue;
    if (cur_y > state->y + state->h)
      break;

    StygianAsset *asset = &state->assets[i];
    bool selected = (i == state->selected_index);
    bool hovered =
        is_mouse_over(ctx, state->x + 4, cur_y, state->w - 8, item_h - 4);

    // Background
    float r = 0.1f, g = 0.1f, b = 0.1f;
    if (selected) {
      r = 0.2f;
      g = 0.3f;
      b = 0.5f;
    } else if (hovered) {
      r = 0.15f;
      g = 0.15f;
      b = 0.15f;
    }

    stygian_rect_rounded(ctx, state->x + 4, cur_y, state->w - 8, item_h - 4, r,
                         g, b, 1.0f, 4.0f);

    // Thumbnail placeholder (colored square based on type)
    float thumb_size = 48.0f;
    float thumb_colors[][3] = {
        {0.8f, 0.3f, 0.3f}, // Texture
        {0.3f, 0.8f, 0.3f}, // Model
        {0.3f, 0.3f, 0.8f}, // Material
        {0.8f, 0.8f, 0.3f}  // Script
    };
    int type_idx = asset->type < 4 ? asset->type : 0;

    stygian_rect_rounded(ctx, state->x + 8, cur_y + 4, thumb_size, thumb_size,
                         thumb_colors[type_idx][0], thumb_colors[type_idx][1],
                         thumb_colors[type_idx][2], 1.0f, 4.0f);

    // Name
    if (font) {
      stygian_text(ctx, font, asset->name, state->x + 12 + thumb_size,
                   cur_y + 20, 13.0f, 0.9f, 0.9f, 0.9f, 1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      state->selected_index = i;
      clicked = true;
    }
  }

  stygian_panel_end(ctx);
  return clicked;
}

// ============================================================================
// Console Log Widget
// ============================================================================

void stygian_console_log(StygianContext *ctx, StygianFont font,
                         StygianConsoleLog *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background (dark console)
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.05f, 0.05f, 0.05f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.08f, 0.08f, 0.08f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Console", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  // Log content
  float row_h = 16.0f;
  float content_y = state->y + 28.0f;

  if (state->log_buffer && font) {
    const char *ptr = state->log_buffer;
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

          // Visibility check
          if (cur_y + row_h > content_y && cur_y < state->y + state->h) {
            // Color based on prefix (simple log level detection)
            float r = 0.8f, g = 0.8f, b = 0.8f;
            if (strstr(line, "[ERROR]")) {
              r = 0.9f;
              g = 0.3f;
              b = 0.3f;
            } else if (strstr(line, "[WARN]")) {
              r = 0.9f;
              g = 0.8f;
              b = 0.2f;
            } else if (strstr(line, "[INFO]")) {
              r = 0.3f;
              g = 0.8f;
              b = 0.9f;
            }

            stygian_text(ctx, font, line, state->x + 8, cur_y, 12.0f, r, g, b,
                         1.0f);
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
