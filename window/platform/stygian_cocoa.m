// stygian_cocoa.m - macOS/Cocoa Platform Implementation (STUB)
// Part of Stygian UI Library
// TODO: Implement when macOS support is needed

#if defined(__APPLE__)

#include "../stygian_window.h"
#include "stygian_cocoa_shim.h"
#include <stdio.h>
#include <stdlib.h>


// Stub implementations - will be filled in when macOS support is added

struct StygianWindow {
  int width, height;
  bool should_close;
  // TODO: Cocoa handles (NSWindow, NSOpenGLContext)
};

StygianWindow *stygian_window_create(const StygianWindowConfig *config) {
  fprintf(stderr, "[Stygian] macOS/Cocoa support not yet implemented\n");
  (void)config;
  return NULL;
}

StygianWindow *stygian_window_create_simple(int w, int h, const char *title) {
  fprintf(stderr, "[Stygian] macOS/Cocoa support not yet implemented\n");
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
void stygian_window_minimize(StygianWindow *win) { (void)win; }
void stygian_window_maximize(StygianWindow *win) { (void)win; }
void stygian_window_restore(StygianWindow *win) { (void)win; }
bool stygian_window_is_maximized(StygianWindow *win) {
  (void)win;
  return false;
}
bool stygian_window_is_minimized(StygianWindow *win) {
  (void)win;
  return false;
}
void stygian_window_focus(StygianWindow *win) { (void)win; }
bool stygian_window_is_focused(StygianWindow *win) {
  (void)win;
  return false;
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
  return stygian_cocoa_gl_set_pixel_format(win);
}

void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx) {
  return stygian_cocoa_gl_create_context(win, share_ctx);
}

void stygian_window_gl_destroy_context(void *ctx) {
  stygian_cocoa_gl_destroy_context(ctx);
}

bool stygian_window_gl_make_current(StygianWindow *win, void *ctx) {
  return stygian_cocoa_gl_make_current(win, ctx);
}

void stygian_window_gl_swap_buffers(StygianWindow *win) {
  stygian_cocoa_gl_swap_buffers(win);
}

void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled) {
  stygian_cocoa_gl_set_vsync(win, enabled);
}

void *stygian_window_gl_get_proc_address(const char *name) {
  return stygian_cocoa_gl_get_proc_address(name);
}

// ============================================================================
// Vulkan Hooks (stub)
// ============================================================================

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts) {
  return stygian_cocoa_vk_get_instance_extensions(out_exts, max_exts);
}

bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **out_surface) {
  return stygian_cocoa_vk_create_surface(win, vk_instance, out_surface);
}

#endif // __APPLE__
