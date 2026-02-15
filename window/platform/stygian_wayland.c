// stygian_wayland.c - Linux/Wayland Platform Implementation (STUB)
// Part of Stygian UI Library
// TODO: Implement when Wayland support is needed

#if defined(__linux__) && !defined(__ANDROID__) && defined(STYGIAN_USE_WAYLAND)

#include "../stygian_window.h"
#include <stdio.h>
#include <stdlib.h>

// Stub implementations - will be filled in when Wayland support is added

struct StygianWindow {
  int width, height;
  bool should_close;
  bool maximized;
  bool minimized;
  bool fullscreen;
  StygianTitlebarBehavior titlebar_behavior;
  // TODO: Wayland handles (wl_display, wl_surface, etc.)
};

StygianWindow *stygian_window_create(const StygianWindowConfig *config) {
  fprintf(stderr, "[Stygian] Linux/Wayland support not yet implemented\n");
  (void)config;
  return NULL;
}

StygianWindow *stygian_window_create_simple(int w, int h, const char *title) {
  fprintf(stderr, "[Stygian] Linux/Wayland support not yet implemented\n");
  (void)w;
  (void)h;
  (void)title;
  return NULL;
}

void stygian_window_destroy(StygianWindow *win) { free(win); }
bool stygian_window_should_close(StygianWindow *win) {
  return win ? win->should_close : true;
}
void stygian_window_request_close(StygianWindow *win) {
  if (win)
    win->should_close = true;
}
void stygian_window_get_size(StygianWindow *win, int *w, int *h) {
  if (win) {
    if (w)
      *w = win->width;
    if (h)
      *h = win->height;
  }
}
void stygian_window_set_size(StygianWindow *win, int w, int h) {
  (void)win;
  (void)w;
  (void)h;
}
void stygian_window_get_position(StygianWindow *win, int *x, int *y) {
  (void)win;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}
