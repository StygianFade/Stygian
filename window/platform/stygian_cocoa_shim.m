// stygian_cocoa_shim.m - macOS Cocoa shim (STUB)
// Part of Stygian UI Library
// TODO: Implement when macOS support is needed

#if defined(__APPLE__)

#include "stygian_cocoa_shim.h"

bool stygian_cocoa_gl_set_pixel_format(void *win) {
  (void)win;
  return false;
}

void *stygian_cocoa_gl_create_context(void *win, void *share_ctx) {
  (void)win;
  (void)share_ctx;
  return NULL;
}

void stygian_cocoa_gl_destroy_context(void *ctx) { (void)ctx; }

bool stygian_cocoa_gl_make_current(void *win, void *ctx) {
  (void)win;
  (void)ctx;
  return false;
}

void stygian_cocoa_gl_swap_buffers(void *win) { (void)win; }

void stygian_cocoa_gl_set_vsync(void *win, bool enabled) {
  (void)win;
  (void)enabled;
}

void *stygian_cocoa_gl_get_proc_address(const char *name) {
  (void)name;
  return NULL;
}

uint32_t stygian_cocoa_vk_get_instance_extensions(const char **out_exts,
                                                  uint32_t max_exts) {
  (void)out_exts;
  (void)max_exts;
  return 0;
}

bool stygian_cocoa_vk_create_surface(void *win, void *vk_instance,
                                     void **out_surface) {
  (void)win;
  (void)vk_instance;
  (void)out_surface;
  return false;
}

#endif // __APPLE__
