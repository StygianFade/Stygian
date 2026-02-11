// stygian_widgets.c - Widget Implementation for Stygian UI Library
// Immediate-mode widgets built on Stygian rendering primitives

#include "stygian_widgets.h"
#include "../include/stygian_clipboard.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// Internal State (Immediate-Mode)
// ============================================================================

// Queue for char events
#define MAX_CHAR_EVENTS 32
#define MAX_KEY_EVENTS 32
#define MAX_WIDGET_REGIONS 4096

typedef struct WidgetRegion {
  float x;
  float y;
  float w;
  float h;
  uint32_t flags;
} WidgetRegion;

typedef struct WidgetState {
  StygianContext *ctx;
  uint32_t hot_id;    // Widget under mouse
  uint32_t active_id; // Widget being interacted with
  uint32_t focus_id;  // Widget with keyboard focus
  int mouse_x, mouse_y;
  bool mouse_down;
  bool mouse_was_down;
  bool right_down;
  bool right_was_down;
  bool mouse_pressed;
  bool mouse_released;
  bool right_pressed;
  bool right_released;
  bool mouse_pressed_mutating;
  bool right_pressed_mutating;

  // Buffered Inputs (cleared each frame)
  uint32_t char_events[MAX_CHAR_EVENTS];
  int char_count;

  struct {
    StygianKey key;
    bool down;
    uint32_t mods;
  } key_events[MAX_KEY_EVENTS];
  int key_count;

  // Scroll
  float scroll_dx, scroll_dy;
  float mouse_dx, mouse_dy; // Calculated internally
  uint32_t repaint_hz_request;

  // Focus navigation (tab cycle)
  uint32_t focus_order_prev[1024];
  uint32_t focus_order_curr[1024];
  uint16_t focus_count_prev;
  uint16_t focus_count_curr;
  bool nav_prepared;
  bool nav_tab_pressed;
  bool nav_shift_pressed;
  bool nav_enter_pressed;
  bool nav_space_pressed;
  bool nav_left_pressed;
  bool nav_right_pressed;
  bool nav_up_pressed;
  bool nav_down_pressed;

  // Clipboard (simulated or real)
  char *clipboard_text;

  // Previous-frame interactive regions for strict input routing.
  WidgetRegion regions_prev[MAX_WIDGET_REGIONS];
  WidgetRegion regions_curr[MAX_WIDGET_REGIONS];
  uint16_t region_count_prev;
  uint16_t region_count_curr;
  bool has_region_snapshot;
  uint64_t impact_pointer_only_events;
  uint64_t impact_mutated_events;
  uint64_t impact_request_events;
} WidgetState;

static WidgetState g_widget_state = {0};

static double perf_now_seconds(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

// Widget ID hash: uses x,y coordinates (floats) and label string for stable IDs
// CRITICAL: DO NOT hash &x (stack address) - that causes ghost click
// collisions!
static uint32_t widget_id(float x, float y, const char *str) {
  uint32_t hash = 2166136261u;
  // Hash x coordinate as bytes
  const unsigned char *px = (const unsigned char *)&x;
  for (size_t i = 0; i < sizeof(float); i++) {
    hash ^= px[i];
    hash *= 16777619;
  }
  // Hash y coordinate as bytes
  const unsigned char *py = (const unsigned char *)&y;
  for (size_t i = 0; i < sizeof(float); i++) {
    hash ^= py[i];
    hash *= 16777619;
  }
  // Hash string if provided
  if (str) {
    while (*str) {
      hash ^= (unsigned char)*str++;
      hash *= 16777619;
    }
  }
  return hash;
}

static bool point_in_rect(float px, float py, float x, float y, float w,
                          float h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

static void widget_register_region_internal(float x, float y, float w, float h,
                                            uint32_t flags) {
  WidgetRegion *r;
  if (w <= 0.0f || h <= 0.0f || flags == 0u)
    return;
  if (g_widget_state.region_count_curr >= MAX_WIDGET_REGIONS)
    return;
  r = &g_widget_state.regions_curr[g_widget_state.region_count_curr++];
  r->x = x;
  r->y = y;
  r->w = w;
  r->h = h;
  r->flags = flags;
}

static bool widget_region_hit_prev(float px, float py, uint32_t need_flags) {
  int i;
  if (need_flags == 0u)
    return false;
  if (!g_widget_state.has_region_snapshot)
    return false;
  for (i = (int)g_widget_state.region_count_prev - 1; i >= 0; --i) {
    const WidgetRegion *r = &g_widget_state.regions_prev[i];
    if ((r->flags & need_flags) != 0u &&
        point_in_rect(px, py, r->x, r->y, r->w, r->h)) {
      return true;
    }
  }
  return false;
}

static bool widget_mouse_pressed(void) {
  return g_widget_state.mouse_pressed ||
         (g_widget_state.mouse_down && !g_widget_state.mouse_was_down);
}

static bool widget_mouse_released(void) {
  return g_widget_state.mouse_released ||
         (!g_widget_state.mouse_down && g_widget_state.mouse_was_down);
}

static bool widget_right_pressed(void) {
  return g_widget_state.right_pressed ||
         (g_widget_state.right_down && !g_widget_state.right_was_down);
}

static void widget_nav_prepare(void) {
  int i;
  if (g_widget_state.nav_prepared)
    return;

  g_widget_state.nav_prepared = true;
  g_widget_state.nav_tab_pressed = false;
  g_widget_state.nav_shift_pressed = false;
  g_widget_state.nav_enter_pressed = false;
  g_widget_state.nav_space_pressed = false;
  g_widget_state.nav_left_pressed = false;
  g_widget_state.nav_right_pressed = false;
  g_widget_state.nav_up_pressed = false;
  g_widget_state.nav_down_pressed = false;

  for (i = 0; i < g_widget_state.key_count; i++) {
    if (!g_widget_state.key_events[i].down)
      continue;
    switch (g_widget_state.key_events[i].key) {
    case STYGIAN_KEY_TAB:
      g_widget_state.nav_tab_pressed = true;
      if (g_widget_state.key_events[i].mods & STYGIAN_MOD_SHIFT)
        g_widget_state.nav_shift_pressed = true;
      break;
    case STYGIAN_KEY_ENTER:
      g_widget_state.nav_enter_pressed = true;
      break;
    case STYGIAN_KEY_SPACE:
      g_widget_state.nav_space_pressed = true;
      break;
    case STYGIAN_KEY_LEFT:
      g_widget_state.nav_left_pressed = true;
      break;
    case STYGIAN_KEY_RIGHT:
      g_widget_state.nav_right_pressed = true;
      break;
    case STYGIAN_KEY_UP:
      g_widget_state.nav_up_pressed = true;
      break;
    case STYGIAN_KEY_DOWN:
      g_widget_state.nav_down_pressed = true;
      break;
    default:
      break;
    }
  }

  if (g_widget_state.nav_tab_pressed && g_widget_state.focus_count_prev > 0u) {
    int i;
    int focused_index = -1;
    for (i = 0; i < (int)g_widget_state.focus_count_prev; i++) {
      if (g_widget_state.focus_order_prev[i] == g_widget_state.focus_id) {
        focused_index = i;
        break;
      }
    }
    if (focused_index < 0) {
      g_widget_state.focus_id = g_widget_state.focus_order_prev[0];
    } else {
      int next =
          g_widget_state.nav_shift_pressed
              ? (focused_index - 1 + (int)g_widget_state.focus_count_prev) %
                    (int)g_widget_state.focus_count_prev
              : (focused_index + 1) % (int)g_widget_state.focus_count_prev;
      g_widget_state.focus_id = g_widget_state.focus_order_prev[next];
    }
  }
}

static void widget_register_focusable(uint32_t id) {
  uint16_t i;
  if (id == 0)
    return;
  for (i = 0; i < g_widget_state.focus_count_curr; i++) {
    if (g_widget_state.focus_order_curr[i] == id)
      return;
  }
  if (g_widget_state.focus_count_curr <
      (uint16_t)(sizeof(g_widget_state.focus_order_curr) /
                 sizeof(g_widget_state.focus_order_curr[0]))) {
    g_widget_state.focus_order_curr[g_widget_state.focus_count_curr++] = id;
  }
}

void stygian_widgets_begin_frame(StygianContext *ctx) {
  StygianWindow *win = stygian_get_window(ctx);
  uint16_t i;

  g_widget_state.ctx = ctx;

  // Clear event buffers
  g_widget_state.char_count = 0;
  g_widget_state.key_count = 0;
  g_widget_state.scroll_dx = 0;
  g_widget_state.scroll_dy = 0;
  g_widget_state.repaint_hz_request = 0;
  g_widget_state.region_count_curr = 0;

  // Reset movement delta; only real mouse-move events set dx/dy.
  int nx, ny;
  g_widget_state.mouse_dx = 0.0f;
  g_widget_state.mouse_dy = 0.0f;
  stygian_mouse_pos(win, &nx, &ny);
  g_widget_state.mouse_x = nx;
  g_widget_state.mouse_y = ny;

  g_widget_state.mouse_was_down = g_widget_state.mouse_down;
  g_widget_state.mouse_down = stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
  g_widget_state.right_was_down = g_widget_state.right_down;
  g_widget_state.right_down = stygian_mouse_down(win, STYGIAN_MOUSE_RIGHT);
  g_widget_state.mouse_pressed = false;
  g_widget_state.mouse_released = false;
  g_widget_state.right_pressed = false;
  g_widget_state.right_released = false;
  g_widget_state.mouse_pressed_mutating = false;
  g_widget_state.right_pressed_mutating = false;

  // Carry focus traversal order from previous frame.
  g_widget_state.focus_count_prev = g_widget_state.focus_count_curr;
  for (i = 0; i < g_widget_state.focus_count_prev; i++) {
    g_widget_state.focus_order_prev[i] = g_widget_state.focus_order_curr[i];
  }
  g_widget_state.focus_count_curr = 0;
  g_widget_state.nav_prepared = false;

  // Reset hot widget each frame
  g_widget_state.hot_id = 0;

  // Keep drag/active interactions smooth without tying redraw to mouse-move
  // events.
  if (g_widget_state.ctx && g_widget_state.mouse_down &&
      g_widget_state.active_id != 0u) {
    stygian_set_repaint_source(g_widget_state.ctx, "drag");
    stygian_request_repaint_hz(g_widget_state.ctx, 60u);
  }
}

StygianWidgetEventImpact
stygian_widgets_process_event_ex(StygianContext *ctx, const StygianEvent *e) {
  StygianWidgetEventImpact impact = STYGIAN_IMPACT_NONE;
  bool should_repaint = false;
  bool hit_region = false;
  bool mutating_region = false;
  if (ctx)
    g_widget_state.ctx = ctx;
  if (!e)
    return impact;
  if (e->type == STYGIAN_EVENT_MOUSE_MOVE) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.mouse_dx = (float)e->mouse_move.dx;
    g_widget_state.mouse_dy = (float)e->mouse_move.dy;
    g_widget_state.mouse_x = e->mouse_move.x;
    g_widget_state.mouse_y = e->mouse_move.y;
    if (g_widget_state.ctx && g_widget_state.active_id != 0u) {
      stygian_set_repaint_source(g_widget_state.ctx, "drag");
      stygian_request_repaint_hz(g_widget_state.ctx, 60u);
      stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    }
  } else if (e->type == STYGIAN_EVENT_MOUSE_DOWN) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.mouse_x = e->mouse_button.x;
    g_widget_state.mouse_y = e->mouse_button.y;
    if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      hit_region = widget_region_hit_prev((float)e->mouse_button.x,
                                          (float)e->mouse_button.y,
                                          STYGIAN_WIDGET_REGION_POINTER_RIGHT);
      mutating_region = widget_region_hit_prev(
          (float)e->mouse_button.x, (float)e->mouse_button.y,
          STYGIAN_WIDGET_REGION_POINTER_RIGHT_MUTATES);
    } else {
      hit_region = widget_region_hit_prev((float)e->mouse_button.x,
                                          (float)e->mouse_button.y,
                                          STYGIAN_WIDGET_REGION_POINTER_LEFT);
      mutating_region = widget_region_hit_prev(
          (float)e->mouse_button.x, (float)e->mouse_button.y,
          STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
    }
    if (e->mouse_button.button == STYGIAN_MOUSE_LEFT) {
      g_widget_state.mouse_down = true;
      if (hit_region)
        g_widget_state.mouse_pressed = true;
      if (mutating_region)
        g_widget_state.mouse_pressed_mutating = true;
    } else if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      g_widget_state.right_down = true;
      if (hit_region)
        g_widget_state.right_pressed = true;
      if (mutating_region)
        g_widget_state.right_pressed_mutating = true;
    } else {
      // Middle/other buttons do not own widget state, but can still wake a
      // frame for region-bound behaviors (e.g. graph pan).
    }
    // Pointer down requests an evaluation frame only for known interactive
    // regions. Mutation remains separately tracked.
    should_repaint = hit_region || mutating_region;
    if (should_repaint) {
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    }
    if (g_widget_state.ctx && should_repaint) {
      stygian_set_repaint_source(g_widget_state.ctx, "event-pointer");
      stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
    }
  } else if (e->type == STYGIAN_EVENT_MOUSE_UP) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.mouse_x = e->mouse_button.x;
    g_widget_state.mouse_y = e->mouse_button.y;
    if (e->mouse_button.button == STYGIAN_MOUSE_LEFT) {
      g_widget_state.mouse_down = false;
    } else if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      g_widget_state.right_down = false;
    }
    // Release repaint is valid only when we had an in-flight interaction.
    if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      should_repaint = (g_widget_state.active_id != 0u ||
                        g_widget_state.right_pressed_mutating);
    } else {
      should_repaint = (g_widget_state.active_id != 0u ||
                        g_widget_state.mouse_pressed_mutating);
    }
    if (should_repaint) {
      if (e->mouse_button.button == STYGIAN_MOUSE_LEFT) {
        g_widget_state.mouse_released = true;
      } else if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
        g_widget_state.right_released = true;
      }
    }
    if (should_repaint) {
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    }
    if (g_widget_state.ctx && should_repaint) {
      stygian_set_repaint_source(g_widget_state.ctx, "event-pointer");
      stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
    }
  } else if (e->type == STYGIAN_EVENT_CHAR) {
    // Character input is meaningful only when a widget currently owns focus.
    if (g_widget_state.focus_id != 0u) {
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
      impact |= STYGIAN_IMPACT_MUTATED_STATE;
      if (g_widget_state.char_count < MAX_CHAR_EVENTS) {
        g_widget_state.char_events[g_widget_state.char_count++] =
            e->chr.codepoint;
      }
      if (g_widget_state.ctx) {
        stygian_set_repaint_source(g_widget_state.ctx, "event-char");
        stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
      }
    }
  } else if (e->type == STYGIAN_EVENT_KEY_DOWN ||
             e->type == STYGIAN_EVENT_KEY_UP) {
    bool key_affects_ui =
        (g_widget_state.focus_id != 0u || e->key.key == STYGIAN_KEY_TAB ||
         e->key.key == STYGIAN_KEY_ENTER || e->key.key == STYGIAN_KEY_SPACE ||
         e->key.key == STYGIAN_KEY_ESCAPE);
    if (key_affects_ui) {
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
      if (e->type == STYGIAN_EVENT_KEY_DOWN) {
        impact |= STYGIAN_IMPACT_MUTATED_STATE;
      }
      if (g_widget_state.key_count < MAX_KEY_EVENTS) {
        g_widget_state.key_events[g_widget_state.key_count].key = e->key.key;
        g_widget_state.key_events[g_widget_state.key_count].down =
            (e->type == STYGIAN_EVENT_KEY_DOWN);
        g_widget_state.key_events[g_widget_state.key_count].mods = e->key.mods;
        g_widget_state.key_count++;
      }
      if (g_widget_state.ctx) {
        stygian_set_repaint_source(g_widget_state.ctx, "event-key");
        stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
      }
    }
  } else if (e->type == STYGIAN_EVENT_SCROLL) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.scroll_dx += e->scroll.dx;
    g_widget_state.scroll_dy += e->scroll.dy;
    // Scroll drives redraw only when the pointer is over a registered
    // scroll-capable region from the previous frame.
    should_repaint = (e->scroll.dx != 0.0f || e->scroll.dy != 0.0f) &&
                     widget_region_hit_prev((float)g_widget_state.mouse_x,
                                            (float)g_widget_state.mouse_y,
                                            STYGIAN_WIDGET_REGION_SCROLL);
    if (should_repaint)
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    if (g_widget_state.ctx && should_repaint) {
      stygian_set_repaint_source(g_widget_state.ctx, "event-scroll");
      stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
    }
  } else if (e->type == STYGIAN_EVENT_RESIZE) {
    impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    impact |= STYGIAN_IMPACT_LAYOUT_CHANGED;
    if (g_widget_state.ctx) {
      stygian_set_repaint_source(g_widget_state.ctx, "event-resize");
      stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
    }
  } else if (e->type == STYGIAN_EVENT_TICK) {
    impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    if (g_widget_state.ctx) {
      stygian_set_repaint_source(g_widget_state.ctx, "event-tick");
      stygian_request_repaint_after_ms(g_widget_state.ctx, 1u);
    }
  }
  if (impact & STYGIAN_IMPACT_POINTER_ONLY)
    g_widget_state.impact_pointer_only_events++;
  if (impact & STYGIAN_IMPACT_MUTATED_STATE)
    g_widget_state.impact_mutated_events++;
  if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
    g_widget_state.impact_request_events++;
  return impact;
}

