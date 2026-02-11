// stygian_tabs.c - Production Tab System Implementation
// Chrome-like draggable, reorderable, closable tabs

#include "stygian_tabs.h"
#include "../include/stygian.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include "stygian_tabs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: Check if point is in rect
static bool point_in_rect(int px, int py, float x, float y, float w, float h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}

// ============================================================================
// Tab Bar Implementation
// ============================================================================

StygianTabBar *stygian_tab_bar_create(float x, float y, float w, float h) {
  StygianTabBar *bar = (StygianTabBar *)calloc(1, sizeof(StygianTabBar));
  if (!bar)
    return NULL;

  bar->x = x;
  bar->y = y;
  bar->w = w;
  bar->h = h;
  bar->dragging_tab = -1;
  bar->hot_tab = -1;
  bar->hot_close_button = -1;
  bar->min_tab_width = 80.0f;
  bar->max_tab_width = 200.0f;
  bar->tab_width = bar->max_tab_width;
  return bar;
}

void stygian_tab_bar_destroy(StygianTabBar *bar) {
  if (bar)
    free(bar);
}

void stygian_tab_bar_set_layout(StygianTabBar *bar, float x, float y, float w,
                                float h) {
  bar->x = x;
  bar->y = y;
  bar->w = w;
  bar->h = h;
}

// Deprecated init removed
// void stygian_tab_bar_init(...)

int stygian_tab_bar_add(StygianTabBar *bar, const char *title, bool closable) {
  if (bar->tab_count >= STYGIAN_MAX_TABS)
    return -1;

  int index = bar->tab_count;
  StygianTabItem *tab = &bar->tabs[index];

  strncpy(tab->title, title, sizeof(tab->title) - 1);
  tab->title[sizeof(tab->title) - 1] = '\0';
  tab->closable = closable;
  tab->pinned = false;
  tab->user_data = NULL;
  tab->logical_index = index;
  tab->visual_index = index;

  bar->tab_count++;

  // Recalculate tab width
  float available_width = bar->w - 10.0f; // Padding
  bar->tab_width = available_width / bar->tab_count;
  if (bar->tab_width < bar->min_tab_width)
    bar->tab_width = bar->min_tab_width;
  if (bar->tab_width > bar->max_tab_width)
    bar->tab_width = bar->max_tab_width;

  return index;
}

void stygian_tab_bar_remove(StygianTabBar *bar, int index) {
  if (index < 0 || index >= bar->tab_count)
    return;

  // Shift tabs down
  for (int i = index; i < bar->tab_count - 1; i++) {
    bar->tabs[i] = bar->tabs[i + 1];
    bar->tabs[i].logical_index = i;
  }

  bar->tab_count--;

  // Adjust active tab
  if (bar->active_tab >= bar->tab_count) {
    bar->active_tab = bar->tab_count - 1;
  }
  if (bar->active_tab < 0)
    bar->active_tab = 0;

  // Recalculate tab width
  if (bar->tab_count > 0) {
    float available_width = bar->w - 10.0f;
    bar->tab_width = available_width / bar->tab_count;
    if (bar->tab_width < bar->min_tab_width)
      bar->tab_width = bar->min_tab_width;
    if (bar->tab_width > bar->max_tab_width)
      bar->tab_width = bar->max_tab_width;
  }
}