void stygian_window_set_position(StygianWindow *win, int x, int y) {
  (void)win;
  (void)x;
  (void)y;
}
void stygian_window_set_title(StygianWindow *win, const char *title) {
  (void)win;
  (void)title;
}
void stygian_window_minimize(StygianWindow *win) {
  if (!win)
    return;
  win->minimized = true;
  win->maximized = false;
}
void stygian_window_maximize(StygianWindow *win) {
  if (!win)
    return;
  win->maximized = true;
  win->minimized = false;
}
void stygian_window_restore(StygianWindow *win) {
  if (!win)
    return;
  win->maximized = false;
  win->minimized = false;
}
bool stygian_window_is_maximized(StygianWindow *win) {
  return win ? win->maximized : false;
}
bool stygian_window_is_minimized(StygianWindow *win) {
  return win ? win->minimized : false;
}
void stygian_window_set_fullscreen(StygianWindow *win, bool enabled) {
  if (!win)
    return;
  win->fullscreen = enabled;
}
bool stygian_window_is_fullscreen(StygianWindow *win) {
  return win ? win->fullscreen : false;
}
void stygian_window_focus(StygianWindow *win) { (void)win; }
bool stygian_window_is_focused(StygianWindow *win) {
  (void)win;
  return false;
}
void stygian_window_get_titlebar_hints(StygianWindow *win,
                                       StygianTitlebarHints *out_hints) {
  (void)win;
  if (!out_hints)
    return;
  out_hints->button_order = STYGIAN_TITLEBAR_BUTTONS_RIGHT;
  out_hints->supports_hover_menu = false;
  out_hints->supports_snap_actions = false;
  out_hints->recommended_titlebar_height = 36.0f;
  out_hints->recommended_button_width = 28.0f;
  out_hints->recommended_button_height = 24.0f;
  out_hints->recommended_button_gap = 6.0f;
}
void stygian_window_set_titlebar_behavior(
    StygianWindow *win, const StygianTitlebarBehavior *behavior) {
  if (!win || !behavior)
    return;
  win->titlebar_behavior = *behavior;
}
void stygian_window_get_titlebar_behavior(StygianWindow *win,
                                          StygianTitlebarBehavior *out_behavior) {
  if (!out_behavior)
    return;
  out_behavior->double_click_mode = STYGIAN_TITLEBAR_DBLCLICK_MAXIMIZE_RESTORE;
  out_behavior->hover_menu_enabled = false;
  if (win)
    *out_behavior = win->titlebar_behavior;
}
bool stygian_window_begin_system_move(StygianWindow *win) {
  (void)win;
  return false;
}
void stygian_window_titlebar_double_click(StygianWindow *win) {
  if (!win)
    return;
  if (win->titlebar_behavior.double_click_mode ==
      STYGIAN_TITLEBAR_DBLCLICK_FULLSCREEN_TOGGLE) {
    win->fullscreen = !win->fullscreen;
    return;
  }
  win->maximized = !win->maximized;
}
uint32_t stygian_window_get_titlebar_menu_actions(
    StygianWindow *win, StygianTitlebarMenuAction *out_actions,
    uint32_t max_actions) {
  uint32_t count = 0;
  if (!win)
    return 0;
  if (out_actions && count < max_actions) {
    out_actions[count] = win->maximized ? STYGIAN_TITLEBAR_ACTION_RESTORE
                                        : STYGIAN_TITLEBAR_ACTION_MAXIMIZE;
  }
  count++;
  if (out_actions && count < max_actions) {
    out_actions[count] = win->fullscreen
                             ? STYGIAN_TITLEBAR_ACTION_EXIT_FULLSCREEN
                             : STYGIAN_TITLEBAR_ACTION_ENTER_FULLSCREEN;
  }
  count++;
  return count;
}
bool stygian_window_apply_titlebar_menu_action(StygianWindow *win,
                                               StygianTitlebarMenuAction action) {
  if (!win)
    return false;
  switch (action) {
  case STYGIAN_TITLEBAR_ACTION_RESTORE:
    stygian_window_restore(win);
    return true;
  case STYGIAN_TITLEBAR_ACTION_MAXIMIZE:
    stygian_window_maximize(win);
    return true;
  case STYGIAN_TITLEBAR_ACTION_ENTER_FULLSCREEN:
    stygian_window_set_fullscreen(win, true);
    return true;
  case STYGIAN_TITLEBAR_ACTION_EXIT_FULLSCREEN:
    stygian_window_set_fullscreen(win, false);
    return true;
  default:
    return false;
  }
}
bool stygian_window_poll_event(StygianWindow *win, StygianEvent *event) {
  (void)win;
  event->type = STYGIAN_EVENT_NONE;
  return false;
}
void stygian_window_wait_event(StygianWindow *win, StygianEvent *event) {
  (void)win;
  event->type = STYGIAN_EVENT_NONE;
}
bool stygian_window_wait_event_timeout(StygianWindow *win, StygianEvent *event,
                                       uint32_t timeout_ms) {
  (void)win;
  (void)timeout_ms;
  if (event)
    event->type = STYGIAN_EVENT_NONE;
  return false;
}
void stygian_window_process_events(StygianWindow *win) { (void)win; }
void stygian_window_make_current(StygianWindow *win) { (void)win; }
void stygian_window_swap_buffers(StygianWindow *win) { (void)win; }
void stygian_window_set_vsync(StygianWindow *win, bool enabled) {
  (void)win;
  (void)enabled;
}
void *stygian_window_native_handle(StygianWindow *win) {
  (void)win;
  return NULL;
}
void *stygian_window_native_context(StygianWindow *win) {
  (void)win;
  return NULL;
}
void stygian_window_set_cursor(StygianWindow *win, StygianCursor cursor) {
  (void)win;
  (void)cursor;
}
void stygian_window_hide_cursor(StygianWindow *win) { (void)win; }
void stygian_window_show_cursor(StygianWindow *win) { (void)win; }
float stygian_window_get_dpi_scale(StygianWindow *win) {
  (void)win;
  return 1.0f;
}
void stygian_window_get_framebuffer_size(StygianWindow *win, int *w, int *h) {
  stygian_window_get_size(win, w, h);
}
bool stygian_key_down(StygianWindow *win, StygianKey key) {
  (void)win;
  (void)key;
  return false;
}
bool stygian_mouse_down(StygianWindow *win, StygianMouseButton button) {
  (void)win;
  (void)button;
  return false;
}
void stygian_mouse_pos(StygianWindow *win, int *x, int *y) {
  (void)win;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}
uint32_t stygian_get_mods(StygianWindow *win) {
  (void)win;
  return 0;
}

// ============================================================================
// OpenGL Hooks (stub)
// ============================================================================

bool stygian_window_gl_set_pixel_format(StygianWindow *win) {
  (void)win;
  return false;
}

void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx) {
  (void)win;
  (void)share_ctx;
  return NULL;
}

void stygian_window_gl_destroy_context(void *ctx) { (void)ctx; }

bool stygian_window_gl_make_current(StygianWindow *win, void *ctx) {
  (void)win;
  (void)ctx;
  return false;
}

void stygian_window_gl_swap_buffers(StygianWindow *win) { (void)win; }

void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled) {
  (void)win;
  (void)enabled;
}

void *stygian_window_gl_get_proc_address(const char *name) {
  (void)name;
  return NULL;
}

// ============================================================================
// Vulkan Hooks (stub)
// ============================================================================

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts) {
  (void)out_exts;
  (void)max_exts;
  return 0;
}

bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **out_surface) {
  (void)win;
  (void)vk_instance;
  (void)out_surface;
  return false;
}

#endif // __linux__ && STYGIAN_USE_WAYLAND