// User must call this for every event in their loop
void stygian_widgets_process_event(StygianContext *ctx, StygianEvent *e) {
  (void)stygian_widgets_process_event_ex(ctx, e);
}

void stygian_widgets_register_region(float x, float y, float w, float h,
                                     StygianWidgetRegionFlags flags) {
  widget_register_region_internal(x, y, w, h, flags);
}

void stygian_widgets_commit_regions(void) {
  uint16_t i;
  g_widget_state.region_count_prev = g_widget_state.region_count_curr;
  if (g_widget_state.region_count_prev > MAX_WIDGET_REGIONS)
    g_widget_state.region_count_prev = MAX_WIDGET_REGIONS;
  for (i = 0; i < g_widget_state.region_count_prev; ++i) {
    g_widget_state.regions_prev[i] = g_widget_state.regions_curr[i];
  }
  g_widget_state.has_region_snapshot = true;
}

float stygian_widgets_scroll_dx(void) { return g_widget_state.scroll_dx; }

float stygian_widgets_scroll_dy(void) { return g_widget_state.scroll_dy; }

void stygian_widgets_request_repaint_hz(uint32_t hz) {
  if (hz == 0u)
    return;
  if (g_widget_state.ctx) {
    stygian_request_repaint_hz(g_widget_state.ctx, hz);
  }
  if (g_widget_state.repaint_hz_request < hz)
    g_widget_state.repaint_hz_request = hz;
}

uint32_t stygian_widgets_repaint_wait_ms(uint32_t idle_wait_ms) {
  if (g_widget_state.ctx) {
    return stygian_next_repaint_wait_ms(g_widget_state.ctx, idle_wait_ms);
  }
  uint32_t hz;
  uint32_t ms;
  if (idle_wait_ms == 0u)
    idle_wait_ms = 1u;
  hz = g_widget_state.repaint_hz_request;
  if (hz == 0u)
    return idle_wait_ms;
  ms = 1000u / hz;
  if (ms < 1u)
    ms = 1u;
  if (ms > idle_wait_ms)
    ms = idle_wait_ms;
  return ms;
}

bool stygian_widgets_wants_repaint(void) {
  if (g_widget_state.ctx) {
    return stygian_has_pending_repaint(g_widget_state.ctx);
  }
  return g_widget_state.repaint_hz_request > 0u;
}

static void perf_history_push(StygianPerfWidget *state, float frame_ms) {
  uint32_t idx;
  if (!state)
    return;
  idx = state->history_head % STYGIAN_PERF_HISTORY_MAX;
  state->history_ms[idx] = frame_ms;
  state->history_head = (state->history_head + 1u) % STYGIAN_PERF_HISTORY_MAX;
  if (state->history_count < STYGIAN_PERF_HISTORY_MAX)
    state->history_count++;
}

static struct {
  StygianPerfWidget *active;
  float drag_off_x;
  float drag_off_y;
} g_perf_drag = {0};

