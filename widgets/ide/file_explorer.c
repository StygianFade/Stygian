// file_explorer.c - File Navigation Widgets for Stygian
// Part of IDE Panels Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h" // For clipboard/path utils if needed
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Helper Functions
// ============================================================================

static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

static bool is_clicked(StygianContext *ctx, float x, float y, float w,
                       float h) {
  StygianWindow *win = stygian_get_window(ctx);
  if (is_mouse_over(ctx, x, y, w, h)) {
    // Simple click detection - ideal would be tracking down/up sequence
    return stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
    // Note: Real implementation needs state tracking like stygian_button to
    // avoid repeat
  }
  return false;
}

// ============================================================================
// File Explorer Widget
// ============================================================================

// Mock file tree for demonstration/initial implementation
// In a real app, this would query the filesystem
static void render_file_node(StygianContext *ctx, StygianFont font,
                             StygianFileExplorer *state, const char *name,
                             bool is_dir, int depth, float *y_offset) {

  float item_h = 24.0f;
  float indent = 16.0f;
  float x = state->x;
  float y = state->y + *y_offset - state->scroll_y;
  float w = state->w;

  // Virtual culling (simple)
  if (y + item_h < state->y || y > state->y + state->h) {
    *y_offset += item_h;
    return;
  }

  // Interaction
  bool hovered = is_mouse_over(ctx, x, y, w, item_h);

  // Background
  if (hovered) {
    stygian_rect(ctx, x, y, w, item_h, 0.25f, 0.25f, 0.25f, 1.0f);
  }

  // Selection highlight
  // (Simple string match for demo)
  if (state->selected_path[0] && strstr(state->selected_path, name)) {
    stygian_rect(ctx, x, y, w, item_h, 0.2f, 0.3f, 0.5f, 0.8f);
  }

  // Icon (placeholder)
  float icon_x = x + 4 + (depth * indent);
  float icon_size = 16.0f;
  if (is_dir) {
    // Folder icon (yellow-ish box)
    stygian_rect(ctx, icon_x, y + 4, icon_size, icon_size, 0.8f, 0.7f, 0.2f,
                 1.0f);
  } else {
    // File icon (white-ish sheet)
    stygian_rect(ctx, icon_x, y + 4, icon_size, icon_size, 0.7f, 0.7f, 0.7f,
                 1.0f);
  }

  // Text
  if (font) {
    float text_x = icon_x + icon_size + 8;
    stygian_text(ctx, font, name, text_x, y + 4, 14.0f, 0.9f, 0.9f, 0.9f, 1.0f);
  }

  *y_offset += item_h;
}

bool stygian_file_explorer(StygianContext *ctx, StygianFont font,
                           StygianFileExplorer *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.12f, 0.12f, 0.12f,
               1.0f);

  float y_offset = 0;

  // Mock data rendering
  // Root
  render_file_node(ctx, font, state, state->root_path ? state->root_path : "/",
                   true, 0, &y_offset);

  // Mock Children (simulating an expanded tree)
  render_file_node(ctx, font, state, "src", true, 1, &y_offset);
  render_file_node(ctx, font, state, "include", true, 1, &y_offset);
  render_file_node(ctx, font, state, "main.c", false, 1, &y_offset);
  render_file_node(ctx, font, state, "utils.h", false, 1, &y_offset);

  // Nested
  render_file_node(ctx, font, state, "widgets", true, 2,
                   &y_offset); // Under src
  render_file_node(ctx, font, state, "file_explorer.c", false, 3, &y_offset);

  stygian_panel_end(ctx);
  return false; // Toggle/Select logic would go here
}

// ============================================================================
// Breadcrumb Widget
// ============================================================================

bool stygian_breadcrumb(StygianContext *ctx, StygianFont font,
                        StygianBreadcrumb *state, char *out_path, int max_len) {
  if (!state->path || !state->path[0])
    return false;

  // Make a copy to tokenize safely (or parse manually)
  // For immediate mode, we'll parse on the fly

  float cur_x = state->x;
  float cur_y = state->y;
  float h = state->h > 0 ? state->h : 24.0f;

  const char *ptr = state->path;
  const char *start = ptr;

  bool clicked_any = false;

  // Background container
  // stygian_rect(ctx, state->x, state->y, state->w, h, 0.15f, 0.15f,
  // 0.15f, 1.0f);

  while (*ptr) {
    if (*ptr == state->separator || *ptr == '/' || *ptr == '\\' ||
        *(ptr + 1) == 0) {
      int len = (int)(ptr - start);
      if (*(ptr + 1) == 0 && *ptr != state->separator && *ptr != '/' &&
          *ptr != '\\')
        len++;

      if (len > 0) {
        // Render segment
        char segment[64];
        if (len >= 64)
          len = 63;
        memcpy(segment, start, len);
        segment[len] = 0;

        float text_w = stygian_text_width(ctx, font, segment, 14.0f);
        float item_w = text_w + 16.0f; // Padding

        // Interaction
        bool hovered = is_mouse_over(ctx, cur_x, cur_y, item_w, h);
        if (hovered) {
          stygian_rect_rounded(ctx, cur_x, cur_y + 2, item_w, h - 4, 0.3f, 0.3f,
                               0.3f, 1.0f, 4.0f);

          // Check click (simple)
          StygianWindow *win = stygian_get_window(ctx);
          if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
            // Build path up to this segment
            int path_len = (int)(ptr - state->path);
            if (*(ptr + 1) == 0)
              path_len = (int)strlen(state->path);

            if (path_len < max_len) {
              memcpy(out_path, state->path, path_len);
              out_path[path_len] = 0;
              clicked_any = true;
            }
          }
        }

        stygian_text(ctx, font, segment, cur_x + 8, cur_y + (h - 14) / 2 + 2,
                     14.0f, 0.9f, 0.9f, 0.9f, 1.0f);

        cur_x += item_w;

        // Render separator
        if (*(ptr + 1) != 0 || *ptr == state->separator) {
          char sep_str[2] = {state->separator ? state->separator : '>', 0};
          stygian_text(ctx, font, sep_str, cur_x, cur_y + (h - 14) / 2 + 2,
                       14.0f, 0.5f, 0.5f, 0.5f, 1.0f);
          cur_x += 16.0f;
        }
      }

      start = ptr + 1;
    }
    ptr++;
  }

  return clicked_any;
}