int stygian_tab_bar_update(StygianContext *ctx, StygianFont font,
                           StygianTabBar *bar) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  bool mouse_down = stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
  static bool was_mouse_down = false;

  int result = 0; // 0=none, 1=switched, 2=closed, 3=reordered

  // Background
  stygian_rect(ctx, bar->x, bar->y, bar->w, bar->h, 0.1f, 0.1f, 0.1f, 1.0f);

  // Update hot states
  bar->hot_tab = -1;
  bar->hot_close_button = -1;

  // Calculate tab positions (but don't override dragged tab position)
  float cur_x = bar->x + 5.0f;

  for (int i = 0; i < bar->tab_count; i++) {
    StygianTabItem *tab = &bar->tabs[i];

    // Only update position if not being dragged
    if (bar->dragging_tab != i) {
      tab->x = cur_x;
      tab->y = bar->y + 2.0f;
      tab->w = bar->tab_width - 4.0f;
      tab->h = bar->h - 4.0f;
    }

    cur_x += bar->tab_width;
  }

  // Render tabs (non-dragged first)
  for (int i = 0; i < bar->tab_count; i++) {
    if (bar->dragging_tab == i)
      continue; // Render dragged tab last

    StygianTabItem *tab = &bar->tabs[i];

    bool is_active = (i == bar->active_tab);
    bool is_hovered = point_in_rect(mx, my, tab->x, tab->y, tab->w, tab->h);

    if (is_hovered && bar->dragging_tab < 0)
      bar->hot_tab = i;

    // Tab background
    float r = 0.15f, g = 0.15f, b = 0.15f;
    if (is_active) {
      r = 0.2f;
      g = 0.25f;
      b = 0.35f;
    } else if (is_hovered) {
      r = 0.18f;
      g = 0.18f;
      b = 0.18f;
    }

    stygian_rect_rounded(ctx, tab->x, tab->y, tab->w, tab->h, r, g, b, 1.0f,
                         4.0f);

    // Title
    if (font) {
      float text_x = tab->x + 8.0f;
      float text_y = tab->y + (tab->h - 14.0f) / 2.0f;
      stygian_text(ctx, font, tab->title, text_x, text_y, 14.0f, 0.9f, 0.9f,
                   0.9f, 1.0f);
    }

    // Close button
    if (tab->closable && !tab->pinned) {
      float close_size = 16.0f;
      float close_x = tab->x + tab->w - close_size - 4.0f;
      float close_y = tab->y + (tab->h - close_size) / 2.0f;

      bool close_hovered =
          point_in_rect(mx, my, close_x, close_y, close_size, close_size);
      if (close_hovered && bar->dragging_tab < 0) {
        bar->hot_close_button = i;
      }

      float close_r = close_hovered ? 0.9f : 0.6f;
      float close_g = close_hovered ? 0.3f : 0.3f;
      float close_b = close_hovered ? 0.3f : 0.3f;

      stygian_rect_rounded(ctx, close_x, close_y, close_size, close_size,
                           close_r, close_g, close_b, 1.0f, close_size / 2.0f);

      // X mark
      if (font) {
        stygian_text(ctx, font, "x", close_x + 4, close_y + 1, 12.0f, 1.0f,
                     1.0f, 1.0f, 1.0f);
      }
    }
  }

  // Render dragged tab on top
  if (bar->dragging_tab >= 0 && bar->dragging_tab < bar->tab_count) {
    StygianTabItem *tab = &bar->tabs[bar->dragging_tab];

    // Update dragged tab position to follow mouse
    tab->x = mx - bar->drag_offset_x;
    tab->y = bar->y + 2.0f;

    // Clamp to bar bounds
    if (tab->x < bar->x)
      tab->x = bar->x;
    if (tab->x + tab->w > bar->x + bar->w)
      tab->x = bar->x + bar->w - tab->w;

    // Render with elevation
    stygian_rect_rounded(ctx, tab->x, tab->y - 2, tab->w, tab->h + 2, 0.25f,
                         0.3f, 0.4f, 1.0f, 4.0f);

    if (font) {
      float text_x = tab->x + 8.0f;
      float text_y = tab->y + (tab->h - 14.0f) / 2.0f;
      stygian_text(ctx, font, tab->title, text_x, text_y, 14.0f, 1.0f, 1.0f,
                   1.0f, 1.0f);
    }
  }

  // Interaction logic
  if (mouse_down && !was_mouse_down) {
    // Mouse pressed
    if (bar->hot_close_button >= 0) {
      // Close button clicked
      int close_idx = bar->hot_close_button;
      stygian_tab_bar_remove(bar, close_idx);
      result = 2; // Tab closed
    } else if (bar->hot_tab >= 0) {
      // Tab clicked - switch and potentially start drag
      if (bar->active_tab != bar->hot_tab) {
        bar->active_tab = bar->hot_tab;
        result = 1; // Tab switched
      }
      bar->dragging_tab = bar->hot_tab;
      bar->drag_offset_x = mx - bar->tabs[bar->hot_tab].x;
    }
  } else if (!mouse_down && was_mouse_down) {
    // Mouse released
    if (bar->dragging_tab >= 0) {
      // Calculate drop position - find which gap the tab is closest to
      StygianTabItem *dragged = &bar->tabs[bar->dragging_tab];
      float drag_center_x = dragged->x + dragged->w / 2.0f;

      // Find closest gap between tabs
      int target_index = 0;
      float min_dist = 999999.0f;

      // Check gap before each tab
      for (int i = 0; i <= bar->tab_count; i++) {
        float gap_x;
        if (i == 0) {
          gap_x = bar->x + 5.0f; // Before first tab
        } else if (i == bar->tab_count) {
          gap_x =
              bar->x + 5.0f + bar->tab_width * bar->tab_count; // After last tab
        } else {
          gap_x = bar->x + 5.0f + bar->tab_width * i; // Between tabs
        }

        float dist = drag_center_x - gap_x;
        if (dist < 0)
          dist = -dist; // abs

        if (dist < min_dist) {
          min_dist = dist;
          target_index = i;
        }
      }

      // Clamp and adjust for removal of dragged tab
      if (target_index > bar->tab_count)
        target_index = bar->tab_count;
      if (target_index > bar->dragging_tab)
        target_index--; // Account for dragged tab removal

      // Perform reordering if position changed
      if (target_index != bar->dragging_tab) {

        StygianTabItem temp = bar->tabs[bar->dragging_tab];

        // Shift tabs
        if (target_index < bar->dragging_tab) {
          // Moving left
          for (int i = bar->dragging_tab; i > target_index; i--) {
            bar->tabs[i] = bar->tabs[i - 1];
            bar->tabs[i].logical_index = i;
          }
        } else {
          // Moving right
          for (int i = bar->dragging_tab; i < target_index; i++) {
            bar->tabs[i] = bar->tabs[i + 1];
            bar->tabs[i].logical_index = i;
          }
        }

        bar->tabs[target_index] = temp;
        bar->tabs[target_index].logical_index = target_index;

        // Update active tab index if it moved
        if (bar->active_tab == bar->dragging_tab) {
          bar->active_tab = target_index;
        } else if (bar->active_tab >= target_index &&
                   bar->active_tab < bar->dragging_tab) {
          bar->active_tab++;
        } else if (bar->active_tab <= target_index &&
                   bar->active_tab > bar->dragging_tab) {
          bar->active_tab--;
        }

        result = 3; // Tab reordered
      }

      // End drag
      bar->dragging_tab = -1;
    }
  }

  was_mouse_down = mouse_down;

  return result;
}