void stygian_perf_widget(StygianContext *ctx, StygianFont font,
                         StygianPerfWidget *state) {
  char line[256];
  const char *renderer = NULL;
  float x, y, w, h;
  float line_y;
  float line_h;
  float line_size;
  float header_h;
  float latest_ms = 0.0f;
  float frame_target_ms = 16.7f;
  uint32_t draw_calls;
  uint32_t elem_count;
  uint32_t dirty_count;
  uint32_t scope_replay_hits;
  uint32_t scope_replay_misses;
  uint32_t scope_replay_forced;
  uint32_t clip_count;
  uint32_t active_elem_count;
  uint32_t element_capacity;
  uint32_t free_elem_count;
  uint32_t font_count;
  uint32_t inline_emoji_count;
  uint16_t clip_capacity;
  uint32_t hot, active, focus;
  uint64_t pointer_only_events;
  uint64_t mutated_events;
  uint64_t request_events;
  uint32_t window_samples;
  bool repaint_pending;
  uint32_t repaint_wait_ms;
  uint32_t repaint_flags;
  const char *repaint_source;
  uint32_t upload_bytes;
  uint32_t upload_ranges;
  float build_ms;
  float submit_ms;
  float present_ms;
  bool triad_mounted;
  bool triad_info_ok;
  StygianTriadPackInfo triad_info;
  double now_s;
  int vp_w = 0;
  int vp_h = 0;
  StygianWindow *win = NULL;
  bool over_header;
  bool dragging;
  bool interacting;
  uint32_t idle_hz;
  uint32_t active_hz;
  uint32_t stress_hz;
  uint32_t target_idle_hz;
  uint32_t sample_hz;
  uint32_t frame_budget_hz;
  float budget_ms;
  float sample_ms;
  float wall_ms;
  double sample_dt_s;
  double elapsed_s;
  uint32_t sample_steps;
  float wall_fps;
  bool draw_memory;
  bool draw_glyphs;
  bool draw_triad;
  bool draw_input;
  int max_lines;
  int slots_left;

  if (!ctx || !state || !state->enabled)
    return;

  x = state->x;
  y = state->y;
  w = state->w;
  h = state->h;
  header_h = state->compact_mode ? 20.0f : 24.0f;
  widget_register_region_internal(x, y, w, header_h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  line_h = state->compact_mode ? 14.0f : 16.0f;
  line_size = state->compact_mode ? 11.0f : 12.0f;
  renderer = state->renderer_name ? state->renderer_name : "unknown";
  triad_info.version = 0u;
  triad_info.encoding = 0u;
  triad_info.tier = 0u;
  triad_info.entry_count = 0u;
  triad_info.data_offset = 0u;
  win = stygian_get_window(ctx);
  if (win)
    stygian_window_get_size(win, &vp_w, &vp_h);

  over_header = point_in_rect((float)g_widget_state.mouse_x,
                              (float)g_widget_state.mouse_y, x, y, w, header_h);
  if (over_header && widget_mouse_pressed()) {
    g_perf_drag.active = state;
    g_perf_drag.drag_off_x = (float)g_widget_state.mouse_x - x;
    g_perf_drag.drag_off_y = (float)g_widget_state.mouse_y - y;
  }
  if (g_perf_drag.active == state) {
    if (g_widget_state.mouse_down) {
      state->x = (float)g_widget_state.mouse_x - g_perf_drag.drag_off_x;
      state->y = (float)g_widget_state.mouse_y - g_perf_drag.drag_off_y;
      if (vp_w > 0 && vp_h > 0) {
        if (state->x < 0.0f)
          state->x = 0.0f;
        if (state->y < 0.0f)
          state->y = 0.0f;
        if (state->x + state->w > (float)vp_w)
          state->x = (float)vp_w - state->w;
        if (state->y + state->h > (float)vp_h)
          state->y = (float)vp_h - state->h;
      }
      x = state->x;
      y = state->y;
    } else {
      g_perf_drag.active = NULL;
    }
  }

  dragging = (g_perf_drag.active == state) && g_widget_state.mouse_down;
  // Keep high-rate only for active drag/manipulation, not passive clicks.
  interacting = dragging;
  idle_hz =
      state->idle_hz > 0u ? state->idle_hz : (state->show_graph ? 30u : 10u);
  active_hz = state->active_hz > 0u ? state->active_hz : 60u;
  stress_hz = state->max_stress_hz;
  target_idle_hz = idle_hz;
  if (state->stress_mode && stress_hz > 0u) {
    if (stress_hz < idle_hz)
      stress_hz = idle_hz;
    target_idle_hz = stress_hz;
  }
  if (state->history_count > 0u) {
    uint32_t last_idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - 1u) %
                        STYGIAN_PERF_HISTORY_MAX;
    latest_ms = state->history_ms[last_idx];
  }
  frame_budget_hz = target_idle_hz > 0u ? target_idle_hz : 1u;
  budget_ms = 1000.0f / (float)frame_budget_hz;
  if (latest_ms > budget_ms * 1.35f) {
    if (state->budget_miss_count < 100000u)
      state->budget_miss_count++;
  } else if (state->budget_miss_count > 0u) {
    state->budget_miss_count--;
  }
  if (target_idle_hz > idle_hz && state->budget_miss_count > 10u) {
    target_idle_hz = target_idle_hz / 2u;
    if (target_idle_hz < idle_hz)
      target_idle_hz = idle_hz;
  }
  if (ctx) {
    stygian_set_repaint_source(ctx, interacting ? "diag-active" : "diag-idle");
  }
  stygian_widgets_request_repaint_hz(interacting ? active_hz : target_idle_hz);

  draw_calls = stygian_get_last_frame_draw_calls(ctx);
  elem_count = stygian_get_last_frame_element_count(ctx);
  dirty_count = 0u;
  scope_replay_hits = stygian_get_last_frame_scope_replay_hits(ctx);
  scope_replay_misses = stygian_get_last_frame_scope_replay_misses(ctx);
  scope_replay_forced = stygian_get_last_frame_scope_forced_rebuilds(ctx);
  clip_count = stygian_get_last_frame_clip_count(ctx);
  active_elem_count = stygian_get_active_element_count(ctx);
  element_capacity = stygian_get_element_capacity(ctx);
  free_elem_count = stygian_get_free_element_count(ctx);
  font_count = stygian_get_font_count(ctx);
  inline_emoji_count = stygian_get_inline_emoji_cache_count(ctx);
  clip_capacity = stygian_get_clip_capacity(ctx);
  triad_mounted = stygian_triad_is_mounted(ctx);
  triad_info_ok =
      triad_mounted && stygian_triad_get_pack_info(ctx, &triad_info);
  repaint_pending = stygian_has_pending_repaint(ctx);
  repaint_wait_ms = stygian_next_repaint_wait_ms(ctx, 250u);
  repaint_flags = stygian_get_repaint_reason_flags(ctx);
  repaint_source = stygian_get_repaint_source(ctx);
  upload_bytes = stygian_get_last_frame_upload_bytes(ctx);
  upload_ranges = stygian_get_last_frame_upload_ranges(ctx);
  dirty_count = upload_ranges;
  build_ms = stygian_get_last_frame_build_ms(ctx);
  submit_ms = stygian_get_last_frame_submit_ms(ctx);
  present_ms = stygian_get_last_frame_present_ms(ctx);

  if (state->history_count > 0u) {
    uint32_t last_idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - 1u) %
                        STYGIAN_PERF_HISTORY_MAX;
    latest_ms = state->history_ms[last_idx];
  }

  sample_hz = interacting ? active_hz : target_idle_hz;
  if (sample_hz == 0u)
    sample_hz = 30u;
  sample_dt_s = 1.0 / (double)sample_hz;
  sample_ms = build_ms + submit_ms + present_ms;
  if (sample_ms <= 0.0f || sample_ms >= 1000.0f) {
    sample_ms = latest_ms > 0.0f ? latest_ms : 16.7f;
  }
  now_s = perf_now_seconds();
  wall_ms = 0.0f;
  wall_fps = 0.0f;
  if (state->last_render_seconds > 0.0) {
    wall_ms = (float)((now_s - state->last_render_seconds) * 1000.0);
    if (wall_ms > 0.0f && wall_ms < 1000.0f) {
      wall_fps = 1000.0f / wall_ms;
      if (state->fps_wall_smoothed <= 0.0f) {
        state->fps_wall_smoothed = wall_fps;
      } else {
        state->fps_wall_smoothed +=
            (wall_fps - state->fps_wall_smoothed) * 0.1f;
      }
    }
  }
  state->last_render_seconds = now_s;
  if (state->last_sample_seconds <= 0.0) {
    state->last_sample_seconds = now_s;
    perf_history_push(state, sample_ms);
    state->fps_smoothed = 1000.0f / sample_ms;
    if (state->fps_wall_smoothed <= 0.0f)
      state->fps_wall_smoothed = state->fps_smoothed;
  } else {
    elapsed_s = now_s - state->last_sample_seconds;
    sample_steps = 0u;
    while (elapsed_s >= sample_dt_s && sample_steps < 8u) {
      float fps = 1000.0f / sample_ms;
      perf_history_push(state, sample_ms);
      if (state->fps_smoothed <= 0.0f) {
        state->fps_smoothed = fps;
      } else {
        state->fps_smoothed += (fps - state->fps_smoothed) * 0.1f;
      }
      state->last_sample_seconds += sample_dt_s;
      elapsed_s -= sample_dt_s;
      sample_steps++;
    }
    if (sample_steps == 8u) {
      state->last_sample_seconds = now_s;
    }
  }

  if (state->history_count > 0u) {
    uint32_t last_idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - 1u) %
                        STYGIAN_PERF_HISTORY_MAX;
    latest_ms = state->history_ms[last_idx];
  }

  window_samples = state->history_window;
  if (window_samples == 0u)
    window_samples = 120u;
  if (window_samples > STYGIAN_PERF_HISTORY_MAX)
    window_samples = STYGIAN_PERF_HISTORY_MAX;
  if (window_samples < 30u)
    window_samples = 30u;
  hot = g_widget_state.hot_id;
  active = g_widget_state.active_id;
  focus = g_widget_state.focus_id;
  pointer_only_events = g_widget_state.impact_pointer_only_events;
  mutated_events = g_widget_state.impact_mutated_events;
  request_events = g_widget_state.impact_request_events;

  // Ensure graph visibility in small widgets by dropping optional lines first.
  draw_memory = state->show_memory;
  draw_glyphs = state->show_glyphs;
  draw_triad = state->show_triad;
  draw_input = state->show_input;
  if (state->show_graph) {
    max_lines = (int)((h - 94.0f) / line_h); // reserve graph area
    if (max_lines < 6)
      max_lines = 6;
    slots_left = max_lines - 6; // after required lines
    if (slots_left <= 0) {
      draw_memory = false;
      draw_glyphs = false;
      draw_triad = false;
      draw_input = false;
    } else {
      if (draw_memory) {
        slots_left--;
      }
      if (draw_glyphs) {
        if (slots_left > 0) {
          slots_left--;
        } else {
          draw_glyphs = false;
        }
      }
      if (draw_triad) {
        if (slots_left > 0) {
          slots_left--;
        } else {
          draw_triad = false;
        }
      }
      if (draw_input) {
        if (slots_left > 0) {
          slots_left--;
        } else {
          draw_input = false;
        }
      }
    }
  }

  stygian_rect_rounded(ctx, x, y, w, h, 0.08f, 0.09f, 0.11f, 0.94f, 6.0f);
  stygian_rect_rounded(ctx, x, y, w, header_h, 0.13f, 0.15f, 0.19f, 0.96f,
                       6.0f);

  snprintf(line, sizeof(line), "Stygian Diagnostics (%s)", renderer);
  stygian_text(ctx, font, line, x + 8.0f, y + 4.0f,
               state->compact_mode ? 12.0f : 13.0f, 0.92f, 0.94f, 0.98f, 1.0f);

  line_y = y + header_h + 6.0f;
  snprintf(line, sizeof(line),
           "Frame: %.2f ms | CPU FPS: %.1f | Wall FPS: %.1f", latest_ms,
           state->fps_smoothed, state->fps_wall_smoothed);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.85f, 0.9f, 0.95f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line),
           "Draw calls: %u | Elements: %u | Dirty ranges: %u",
           draw_calls, elem_count, dirty_count);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line), "Scope replay h/m/f: %u/%u/%u",
           scope_replay_hits, scope_replay_misses, scope_replay_forced);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line),
           "Repaint: %s flags=0x%X pending=%u next_wait=%ums",
           repaint_source ? repaint_source : "none", repaint_flags,
           repaint_pending ? 1u : 0u, repaint_wait_ms);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line), "CPU ms: build=%.2f submit=%.2f present=%.2f",
           build_ms, submit_ms, present_ms);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line), "Upload: %u bytes in %u range(s)", upload_bytes,
           upload_ranges);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line), "Clip regions: %u / %u", clip_count,
           (uint32_t)clip_capacity);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.82f,
               0.88f, 1.0f);
  line_y += line_h;

  if (draw_memory) {
    snprintf(line, sizeof(line), "Element pool: active=%u free=%u cap=%u",
             active_elem_count, free_elem_count, element_capacity);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.84f,
                 0.90f, 1.0f);
    line_y += line_h;
  }

  if (draw_glyphs) {
    snprintf(line, sizeof(line), "Fonts=%u | Inline emoji cache=%u", font_count,
             inline_emoji_count);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.84f,
                 0.90f, 1.0f);
    line_y += line_h;
  }

  if (draw_triad) {
    if (triad_info_ok) {
      snprintf(line, sizeof(line), "TRIAD mounted: tier=%u entries=%u enc=%u",
               triad_info.tier, triad_info.entry_count, triad_info.encoding);
    } else if (triad_mounted) {
      snprintf(line, sizeof(line), "TRIAD mounted (pack info unavailable)");
    } else {
      snprintf(line, sizeof(line), "TRIAD not mounted");
    }
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.84f,
                 0.90f, 1.0f);
    line_y += line_h;
  }

  if (draw_input) {
    snprintf(
        line, sizeof(line), "Input: mouse(%d,%d) hot=%u active=%u focus=%u",
        g_widget_state.mouse_x, g_widget_state.mouse_y, hot, active, focus);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.76f, 0.8f,
                 0.86f, 1.0f);
    line_y += line_h;
    snprintf(line, sizeof(line), "Event impact ptr/mut/req: %llu/%llu/%llu",
             (unsigned long long)pointer_only_events,
             (unsigned long long)mutated_events,
             (unsigned long long)request_events);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.76f, 0.8f,
                 0.86f, 1.0f);
    line_y += line_h;
  }

  if (state->show_graph && state->history_count > 0u && h > 90.0f) {
    float graph_x = x + 8.0f;
    float graph_w = w - 16.0f;
    float graph_h = h - (line_y - y) - 8.0f;
    float max_ms = state->auto_scale_graph ? 16.7f : 33.3f;
    float prev_px = 0.0f;
    float prev_py = 0.0f;
    bool has_prev = false;
    uint32_t sample_count = state->history_count;
    uint32_t draw_points;
    uint32_t max_segments;
    uint32_t i;
    if (sample_count > window_samples)
      sample_count = window_samples;

    if (graph_h > 8.0f) {
      max_segments = state->graph_max_segments;
      if (max_segments == 0u)
        max_segments = 64u;
      draw_points = sample_count;
      if (draw_points > max_segments + 1u)
        draw_points = max_segments + 1u;

      if (state->auto_scale_graph) {
        for (i = 0; i < sample_count; i++) {
          uint32_t idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX -
                          sample_count + i) %
                         STYGIAN_PERF_HISTORY_MAX;
          if (state->history_ms[idx] > max_ms) {
            max_ms = state->history_ms[idx];
          }
        }
        if (max_ms < 8.0f)
          max_ms = 8.0f;
      }

      stygian_rect(ctx, graph_x, line_y, graph_w, graph_h, 0.05f, 0.06f, 0.08f,
                   0.9f);
      {
        float tt = frame_target_ms / max_ms;
        float ty;
        if (tt < 0.0f)
          tt = 0.0f;
        if (tt > 1.0f)
          tt = 1.0f;
        ty = line_y + graph_h - (tt * graph_h);
        stygian_line(ctx, graph_x, ty, graph_x + graph_w, ty, 1.0f, 0.65f,
                     0.72f, 0.9f, 0.55f);
      }
      for (i = 0; i < draw_points; i++) {
        uint32_t src_i;
        uint32_t idx;
        float ms;
        float t;
        float px;
        float py;
        float stress;
        float r = 0.28f, g = 0.90f, b = 0.52f;
        if (draw_points <= 1u) {
          src_i = sample_count - 1u;
        } else {
          src_i = (uint32_t)(((uint64_t)i * (uint64_t)(sample_count - 1u)) /
                             (uint64_t)(draw_points - 1u));
        }
        idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - sample_count +
               src_i) %
              STYGIAN_PERF_HISTORY_MAX;
        ms = state->history_ms[idx];
        t = ms / max_ms;
        if (t < 0.0f)
          t = 0.0f;
        if (t > 1.0f)
          t = 1.0f;
        if (draw_points <= 1u) {
          px = graph_x;
        } else {
          px = graph_x + (graph_w * ((float)i / (float)(draw_points - 1u)));
        }
        py = line_y + graph_h - (t * graph_h);
        stress = ms / frame_target_ms;
        if (stress > 2.0f) {
          r = 0.97f;
          g = 0.30f;
          b = 0.33f;
        } else if (stress > 1.0f) {
          r = 0.97f;
          g = 0.78f;
          b = 0.23f;
        }
        if (has_prev) {
          stygian_line(ctx, prev_px, prev_py, px, py, 1.5f, r, g, b, 0.95f);
        }
        prev_px = px;
        prev_py = py;
        has_prev = true;
      }
    }
  }
}

void stygian_perf_widget_set_rates(StygianPerfWidget *state, uint32_t graph_hz,
                                   uint32_t text_hz) {
  if (!state)
    return;
  state->idle_hz = graph_hz;
  state->active_hz = graph_hz;
  state->text_hz = text_hz;
}

void stygian_perf_widget_set_enabled(StygianPerfWidget *state, bool enabled) {
  if (!state)
    return;
  state->enabled = enabled;
}

// ============================================================================
// Overlay Widgets
// ============================================================================

typedef struct {
  bool active;
  uint8_t clip_id;
} ModalRuntimeState;

typedef struct {
  bool active;
  StygianContextMenu *menu;
  float x, y, w;
  float item_h;
  float panel_h;
  int item_count;
  int item_cursor;
} ContextMenuRuntimeState;

static ModalRuntimeState g_modal_runtime = {0};
static ContextMenuRuntimeState g_context_menu_runtime = {0};

void stygian_tooltip(StygianContext *ctx, StygianFont font,
                     const StygianTooltip *tooltip) {
  float x;
  float y;
  float w;
  float h = 24.0f;
  float max_w;
  int vp_w = 2000;
  int vp_h = 1200;
  StygianWindow *win;
  if (!ctx || !tooltip || !tooltip->show || !tooltip->text || !tooltip->text[0])
    return;
  win = stygian_get_window(ctx);
  if (win)
    stygian_window_get_size(win, &vp_w, &vp_h);

  max_w = tooltip->max_w > 20.0f ? tooltip->max_w : 320.0f;
  w = stygian_text_width(ctx, font, tooltip->text, 14.0f) + 14.0f;
  if (w > max_w)
    w = max_w;

  x = tooltip->x + 12.0f;
  y = tooltip->y + 16.0f;
  if (x + w > (float)vp_w)
    x = tooltip->x - w - 6.0f;
  if (y + h > (float)vp_h)
    y = tooltip->y - h - 6.0f;
  if (x < 0.0f)
    x = 0.0f;
  if (y < 0.0f)
    y = 0.0f;

  stygian_rect_rounded(ctx, x, y, w, h, 0.08f, 0.09f, 0.12f, 0.96f, 4.0f);
  stygian_text(ctx, font, tooltip->text, x + 7.0f, y + 5.0f, 14.0f, 0.94f,
               0.96f, 1.0f, 1.0f);
}

bool stygian_context_menu_trigger_region(StygianContext *ctx,
                                         StygianContextMenu *state, float x,
                                         float y, float w, float h) {
  bool opened = false;
  (void)ctx;
  if (!state)
    return false;
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_RIGHT_MUTATES);

  if (point_in_rect((float)g_widget_state.mouse_x,
                    (float)g_widget_state.mouse_y, x, y, w, h) &&
      widget_right_pressed()) {
    state->open = true;
    state->x = (float)g_widget_state.mouse_x;
    state->y = (float)g_widget_state.mouse_y;
    opened = true;
  }
  return opened;
}

bool stygian_context_menu_begin(StygianContext *ctx, StygianFont font,
                                StygianContextMenu *state, int item_count) {
  float panel_x;
  float panel_y;
  float panel_w;
  float item_h;
  float panel_h;
  int vp_w = 2000;
  int vp_h = 1200;
  StygianWindow *win;
  if (!ctx || !state || !state->open || item_count <= 0)
    return false;
  win = stygian_get_window(ctx);
  if (win)
    stygian_window_get_size(win, &vp_w, &vp_h);

  panel_w = state->w > 60.0f ? state->w : 180.0f;
  item_h = state->item_h > 18.0f ? state->item_h : 28.0f;
  panel_h = item_h * (float)item_count + 8.0f;
  panel_x = state->x;
  panel_y = state->y;
  if (panel_x + panel_w > (float)vp_w)
    panel_x = (float)vp_w - panel_w;
  if (panel_y + panel_h > (float)vp_h)
    panel_y = (float)vp_h - panel_h;
  if (panel_x < 0.0f)
    panel_x = 0.0f;
  if (panel_y < 0.0f)
    panel_y = 0.0f;

  // While menu is open, allow pointer routing anywhere so outside-click close
  // works without input-driven global repaint leaks in normal mode.
  widget_register_region_internal(0.0f, 0.0f, (float)vp_w, (float)vp_h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  widget_register_region_internal(panel_x, panel_y, panel_w, panel_h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  stygian_rect_rounded(ctx, panel_x, panel_y, panel_w, panel_h, 0.11f, 0.12f,
                       0.14f, 0.97f, 6.0f);
  if (font) {
    stygian_text(ctx, font, "Menu", panel_x + 8.0f, panel_y + 4.0f, 12.0f,
                 0.82f, 0.86f, 0.92f, 1.0f);
  }

  g_context_menu_runtime.active = true;
  g_context_menu_runtime.menu = state;
  g_context_menu_runtime.x = panel_x + 4.0f;
  g_context_menu_runtime.y = panel_y + 4.0f;
  g_context_menu_runtime.w = panel_w - 8.0f;
  g_context_menu_runtime.item_h = item_h;
  g_context_menu_runtime.panel_h = panel_h;
  g_context_menu_runtime.item_count = item_count;
  g_context_menu_runtime.item_cursor = 0;
  return true;
}

bool stygian_context_menu_item(StygianContext *ctx, StygianFont font,
                               StygianContextMenu *state, const char *label,
                               int item_index) {
  float bx;
  float by;
  bool clicked;
  (void)item_index;
  if (!ctx || !state || !label || !g_context_menu_runtime.active ||
      g_context_menu_runtime.menu != state)
    return false;

  bx = g_context_menu_runtime.x;
  by = g_context_menu_runtime.y + (float)g_context_menu_runtime.item_cursor *
                                      g_context_menu_runtime.item_h;
  g_context_menu_runtime.item_cursor++;
  widget_register_region_internal(bx, by, g_context_menu_runtime.w,
                                  g_context_menu_runtime.item_h - 2.0f,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  clicked = stygian_button(ctx, font, label, bx, by, g_context_menu_runtime.w,
                           g_context_menu_runtime.item_h - 2.0f);
  if (clicked)
    state->open = false;
  return clicked;
}

void stygian_context_menu_end(StygianContext *ctx, StygianContextMenu *state) {
  bool inside;
  if (!ctx || !state || !g_context_menu_runtime.active ||
      g_context_menu_runtime.menu != state) {
    g_context_menu_runtime.active = false;
    return;
  }

  inside = point_in_rect(
      (float)g_widget_state.mouse_x, (float)g_widget_state.mouse_y,
      g_context_menu_runtime.x - 4.0f, g_context_menu_runtime.y - 4.0f,
      g_context_menu_runtime.w + 8.0f, g_context_menu_runtime.panel_h);
  if (!inside && widget_mouse_pressed()) {
    state->open = false;
  }

  g_context_menu_runtime.active = false;
}

bool stygian_modal_begin(StygianContext *ctx, StygianFont font,
                         StygianModal *state, float viewport_w,
                         float viewport_h) {
  float mw;
  float mh;
  float mx;
  float my;
  bool pressed;
  bool inside;
  if (!ctx || !state || !state->open)
    return false;

  mw = state->w > 40.0f ? state->w : 420.0f;
  mh = state->h > 40.0f ? state->h : 260.0f;
  mx = (viewport_w - mw) * 0.5f;
  my = (viewport_h - mh) * 0.5f;
  if (state->close_on_backdrop) {
    widget_register_region_internal(0.0f, 0.0f, viewport_w, viewport_h,
                                    STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  } else {
    widget_register_region_internal(mx, my, mw, mh,
                                    STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  }

  stygian_rect(ctx, 0.0f, 0.0f, viewport_w, viewport_h, 0.02f, 0.02f, 0.03f,
               0.70f);
  stygian_rect_rounded(ctx, mx, my, mw, mh, 0.11f, 0.12f, 0.14f, 0.98f, 8.0f);
  stygian_rect_rounded(ctx, mx, my, mw, 28.0f, 0.15f, 0.17f, 0.22f, 1.0f, 8.0f);
  if (font && state->title) {
    stygian_text(ctx, font, state->title, mx + 10.0f, my + 6.0f, 14.0f, 0.95f,
                 0.97f, 1.0f, 1.0f);
  }

  pressed = widget_mouse_pressed();
  inside = point_in_rect((float)g_widget_state.mouse_x,
                         (float)g_widget_state.mouse_y, mx, my, mw, mh);
  if (state->close_on_backdrop && pressed && !inside) {
    state->open = false;
    return false;
  }

  g_modal_runtime.active = true;
  g_modal_runtime.clip_id =
      stygian_clip_push(ctx, mx + 8.0f, my + 32.0f, mw - 16.0f, mh - 40.0f);
  return true;
}

void stygian_modal_end(StygianContext *ctx, StygianModal *state) {
  (void)state;
  if (!ctx || !g_modal_runtime.active)
    return;
  stygian_clip_pop(ctx);
  g_modal_runtime.active = false;
}

// ... (button, etc)

// ... INSIDE stygian_text_area ...
// (Need separate patch for that, this chunk is strictly beginning of file)

// ============================================================================
// Button Widget
// ============================================================================

bool stygian_button(StygianContext *ctx, StygianFont font, const char *label,
                    float x, float y, float w, float h) {
  uint32_t id = widget_id(x, y, label);
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool hovered =
      point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y, x, y, w, h);
  bool focused;
  bool clicked = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  // Update state
  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);

  // Check for click (mouse released on active button) and always clear active
  // state on release to avoid stale captures.
  if (active && widget_mouse_released()) {
    if (hovered) {
      clicked = true;
    }
    g_widget_state.active_id = 0;
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    clicked = true;
  }

  // Render button
  float bg_r = 0.25f, bg_g = 0.25f, bg_b = 0.25f, bg_a = 1.0f;
  if (active) {
    bg_r = 0.15f;
    bg_g = 0.15f;
    bg_b = 0.15f;
  } else if (focused) {
    bg_r = 0.22f;
    bg_g = 0.24f;
    bg_b = 0.30f;
  } else if (hovered) {
    bg_r = 0.35f;
    bg_g = 0.35f;
    bg_b = 0.35f;
  }

  stygian_rect_rounded(ctx, x, y, w, h, bg_r, bg_g, bg_b, bg_a, 4.0f);

  // Render text (centered)
  if (label) {
    float text_w = stygian_text_width(ctx, font, label, 16.0f);
    float text_x = x + (w - text_w) * 0.5f;
    float text_y = y + (h - 16.0f) * 0.5f;
    stygian_text(ctx, font, label, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return clicked;
}

bool stygian_button_ex(StygianContext *ctx, StygianFont font,
                       StygianButton *state, const StygianWidgetStyle *style) {
  uint32_t id = widget_id(state->x, state->y, state->label);
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool focused;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  state->hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                                 state->x, state->y, state->w, state->h);
  state->clicked = false;

  if (state->hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
      state->pressed = true;
    }
  }

  if (widget_mouse_released()) {
    if (state->pressed && state->hovered) {
      state->clicked = true;
    }
    state->pressed = false;
    if (g_widget_state.active_id == id) {
      g_widget_state.active_id = 0;
    }
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    state->clicked = true;
  }

  // Render with custom style
  const float *color = style->bg_color;
  if (state->pressed)
    color = style->active_color;
  else if (state->hovered)
    color = style->hover_color;

  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, color[0],
                       color[1], color[2], color[3], style->border_radius);

  if (state->label) {
    float text_w = stygian_text_width(ctx, font, state->label, 16.0f);
    float text_x = state->x + (state->w - text_w) * 0.5f;
    float text_y = state->y + (state->h - 16.0f) * 0.5f;
    stygian_text(ctx, font, state->label, text_x, text_y, 16.0f,
                 style->text_color[0], style->text_color[1],
                 style->text_color[2], style->text_color[3]);
  }

  return state->clicked;
}

// ============================================================================
// Slider Widget
// ============================================================================

bool stygian_slider(StygianContext *ctx, float x, float y, float w, float h,
                    float *value, float min, float max) {
  uint32_t id = widget_id(x, y, NULL);
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool focused;
  bool hovered =
      point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y, x, y, w, h);
  bool changed = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);

  if (active && widget_mouse_released()) {
    g_widget_state.active_id = 0;
    active = false;
  }

  if (focused &&
      (g_widget_state.nav_left_pressed || g_widget_state.nav_right_pressed ||
       g_widget_state.nav_up_pressed || g_widget_state.nav_down_pressed)) {
    float span = max - min;
    float step = span * 0.01f;
    if (step <= 0.0f)
      step = 0.01f;
    if (g_widget_state.nav_left_pressed || g_widget_state.nav_down_pressed)
      *value -= step;
    if (g_widget_state.nav_right_pressed || g_widget_state.nav_up_pressed)
      *value += step;
    if (*value < min)
      *value = min;
    if (*value > max)
      *value = max;
    changed = true;
  }

  // Update value if dragging
  if (active && g_widget_state.mouse_down) {
    float t = (g_widget_state.mouse_x - x) / w;
    if (t < 0.0f)
      t = 0.0f;
    if (t > 1.0f)
      t = 1.0f;
    float new_value = min + t * (max - min);
    if (new_value != *value) {
      *value = new_value;
      changed = true;
    }
  }

  // Render track
  stygian_rect_rounded(ctx, x, y, w, h, 0.15f, 0.15f, 0.15f, 1.0f, h * 0.5f);

  // Render filled portion
  float t = (*value - min) / (max - min);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  float fill_w = w * t;
  if (fill_w > 0.0f) {
    stygian_rect_rounded(ctx, x, y, fill_w, h, 0.4f, 0.6f, 0.9f, 1.0f,
                         h * 0.5f);
  }

  // Render thumb
  float thumb_size = h * 1.5f;
  float thumb_x = x + fill_w - thumb_size * 0.5f;
  float thumb_y = y + h * 0.5f - thumb_size * 0.5f;

  float thumb_r = 0.5f, thumb_g = 0.7f, thumb_b = 1.0f;
  if (active) {
    thumb_r = 0.3f;
    thumb_g = 0.5f;
    thumb_b = 0.8f;
  } else if (focused) {
    thumb_r = 0.55f;
    thumb_g = 0.78f;
    thumb_b = 1.0f;
  } else if (hovered) {
    thumb_r = 0.6f;
    thumb_g = 0.8f;
    thumb_b = 1.0f;
  }

  stygian_rect_rounded(ctx, thumb_x, thumb_y, thumb_size, thumb_size, thumb_r,
                       thumb_g, thumb_b, 1.0f, thumb_size * 0.5f);

  return changed;
}