void *stygian_tab_bar_get_active_data(StygianTabBar *bar) {
  if (bar->active_tab < 0 || bar->active_tab >= bar->tab_count)
    return NULL;
  return bar->tabs[bar->active_tab].user_data;
}

int stygian_tab_bar_get_active_index(StygianTabBar *bar) {
  return bar->active_tab;
}

int stygian_tab_bar_get_count(StygianTabBar *bar) { return bar->tab_count; }

const char *stygian_tab_bar_get_title(StygianTabBar *bar, int index) {
  if (index < 0 || index >= bar->tab_count)
    return NULL;
  return bar->tabs[index].title;
}

// ============================================================================
// Multiviewport System Implementation
// ============================================================================

StygianMultiViewport *stygian_multiviewport_create(void) {
  StygianMultiViewport *mv =
      (StygianMultiViewport *)calloc(1, sizeof(StygianMultiViewport));
  if (!mv)
    return NULL;

  mv->layout_mode = STYGIAN_VIEWPORT_LAYOUT_SINGLE;
  mv->split_ratio_h = 0.5f;
  mv->split_ratio_v = 0.5f;
  return mv;
}

void stygian_multiviewport_destroy(StygianMultiViewport *mv) {
  if (mv)
    free(mv);
}

int stygian_multiviewport_add(StygianMultiViewport *mv, const char *name,
                              StygianViewportType type) {
  if (mv->viewport_count >= STYGIAN_MAX_VIEWPORTS)
    return -1;

  int index = mv->viewport_count;
  StygianViewport *vp = &mv->viewports[index];

  strncpy(vp->name, name, sizeof(vp->name) - 1);
  vp->name[sizeof(vp->name) - 1] = '\0';
  vp->type = type;
  vp->active = (index == 0);
  vp->show_grid = true;
  vp->show_gizmo = true;
  vp->framebuffer_texture = 0;
  vp->user_data = NULL;

  mv->viewport_count++;
  return index;
}

void stygian_multiviewport_set_layout(StygianMultiViewport *mv,
                                      int layout_mode) {
  mv->layout_mode = layout_mode;
}

void stygian_multiviewport_render(StygianContext *ctx, StygianFont font,
                                  StygianMultiViewport *mv) {
  // TODO: Implement layout-specific rendering
  // For now, just render active viewport
  if (mv->active_viewport < 0 || mv->active_viewport >= mv->viewport_count)
    return;

  StygianViewport *vp = &mv->viewports[mv->active_viewport];

  // Render viewport frame
  stygian_rect(ctx, vp->x, vp->y, vp->w, vp->h, 0.05f, 0.05f, 0.05f, 1.0f);

  // Render framebuffer texture if available
  if (vp->framebuffer_texture != 0) {
    stygian_image(ctx, vp->framebuffer_texture, vp->x + 2, vp->y + 2, vp->w - 4,
                  vp->h - 4);
  }

  // Render viewport name
  if (font) {
    stygian_text(ctx, font, vp->name, vp->x + 10, vp->y + 10, 14.0f, 0.7f, 0.7f,
                 0.7f, 1.0f);
  }
}

int stygian_multiviewport_hit_test(StygianMultiViewport *mv, int mouse_x,
                                   int mouse_y) {
  for (int i = 0; i < mv->viewport_count; i++) {
    StygianViewport *vp = &mv->viewports[i];
    if (point_in_rect(mouse_x, mouse_y, vp->x, vp->y, vp->w, vp->h)) {
      return i;
    }
  }
  return -1;
}