bool stygian_slider_ex(StygianContext *ctx, StygianSlider *state,
                       const StygianWidgetStyle *style) {
  uint32_t id = widget_id(state->x, state->y, NULL);
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool focused;
  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               state->x, state->y, state->w, state->h);
  bool changed = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
      state->dragging = true;
    }
  }

  if (!g_widget_state.mouse_down) {
    state->dragging = false;
    if (g_widget_state.active_id == id) {
      g_widget_state.active_id = 0;
    }
  }

  if (focused &&
      (g_widget_state.nav_left_pressed || g_widget_state.nav_right_pressed ||
       g_widget_state.nav_up_pressed || g_widget_state.nav_down_pressed)) {
    float span = state->max - state->min;
    float step = span * 0.01f;
    if (step <= 0.0f)
      step = 0.01f;
    if (g_widget_state.nav_left_pressed || g_widget_state.nav_down_pressed)
      state->value -= step;
    if (g_widget_state.nav_right_pressed || g_widget_state.nav_up_pressed)
      state->value += step;
    if (state->value < state->min)
      state->value = state->min;
    if (state->value > state->max)
      state->value = state->max;
    changed = true;
  }

  if (state->dragging && g_widget_state.mouse_down) {
    float t = (g_widget_state.mouse_x - state->x) / state->w;
    if (t < 0.0f)
      t = 0.0f;
    if (t > 1.0f)
      t = 1.0f;
    float new_value = state->min + t * (state->max - state->min);
    if (new_value != state->value) {
      state->value = new_value;
      changed = true;
    }
  }

  // Render with custom style
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h,
                       style->bg_color[0], style->bg_color[1],
                       style->bg_color[2], style->bg_color[3],
                       style->border_radius);

  float t = (state->value - state->min) / (state->max - state->min);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  float fill_w = state->w * t;

  if (fill_w > 0.0f) {
    stygian_rect_rounded(ctx, state->x, state->y, fill_w, state->h,
                         style->active_color[0], style->active_color[1],
                         style->active_color[2], style->active_color[3],
                         style->border_radius);
  }

  return changed;
}

// ============================================================================
// Checkbox Widget
// ============================================================================

bool stygian_checkbox(StygianContext *ctx, StygianFont font, const char *label,
                      float x, float y, bool *checked) {
  float box_size = 20.0f;
  uint32_t id = widget_id(x, y, label);
  bool focused;

  // Calculate bounds (box + label)
  float label_w = label ? stygian_text_width(ctx, font, label, 16.0f) : 0.0f;
  float total_w = box_size + (label ? 8.0f + label_w : 0.0f);
  widget_register_region_internal(x, y, total_w, box_size,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               x, y, total_w, box_size);
  bool clicked = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);

  if (active && widget_mouse_released()) {
    if (hovered) {
      *checked = !*checked;
      clicked = true;
    }
    g_widget_state.active_id = 0; // Always clear after release
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    *checked = !*checked;
    clicked = true;
  }

  // Render checkbox box
  float bg_r = 0.2f, bg_g = 0.2f, bg_b = 0.2f;
  if (active) {
    bg_r = 0.15f;
    bg_g = 0.15f;
    bg_b = 0.15f;
  } else if (hovered) {
    bg_r = 0.3f;
    bg_g = 0.3f;
    bg_b = 0.3f;
  }

  stygian_rect_rounded(ctx, x, y, box_size, box_size, bg_r, bg_g, bg_b, 1.0f,
                       3.0f);

  // Render checkmark if checked
  if (*checked) {
    float check_padding = 4.0f;
    stygian_rect_rounded(
        ctx, x + check_padding, y + check_padding, box_size - check_padding * 2,
        box_size - check_padding * 2, 0.4f, 0.7f, 1.0f, 1.0f, 2.0f);
  }

  // Render label
  if (label) {
    float text_x = x + box_size + 8.0f;
    float text_y = y + (box_size - 16.0f) * 0.5f;
    stygian_text(ctx, font, label, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return clicked;
}

// ============================================================================
// Radio Button Widget
// ============================================================================

bool stygian_radio_button(StygianContext *ctx, StygianFont font,
                          const char *label, float x, float y, int *selected,
                          int value) {
  float circle_size = 20.0f;
  // Use selected pointer + value for stable ID across frames
  uint32_t id = widget_id(x, y, label) + (uint32_t)value;
  bool focused;

  // Calculate bounds (circle + label)
  float label_w = 0.0f;
  if (label) {
    if (font) {
      label_w = stygian_text_width(ctx, font, label, 16.0f);
    } else {
      // Estimate when no font (8px per char)
      label_w = strlen(label) * 8.0f;
    }
  }
  float total_w = circle_size + (label_w > 0 ? 8.0f + label_w : 0.0f);
  widget_register_region_internal(x, y, total_w, circle_size,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               x, y, total_w, circle_size);
  bool clicked = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);
  bool is_selected = (*selected == value);

  // Process click on mouse release
  if (active && widget_mouse_released()) {
    if (hovered) {
      *selected = value;
      clicked = true;
    }
    g_widget_state.active_id = 0; // Always clear after release
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    *selected = value;
    clicked = true;
  }

  // Render radio button circle
  float bg_r = 0.2f, bg_g = 0.2f, bg_b = 0.2f;
  if (active) {
    bg_r = 0.15f;
    bg_g = 0.15f;
    bg_b = 0.15f;
  } else if (hovered) {
    bg_r = 0.3f;
    bg_g = 0.3f;
    bg_b = 0.3f;
  }

  stygian_rect_rounded(ctx, x, y, circle_size, circle_size, bg_r, bg_g, bg_b,
                       1.0f,
                       circle_size / 2.0f); // Full circle

  // Render inner dot if selected
  if (is_selected) {
    float dot_padding = 5.0f;
    stygian_rect_rounded(ctx, x + dot_padding, y + dot_padding,
                         circle_size - dot_padding * 2,
                         circle_size - dot_padding * 2, 0.4f, 0.7f, 1.0f, 1.0f,
                         (circle_size - dot_padding * 2) / 2.0f);
  }

  // Render label
  if (label) {
    float text_x = x + circle_size + 8.0f;
    float text_y = y + (circle_size - 16.0f) * 0.5f;
    stygian_text(ctx, font, label, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return clicked;
}

// ============================================================================
// Text Input Widget
// ============================================================================

bool stygian_text_input(StygianContext *ctx, StygianFont font, float x, float y,
                        float w, float h, char *buffer, int buffer_size) {
  // Stable per-field ID: do NOT hash buffer contents (changes each keystroke).
  uint32_t id = widget_id(x, y, "text_input");
  uintptr_t buf_addr = (uintptr_t)buffer;
  id ^= (uint32_t)(buf_addr & 0xFFFFFFFFu);
  id *= 16777619u;
  id ^= (uint32_t)((buf_addr >> 32) & 0xFFFFFFFFu);
  id *= 16777619u;
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool hovered =
      point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y, x, y, w, h);
  bool changed = false;

  widget_register_focusable(id);
  widget_nav_prepare();

  // Focus management
  if (hovered && widget_mouse_pressed()) {
    g_widget_state.focus_id = id;
  } else if (!hovered && widget_mouse_pressed() &&
             g_widget_state.focus_id == id) {
    g_widget_state.focus_id = 0;
  }

  bool focused = (g_widget_state.focus_id == id);

  // Keyboard input handling (single-line input)
  if (focused && buffer && buffer_size > 1) {
    size_t len = strlen(buffer);
    int i;

    // Process key events first (editing/navigation keys).
    for (i = 0; i < g_widget_state.key_count; i++) {
      StygianKey key = g_widget_state.key_events[i].key;
      if (!g_widget_state.key_events[i].down)
        continue;

      if (key == STYGIAN_KEY_BACKSPACE) {
        if (len > 0) {
          buffer[len - 1] = '\0';
          len--;
          changed = true;
        }
      } else if (key == STYGIAN_KEY_DELETE) {
        if (len > 0) {
          buffer[0] = '\0';
          len = 0;
          changed = true;
        }
      } else if (key == STYGIAN_KEY_V &&
                 (g_widget_state.key_events[i].mods & STYGIAN_MOD_CTRL)) {
        char *clip = stygian_clipboard_pop(ctx);
        if (clip) {
          const char *p = clip;
          while (*p && (int)len < (buffer_size - 1)) {
            unsigned char c = (unsigned char)*p++;
            // Keep insertion path simple/consistent with current single-line
            // handling.
            if (c >= 32 && c <= 0x7E) {
              buffer[len++] = (char)c;
              changed = true;
            }
          }
          buffer[len] = '\0';
          free(clip);
        }
      }
    }

    // Process character input (UTF-32 codepoints from event queue).
    // Keep this simple for now: BMP/ASCII append path for chat input.
    for (i = 0; i < g_widget_state.char_count; i++) {
      uint32_t cp = g_widget_state.char_events[i];
      if (cp >= 32 && cp <= 0x7E) {
        if ((int)len < (buffer_size - 1)) {
          buffer[len++] = (char)cp;
          buffer[len] = '\0';
          changed = true;
        }
      }
    }
  }

  // Render background
  float bg_r = 0.15f, bg_g = 0.15f, bg_b = 0.15f;
  if (focused) {
    bg_r = 0.2f;
    bg_g = 0.2f;
    bg_b = 0.25f;
  } else if (hovered) {
    bg_r = 0.18f;
    bg_g = 0.18f;
    bg_b = 0.18f;
  }

  stygian_rect_rounded(ctx, x, y, w, h, bg_r, bg_g, bg_b, 1.0f, 4.0f);

  // Render border if focused
  if (focused) {
    // TODO: Use STYGIAN_RECT_OUTLINE when available
    // For now, just render a subtle highlight
    stygian_rect_rounded(ctx, x - 1, y - 1, w + 2, h + 2, 0.4f, 0.6f, 0.9f,
                         0.3f, 4.0f);
  }

  // Render text
  if (buffer && buffer[0]) {
    float text_x = x + 8.0f;
    float text_y = y + (h - 16.0f) * 0.5f;
    stygian_text(ctx, font, buffer, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  // Render cursor if focused
  if (focused) {
    float cursor_x = x + 8.0f;
    if (buffer && buffer[0]) {
      cursor_x += stygian_text_width(ctx, font, buffer, 16.0f);
    }
    float cursor_y = y + 4.0f;
    float cursor_h = h - 8.0f;

    // Blinking cursor (simple version - always visible for now)
    stygian_rect(ctx, cursor_x, cursor_y, 2.0f, cursor_h, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return changed;
}

// ============================================================================
// Text Area Widget (Multiline)
// ============================================================================

// ============================================================================
// Text Area Logic (Wrapping & Interaction)
// ============================================================================

typedef struct {
  StygianContext *ctx;
  StygianFont font;
  const char *text;
  float max_w;

  // State
  const char *p; // Current char pointer
  const char *line_start;
  float current_w; // Width accumulated for current line
  float y;         // Current Y offset
  int line_index;
  bool done;

  // Cached ASCII glyph advance widths (eliminates per-char font lookups)
  float advance_lut[128];
} TextAreaIter;

static void iter_begin(TextAreaIter *it, StygianContext *ctx, StygianFont font,
                       const char *text, float max_w) {
  it->ctx = ctx;
  it->font = font;
  it->text = text;
  it->max_w = max_w;
  it->p = text;
  it->line_start = text;
  it->current_w = 0;
  it->y = 0;
  it->line_index = 0;
  it->done = (*text == 0);

  // Pre-compute ASCII advance widths once
  for (int c = 0; c < 128; c++) {
    char temp[2] = {(char)c, 0};
    it->advance_lut[c] =
        (c >= 32) ? stygian_text_width(ctx, font, temp, 16.0f) : 0.0f;
  }
}

// Advances to next line (hard or soft break)
// Returns true if there is a line to process
static bool iter_next_line(TextAreaIter *it, const char **out_start,
                           const char **out_end) {
  if (it->done)
    return false;

  it->line_start = it->p;
  it->current_w = 0;

  const char *scan = it->p;
  const char *last_space = NULL;

  while (*scan) {
    if (*scan == '\n') {
      *out_start = it->line_start;
      *out_end = scan;
      it->p = scan + 1; // Skip newline
      it->y += 18.0f;
      return true;
    }

    // Measure char (Brutal: using fixed step or real measure)
    // Fast ASCII advance from LUT, fallback for non-ASCII
    unsigned char uc = (unsigned char)*scan;
    float cw = (uc < 128) ? it->advance_lut[uc]
                          : stygian_text_width(it->ctx, it->font,
                                               (char[]){*scan, 0}, 16.0f);

    if (it->current_w + cw > it->max_w) {
      // Soft wrap needed
      if (last_space) {
        // Break at space
        *out_start = it->line_start;
        *out_end = last_space;
        it->p = last_space + 1; // Skip space
      } else {
        // Forced break at char
        *out_start = it->line_start;
        *out_end = scan;
        it->p = scan; // Start next line here
      }
      it->y += 18.0f;
      return true;
    }

    it->current_w += cw;
    if (*scan == ' ') {
      last_space = scan;
    }
    scan++;
  }

  // End of string
  *out_start = it->line_start;
  *out_end = scan;
  it->p = scan;    // Point to null
  it->done = true; // Mark as last pass
  it->y += 18.0f;
  return true;
}

// Helper to measure a single line (with consistent "fat space" logic)
// Uses advance LUT when available, falls back to stygian_text_width.
static float measure_line_lut(const float *lut, StygianContext *ctx,
                              StygianFont font, const char *start,
                              const char *end) {
  float w = 0;
  const char *p = start;
  while (p < end) {
    unsigned char uc = (unsigned char)*p;
    float cw = (lut && uc < 128)
                   ? lut[uc]
                   : stygian_text_width(ctx, font, (char[]){*p, 0}, 16.0f);
    if (*p == ' ' && cw < 1.0f)
      cw = 4.0f; // Minimal width for spaces
    w += cw;
    p++;
  }
  return w;
}

static int text_xy_to_index(StygianContext *ctx, StygianFont font,
                            const char *text, float param_x, float param_y,
                            float scroll_y, float max_w) {
  if (!text || !*text)
    return 0;

  TextAreaIter it;
  iter_begin(&it, ctx, font, text, max_w);

  const char *start, *end;
  float target_y = param_y + scroll_y;

  // Iterate lines to find Y match
  while (iter_next_line(&it, &start, &end)) {
    // Check Y bounds of this line
    float line_top = it.y - 18.0f; // iter_next_line advances Y
    float line_bottom = it.y;

    if (target_y >= line_top && target_y < line_bottom) {
      // Found the line. Scan X.
      // Linear scan within line
      const char *scan = start;
      float lx = 0;
      while (scan < end) {
        float cw = measure_line_lut(NULL, ctx, font, scan, scan + 1);
        float mid_x = lx + cw * 0.5f;
        if (param_x < mid_x)
          return (int)(scan - text);
        lx += cw;
        scan++;
      }
      return (int)(end - text); // Clicked past end of line
    }
  }
  return (int)strlen(text); // Below all text
}

// Helper to insert character at index
static void buffer_insert(char *buf, int size, int idx, char c) {
  int len = strlen(buf);
  if (len + 1 >= size)
    return;
  memmove(buf + idx + 1, buf + idx, len - idx + 1);
  buf[idx] = c;
}

// Helper to delete character before index
static void buffer_delete(char *buf, int idx) {
  if (idx <= 0)
    return;
  int len = strlen(buf);
  memmove(buf + idx - 1, buf + idx, len - idx + 1);
}

bool stygian_text_area(StygianContext *ctx, StygianFont font,
                       StygianTextArea *state) {
  uint32_t id = widget_id(state->x, state->y, "textarea");
  StygianWidgetRegionFlags region_flags =
      STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES;
  if (state->total_height > state->h) {
    region_flags =
        (StygianWidgetRegionFlags)(region_flags | STYGIAN_WIDGET_REGION_SCROLL);
  }
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  region_flags);
  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               state->x, state->y, state->w, state->h);
  widget_register_focusable(id);
  widget_nav_prepare();

  // Input Handling
  if (hovered && widget_mouse_pressed()) {
    g_widget_state.focus_id = id;
    g_widget_state.active_id = id; // Start dragging
    state->focused = true;
    float local_x = g_widget_state.mouse_x - state->x;
    float local_y = g_widget_state.mouse_y - state->y;
    // Pass state->w - 10 (padding) as max wrap width
    int idx = text_xy_to_index(ctx, font, state->buffer, local_x, local_y,
                               state->scroll_y, state->w - 10.0f);
    state->cursor_idx = idx;

    // Shift-Click extends selection
    bool shift = stygian_key_down(stygian_get_window(ctx), STYGIAN_KEY_SHIFT);
    if (shift) {
      if (state->selection_start == -1) {
        state->selection_start = idx;
        state->selection_end = idx;
      } else {
        state->selection_end = idx;
      }
    } else {
      state->selection_start = idx;
      state->selection_end = idx;
    }
  }

  // Drag Selection
  if (g_widget_state.active_id == id && g_widget_state.mouse_down) {
    float local_x = g_widget_state.mouse_x - state->x;
    float local_y = g_widget_state.mouse_y - state->y;
    int idx = text_xy_to_index(ctx, font, state->buffer, local_x, local_y,
                               state->scroll_y, state->w - 10.0f);
    state->cursor_idx = idx;
    // Update end while keeping start as anchor
    state->selection_end = idx;
  }

  if (!g_widget_state.mouse_down && g_widget_state.active_id == id) {
    g_widget_state.active_id = 0;
  }

  // Force global sync
  if (g_widget_state.focus_id != id)
    state->focused = false;
  else
    state->focused = true;

  // Normalized range helper
  int sel_min = state->selection_start < state->selection_end
                    ? state->selection_start
                    : state->selection_end;
  int sel_max = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;
  bool has_selection = (sel_min != sel_max);

  bool changed = false;
  if (state->focused) {
    // Process Buffer Keys
    for (int i = 0; i < g_widget_state.key_count; i++) {
      if (!g_widget_state.key_events[i].down)
        continue;
      StygianKey key = g_widget_state.key_events[i].key;
      bool shift = (g_widget_state.key_events[i].mods & STYGIAN_MOD_SHIFT);

      if (key == STYGIAN_KEY_BACKSPACE) {
        if (has_selection) {
          // Delete range
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
          state->selection_start = state->selection_end = sel_min;
          changed = true;
        } else if (state->cursor_idx > 0) {
          buffer_delete(state->buffer, state->cursor_idx);
          state->cursor_idx--;
          state->selection_start = state->selection_end = state->cursor_idx;
          changed = true;
        }
      } else if (key == STYGIAN_KEY_ENTER) {
        if (has_selection) {
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
        }
        buffer_insert(state->buffer, state->buffer_size, state->cursor_idx,
                      '\n');
        state->cursor_idx++;
        state->selection_start = state->selection_end = state->cursor_idx;
        changed = true;
      } else if (key == STYGIAN_KEY_LEFT) {
        if (state->cursor_idx > 0)
          state->cursor_idx--;
        if (shift)
          state->selection_end = state->cursor_idx;
        else
          state->selection_start = state->selection_end = state->cursor_idx;
      } else if (key == STYGIAN_KEY_RIGHT) {
        if (state->buffer[state->cursor_idx] != 0)
          state->cursor_idx++;
        if (shift)
          state->selection_end = state->cursor_idx;
        else
          state->selection_start = state->selection_end = state->cursor_idx;
      } else if (key == STYGIAN_KEY_UP) {
        // TODO: Line jump logic
      } else if (key == STYGIAN_KEY_DOWN) {
        // TODO: Line jump logic
      } else if (key == STYGIAN_KEY_C &&
                 (g_widget_state.key_events[i].mods & STYGIAN_MOD_CTRL)) {
        // Universal Copy
        if (has_selection) {
          int len = sel_max - sel_min;
          if (len > 0 && len < 8192) {
            char temp[8192];
            memcpy(temp, state->buffer + sel_min, len);
            temp[len] = 0;
            stygian_clipboard_push(ctx, temp, NULL);
          }
        } else {
          stygian_clipboard_push(ctx, state->buffer, NULL);
        }
      } else if (key == STYGIAN_KEY_V &&
                 (g_widget_state.key_events[i].mods & STYGIAN_MOD_CTRL)) {
        // Universal Paste
        if (has_selection) {
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
        }
        char *clip = stygian_clipboard_pop(ctx);
        if (clip) {
          const char *p = clip;
          while (*p) {
            buffer_insert(state->buffer, state->buffer_size, state->cursor_idx,
                          *p);
            state->cursor_idx++;
            p++;
          }
          free(clip); // Use standard free (matches _strdup)
          state->selection_start = state->selection_end = state->cursor_idx;
          changed = true;
        }
      }
    }

    // Process Char Input
    for (int i = 0; i < g_widget_state.char_count; i++) {
      char c = (char)g_widget_state.char_events[i];
      if (c >= 32 && c <= 126) { // Printable ASCII for now
        if (has_selection) {
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
          // state->selection_start = state->selection_end = sel_min; //
          // handled at end
          has_selection = false;
          sel_min = sel_max = state->cursor_idx;
        }
        buffer_insert(state->buffer, state->buffer_size, state->cursor_idx, c);
        state->cursor_idx++;
        state->selection_start = state->selection_end = state->cursor_idx;
        changed = true;
      }
    }
  }
  // Scroll Handling
  if (hovered &&
      (g_widget_state.scroll_dy != 0 || g_widget_state.scroll_dx != 0)) {
    state->scroll_y -= g_widget_state.scroll_dy * 20.0f;
  }
  // Clamp scroll
  if (state->scroll_y < 0.0f)
    state->scroll_y = 0;
  if (state->total_height > state->h &&
      state->scroll_y > state->total_height - state->h)
    state->scroll_y = state->total_height - state->h;

  // Render Background
  float bg_col[4] = {0.1f, 0.1f, 0.12f, 1.0f};
  if (state->focused) {
    bg_col[0] = 0.12f;
    bg_col[1] = 0.12f;
    bg_col[2] = 0.15f;
  }
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, bg_col[0],
                       bg_col[1], bg_col[2], 1.0f, 4.0f);

  // Clip Content
  stygian_clip_push(ctx, state->x, state->y, state->w, state->h);

  // Render Text Line by Line (Wrapped)
  float x_off = state->x + 5.0f;
  bool show_scrollbar = state->total_height > state->h;
  float max_w = state->w - (show_scrollbar ? 14.0f : 10.0f);
  if (max_w < 20.0f)
    max_w = 20.0f;

  TextAreaIter it;
  iter_begin(&it, ctx, font, state->buffer, max_w);
  const char *start, *end;

  while (iter_next_line(&it, &start, &end)) {
    // Draw visible lines
    float line_top = it.y - 18.0f;
    float abs_top = state->y + line_top - state->scroll_y;

    if (abs_top + 18.0f > state->y && abs_top < state->y + state->h) {
      float lx = x_off;

      // Draw Selection Rect (Block)
      if (has_selection) {
        int idx_start = (int)(start - state->buffer);
        int idx_end = (int)(end - state->buffer);

        int intersect_min = (sel_min > idx_start) ? sel_min : idx_start;
        int intersect_max = (sel_max < idx_end) ? sel_max : idx_end;

        if (intersect_min < intersect_max) {
          // Valid intersection on this line
          float pre_width = measure_line_lut(NULL, ctx, font, start,
                                             state->buffer + intersect_min);
          float sel_width =
              measure_line_lut(NULL, ctx, font, state->buffer + intersect_min,
                               state->buffer + intersect_max);

          stygian_rect(ctx, lx + pre_width, abs_top, sel_width, 18.0f, 0.2f,
                       0.4f, 0.8f, 0.5f);
        }
      }

      const char *lp = start;
      while (lp < end) {
        char temp[2] = {*lp, 0};
        // Re-measure for consistent placement
        float cw = stygian_text_width(ctx, font, temp, 16.0f);
        if (*lp == ' ' && cw < 1.0f)
          cw = 4.0f;

        stygian_text(ctx, font, temp, lx, abs_top, 16.0f, 0.9f, 0.9f, 0.9f,
                     1.0f);
        lx += cw;
        lp++;
      }

      // Cursor
      if (state->focused) {
        int idx_start = (int)(start - state->buffer);
        int idx_end = (int)(end - state->buffer);
        if (state->cursor_idx >= idx_start && state->cursor_idx <= idx_end) {
          float cx =
              x_off + measure_line_lut(NULL, ctx, font, start,
                                       state->buffer + state->cursor_idx);
          stygian_rect(ctx, cx, abs_top, 2.0f, 16.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        }
      }
    }
  }

  stygian_clip_pop(ctx);
  // Auto-scroll logic could be added here
  state->total_height = it.y;

  stygian_scrollbar_v(ctx, state->x + state->w - 8.0f, state->y + 2.0f, 6.0f,
                      state->h - 4.0f, state->total_height, &state->scroll_y);

  return changed;
}

bool stygian_scrollbar_v(StygianContext *ctx, float x, float y, float w,
                         float h, float content_height, float *scroll_y) {
  uint32_t id;
  float max_scroll;
  float thumb_h;
  float travel;
  float ratio;
  float thumb_y;
  bool changed = false;
  bool hovered;
  bool thumb_hovered;
  bool mouse_pressed;
  bool mouse_released;

  if (!ctx || !scroll_y || h <= 1.0f || w <= 1.0f)
    return false;

  id = widget_id(x, y, "vscroll");
  max_scroll = content_height - h;
  if (max_scroll < 0.0f)
    max_scroll = 0.0f;
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  if (max_scroll > 0.0f) {
    widget_register_region_internal(x, y, w, h, STYGIAN_WIDGET_REGION_SCROLL);
  }
  if (*scroll_y < 0.0f)
    *scroll_y = 0.0f;
  if (*scroll_y > max_scroll)
    *scroll_y = max_scroll;

  // Draw track always so layout is stable.
  stygian_rect_rounded(ctx, x, y, w, h, 0.16f, 0.16f, 0.18f, 0.55f, 3.0f);

  if (max_scroll <= 0.0f)
    return false;

  thumb_h = h * (h / content_height);
  if (thumb_h < 18.0f)
    thumb_h = 18.0f;
  if (thumb_h > h)
    thumb_h = h;

  travel = h - thumb_h;
  ratio = (max_scroll > 0.0f) ? (*scroll_y / max_scroll) : 0.0f;
  if (ratio < 0.0f)
    ratio = 0.0f;
  if (ratio > 1.0f)
    ratio = 1.0f;
  thumb_y = y + ratio * travel;

  hovered = point_in_rect((float)g_widget_state.mouse_x,
                          (float)g_widget_state.mouse_y, x, y, w, h);
  thumb_hovered =
      point_in_rect((float)g_widget_state.mouse_x,
                    (float)g_widget_state.mouse_y, x, thumb_y, w, thumb_h);
  mouse_pressed = widget_mouse_pressed();
  mouse_released = widget_mouse_released();

  if (hovered && g_widget_state.scroll_dy != 0.0f) {
    *scroll_y -= g_widget_state.scroll_dy * 24.0f;
    if (*scroll_y < 0.0f)
      *scroll_y = 0.0f;
    if (*scroll_y > max_scroll)
      *scroll_y = max_scroll;
    changed = true;
  }

  if (thumb_hovered)
    g_widget_state.hot_id = id;

  if (mouse_pressed && hovered) {
    g_widget_state.active_id = id;
    if (!thumb_hovered) {
      // Page jump to the clicked track position.
      float local = (float)g_widget_state.mouse_y - y - thumb_h * 0.5f;
      if (local < 0.0f)
        local = 0.0f;
      if (local > travel)
        local = travel;
      *scroll_y = (travel > 0.0f) ? (local / travel) * max_scroll : 0.0f;
      changed = true;
    }
  }

  if (g_widget_state.active_id == id) {
    if (g_widget_state.mouse_down) {
      float local = (float)g_widget_state.mouse_y - y - thumb_h * 0.5f;
      if (local < 0.0f)
        local = 0.0f;
      if (local > travel)
        local = travel;
      {
        float new_scroll =
            (travel > 0.0f) ? (local / travel) * max_scroll : 0.0f;
        if (fabsf(new_scroll - *scroll_y) > 0.01f) {
          *scroll_y = new_scroll;
          changed = true;
        }
      }
    } else if (mouse_released) {
      g_widget_state.active_id = 0;
    }
  }

  // Recompute thumb after any change.
  ratio = (max_scroll > 0.0f) ? (*scroll_y / max_scroll) : 0.0f;
  if (ratio < 0.0f)
    ratio = 0.0f;
  if (ratio > 1.0f)
    ratio = 1.0f;
  thumb_y = y + ratio * travel;

  {
    bool active = (g_widget_state.active_id == id);
    float r = active ? 0.64f : (thumb_hovered ? 0.56f : 0.48f);
    float g = active ? 0.67f : (thumb_hovered ? 0.58f : 0.5f);
    float b = active ? 0.76f : (thumb_hovered ? 0.66f : 0.56f);
    float a = active ? 0.95f : 0.88f;
    stygian_rect_rounded(ctx, x, thumb_y, w, thumb_h, r, g, b, a, 3.0f);
  }

  return changed;
}

// ============================================================================
// Panel Widget
// ============================================================================

static struct {
  float x, y, w, h;
  uint8_t clip_id;
  bool active;
} g_panel_state = {0};

void stygian_panel_begin(StygianContext *ctx, float x, float y, float w,
                         float h) {
  // Render panel background
  stygian_rect_rounded(ctx, x, y, w, h, 0.12f, 0.12f, 0.12f, 1.0f, 6.0f);

  // Set up clipping
  g_panel_state.x = x;
  g_panel_state.y = y;
  g_panel_state.w = w;
  g_panel_state.h = h;
  g_panel_state.clip_id = stygian_clip_push(ctx, x, y, w, h);
  g_panel_state.active = true;
}

void stygian_panel_end(StygianContext *ctx) {
  if (g_panel_state.active) {
    stygian_clip_pop(ctx);
    g_panel_state.active = false;
  }
}

// ============================================================================
// Node Graph Editor (Spatial JIT Implementation)
// ============================================================================

// Helper: Cubic Bezier wire (single element, optimized)
static void draw_cubic_bezier(StygianContext *ctx, float x1, float y1, float x2,
                              float y2, float thick, float color[4]) {
  // Control points for S-curve (standard node editor style)
  float cp1x = x1 + (x2 - x1) * 0.5f;
  float cp1y = y1;
  float cp2x = x1 + (x2 - x1) * 0.5f;
  float cp2y = y2;

  // Single SDF cubic bezier element - no gap artifacts, low overhead
  stygian_wire(ctx, x1, y1, cp1x, cp1y, cp2x, cp2y, x2, y2, thick, color[0],
               color[1], color[2], color[3]);
}

static void draw_straight_wire(StygianContext *ctx, float x1, float y1,
                               float x2, float y2, float thick,
                               float color[4]) {
  float mx = (x1 + x2) * 0.5f;
  float my = (y1 + y2) * 0.5f;
  stygian_wire(ctx, x1, y1, mx, my, mx, my, x2, y2, thick, color[0], color[1],
               color[2], color[3]);
}

static void draw_orthogonal_wire(StygianContext *ctx, float x1, float y1,
                                 float x2, float y2, float thick,
                                 float color[4]) {
  float mid_x = (x1 + x2) * 0.5f;
  draw_straight_wire(ctx, x1, y1, mid_x, y1, thick, color);
  draw_straight_wire(ctx, mid_x, y1, mid_x, y2, thick, color);
  draw_straight_wire(ctx, mid_x, y2, x2, y2, thick, color);
}

static void graph_view_bounds(const StygianGraphState *state, float padding,
                              float *l, float *t, float *r, float *b) {
  float pad = (padding > 0.0f) ? padding : 0.0f;
  *l = -state->pan_x - (state->w * 0.5f) / state->zoom - pad;
  *r = -state->pan_x + (state->w * 0.5f) / state->zoom + pad;
  *t = -state->pan_y - (state->h * 0.5f) / state->zoom - pad;
  *b = -state->pan_y + (state->h * 0.5f) / state->zoom + pad;
}

void stygian_node_graph_begin(StygianContext *ctx, StygianGraphState *state,
                              StygianNodeBuffers *data, int count) {
  // 1. Handle Input (Pan / Zoom)
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES |
                                      STYGIAN_WIDGET_REGION_SCROLL);
  bool hovered = point_in_rect((float)g_widget_state.mouse_x,
                               (float)g_widget_state.mouse_y, state->x,
                               state->y, state->w, state->h);

  // Pan: Middle Mouse
  if (hovered &&
      stygian_mouse_down(stygian_get_window(ctx), STYGIAN_MOUSE_MIDDLE)) {
    state->pan_x += g_widget_state.mouse_dx / state->zoom;
    state->pan_y += g_widget_state.mouse_dy / state->zoom;
  }

  // Zoom: Scroll events
  if (hovered && g_widget_state.scroll_dy != 0) {
    float prev_zoom = state->zoom;
    float zoom_factor = 1.0f + (g_widget_state.scroll_dy * 0.1f);

    float center_x = state->x + (state->w * 0.5f);
    float center_y = state->y + (state->h * 0.5f);

    float screen_mx = (float)g_widget_state.mouse_x;
    float screen_my = (float)g_widget_state.mouse_y;

    float world_x = (screen_mx - center_x) / prev_zoom - state->pan_x;
    float world_y = (screen_my - center_y) / prev_zoom - state->pan_y;

    state->zoom *= zoom_factor;
    if (state->zoom < 0.05f)
      state->zoom = 0.05f;
    if (state->zoom > 10.0f)
      state->zoom = 10.0f;

    state->pan_x = (screen_mx - center_x) / state->zoom - world_x;
    state->pan_y = (screen_my - center_y) / state->zoom - world_y;
  }

  // Node Dragging Logic
  if (hovered &&
      !stygian_mouse_down(stygian_get_window(ctx), STYGIAN_MOUSE_MIDDLE)) {
    if (widget_mouse_pressed()) {
      // Try to pick a node
      int pick =
          stygian_graph_pick_node(state, data, (float)g_widget_state.mouse_x,
                                  (float)g_widget_state.mouse_y);
      if (pick >= 0) {
        state->dragging_id = pick + 1; // 1-based
      }
    }
  }

  if (!g_widget_state.mouse_down) {
    state->dragging_id = 0;
  }

  if (state->dragging_id > 0) {
    int idx = state->dragging_id - 1;
    data->x[idx] += g_widget_state.mouse_dx / state->zoom;
    data->y[idx] += g_widget_state.mouse_dy / state->zoom;
    if (state->snap_enabled && state->snap_size > 0.0f) {
      stygian_graph_snap_pos(state, &data->x[idx], &data->y[idx]);
    }
  }

  // 2. Spatial JIT Culling
  state->iter_idx = 0;
  state->visible_count = 0;

  float view_l = 0.0f;
  float view_t = 0.0f;
  float view_r = 0.0f;
  float view_b = 0.0f;
  graph_view_bounds(state, 0.0f, &view_l, &view_t, &view_r, &view_b);

  // Linear Cull
  for (int i = 0; i < count; ++i) {
    float nx = data->x[i];
    float ny = data->y[i];
    float nw = data->w[i];
    float nh = data->h[i];

    if (nx + nw > view_l && nx < view_r && ny + nh > view_t && ny < view_b) {
      if (state->visible_count < 8192) {
        state->visible_ids[state->visible_count++] = i;
      }
    }
  }

  // 3. Draw Background Grid
  stygian_clip_push(ctx, state->x, state->y, state->w, state->h);
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.05f, 0.05f, 0.05f,
               1.0f);

  // Grid Lines (default)
  stygian_graph_draw_grid(ctx, state, 100.0f, 20.0f, 0.15f, 0.15f, 0.15f, 0.5f);
}

bool stygian_node_graph_next(StygianContext *ctx, StygianGraphState *state,
                             int *out_index) {
  if (state->iter_idx < state->visible_count) {
    *out_index = state->visible_ids[state->iter_idx++];
    return true;
  }
  return false;
}

bool stygian_node_def(StygianContext *ctx, const char *title, float x, float y,
                      float w, float h, bool selected) {
  stygian_rect_rounded(ctx, x, y, w, h, 0.15f, 0.15f, 0.18f, 1.0f, 8.0f);
  stygian_rect_rounded(ctx, x, y, w, 24.0f, 0.25f, 0.25f, 0.28f, 1.0f, 8.0f);
  stygian_text(ctx, 0, title, x + 10, y + 4, 16.0f, 0.9f, 0.9f, 0.9f, 1.0f);
  return false;
}

void stygian_node_graph_end(StygianContext *ctx, StygianGraphState *state) {
  stygian_clip_pop(ctx);
}

void stygian_node_link(StygianContext *ctx, float x1, float y1, float x2,
                       float y2, float thick, float color[4]) {
  draw_cubic_bezier(ctx, x1, y1, x2, y2, thick, color);
}

void stygian_graph_link(StygianContext *ctx, const StygianGraphState *state,
                        float x1, float y1, float x2, float y2, float thick,
                        float color[4]) {
  int style = state ? state->wire_style : STYGIAN_WIRE_SMOOTH;
  if (style == STYGIAN_WIRE_SHARP) {
    draw_orthogonal_wire(ctx, x1, y1, x2, y2, thick, color);
  } else {
    draw_cubic_bezier(ctx, x1, y1, x2, y2, thick, color);
  }
}

void stygian_graph_set_wire_style(StygianGraphState *state, int style) {
  if (!state)
    return;
  state->wire_style = style;
}

void stygian_graph_set_snap(StygianGraphState *state, bool enabled,
                            float size) {
  if (!state)
    return;
  state->snap_enabled = enabled;
  state->snap_size = (size > 0.0f) ? size : 0.0f;
}

void stygian_graph_snap_pos(const StygianGraphState *state, float *x,
                            float *y) {
  if (!state || !state->snap_enabled || state->snap_size <= 0.0f || !x || !y)
    return;
  float s = state->snap_size;
  *x = floorf((*x / s) + 0.5f) * s;
  *y = floorf((*y / s) + 0.5f) * s;
}

void stygian_graph_world_to_screen(const StygianGraphState *state, float wx,
                                   float wy, float *sx, float *sy) {
  if (!state || !sx || !sy)
    return;
  float center_x = state->x + (state->w * 0.5f);
  float center_y = state->y + (state->h * 0.5f);
  *sx = (wx + state->pan_x) * state->zoom + center_x;
  *sy = (wy + state->pan_y) * state->zoom + center_y;
}

void stygian_graph_screen_to_world(const StygianGraphState *state, float sx,
                                   float sy, float *wx, float *wy) {
  if (!state || !wx || !wy)
    return;
  float center_x = state->x + (state->w * 0.5f);
  float center_y = state->y + (state->h * 0.5f);
  *wx = (sx - center_x) / state->zoom - state->pan_x;
  *wy = (sy - center_y) / state->zoom - state->pan_y;
}

void stygian_graph_node_screen_rect(const StygianGraphState *state, float wx,
                                    float wy, float ww, float wh, float *sx,
                                    float *sy, float *sw, float *sh) {
  if (!state || !sx || !sy || !sw || !sh)
    return;
  stygian_graph_world_to_screen(state, wx, wy, sx, sy);
  *sw = ww * state->zoom;
  *sh = wh * state->zoom;
}

void stygian_graph_pin_center_world(const StygianGraphState *state, float wx,
                                    float wy, float ww, bool output, float *px,
                                    float *py) {
  if (!state || !px || !py)
    return;
  float offset_y = (state->pin_y_offset > 0.0f) ? state->pin_y_offset : 48.0f;
  *px = output ? (wx + ww) : wx;
  *py = wy + offset_y;
}

void stygian_graph_pin_rect_screen(const StygianGraphState *state, float wx,
                                   float wy, float ww, bool output, float *x,
                                   float *y, float *w, float *h) {
  if (!state || !x || !y || !w || !h)
    return;
  float psize =
      (state->pin_size > 0.0f) ? state->pin_size : (16.0f * state->zoom);
  float px_world = 0.0f;
  float py_world = 0.0f;
  stygian_graph_pin_center_world(state, wx, wy, ww, output, &px_world,
                                 &py_world);
  float sx = 0.0f;
  float sy = 0.0f;
  stygian_graph_world_to_screen(state, px_world, py_world, &sx, &sy);
  *w = psize;
  *h = psize;
  *x = sx - (*w * 0.5f);
  *y = sy - (*h * 0.5f);
}

bool stygian_graph_pin_hit_test(const StygianGraphState *state, float wx,
                                float wy, float ww, bool output, float mx,
                                float my) {
  if (!state)
    return false;
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
  stygian_graph_pin_rect_screen(state, wx, wy, ww, output, &x, &y, &w, &h);
  return point_in_rect(mx, my, x, y, w, h);
}

bool stygian_graph_link_visible(const StygianGraphState *state, float ax,
                                float ay, float bx, float by, float padding) {
  if (!state)
    return true;
  float view_l = 0.0f;
  float view_t = 0.0f;
  float view_r = 0.0f;
  float view_b = 0.0f;
  graph_view_bounds(state, padding, &view_l, &view_t, &view_r, &view_b);
  return ((ax > view_l || bx > view_l) && (ax < view_r || bx < view_r) &&
          (ay > view_t || by > view_t) && (ay < view_b || by < view_b));
}

bool stygian_graph_link_visible_bezier(const StygianGraphState *state, float x1,
                                       float y1, float x2, float y2,
                                       float padding) {
  if (!state)
    return true;
  float mid_x = (x1 + x2) * 0.5f;
  float cp1x = mid_x;
  float cp1y = y1;
  float cp2x = mid_x;
  float cp2y = y2;
  float minx = x1;
  if (cp1x < minx)
    minx = cp1x;
  if (cp2x < minx)
    minx = cp2x;
  if (x2 < minx)
    minx = x2;
  float maxx = x1;
  if (cp1x > maxx)
    maxx = cp1x;
  if (cp2x > maxx)
    maxx = cp2x;
  if (x2 > maxx)
    maxx = x2;
  float miny = y1;
  if (cp1y < miny)
    miny = cp1y;
  if (cp2y < miny)
    miny = cp2y;
  if (y2 < miny)
    miny = y2;
  float maxy = y1;
  if (cp1y > maxy)
    maxy = cp1y;
  if (cp2y > maxy)
    maxy = cp2y;
  if (y2 > maxy)
    maxy = y2;
  float view_l = 0.0f;
  float view_t = 0.0f;
  float view_r = 0.0f;
  float view_b = 0.0f;
  graph_view_bounds(state, padding, &view_l, &view_t, &view_r, &view_b);
  return (maxx > view_l && minx < view_r && maxy > view_t && miny < view_b);
}

void stygian_graph_draw_grid(StygianContext *ctx,
                             const StygianGraphState *state, float major,
                             float minor, float r, float g, float b, float a) {
  (void)minor;
  if (!ctx || !state || major <= 0.0f)
    return;
  float world_l = 0.0f;
  float world_t = 0.0f;
  float world_r = 0.0f;
  float world_b = 0.0f;
  graph_view_bounds(state, 0.0f, &world_l, &world_t, &world_r, &world_b);
  float grid_start_x = floorf(world_l / major) * major;
  float grid_start_y = floorf(world_t / major) * major;
  int line_count = 0;
  for (float wx = grid_start_x; wx < world_r && line_count < 200;
       wx += major, line_count++) {
    float sx = (wx + state->pan_x) * state->zoom + state->x + state->w * 0.5f;
    if (sx >= state->x && sx <= state->x + state->w) {
      stygian_rect(ctx, sx, state->y, 1.0f, state->h, r, g, b, a);
    }
  }
  line_count = 0;
  for (float wy = grid_start_y; wy < world_b && line_count < 200;
       wy += major, line_count++) {
    float sy = (wy + state->pan_y) * state->zoom + state->y + state->h * 0.5f;
    if (sy >= state->y && sy <= state->y + state->h) {
      stygian_rect(ctx, state->x, sy, state->w, 1.0f, r, g, b, a);
    }
  }
}

bool stygian_graph_node_hit_test(const StygianGraphState *state, float wx,
                                 float wy, float ww, float wh, float mx,
                                 float my) {
  if (!state)
    return false;
  float sx = 0.0f;
  float sy = 0.0f;
  float sw = 0.0f;
  float sh = 0.0f;
  stygian_graph_node_screen_rect(state, wx, wy, ww, wh, &sx, &sy, &sw, &sh);
  return point_in_rect(mx, my, sx, sy, sw, sh);
}

int stygian_graph_pick_node(const StygianGraphState *state,
                            const StygianNodeBuffers *data, float mx,
                            float my) {
  if (!state || !data)
    return -1;
  for (int i = state->visible_count - 1; i >= 0; i--) {
    int idx = state->visible_ids[i];
    if (stygian_graph_node_hit_test(state, data->x[idx], data->y[idx],
                                    data->w[idx], data->h[idx], mx, my)) {
      return idx;
    }
  }
  return -1;
}
