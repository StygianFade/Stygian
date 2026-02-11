// stygian_cocoa_shim.h - macOS Cocoa shim (C ABI)
#pragma once

#if defined(__APPLE__)

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool stygian_cocoa_gl_set_pixel_format(void *win);
void *stygian_cocoa_gl_create_context(void *win, void *share_ctx);
void stygian_cocoa_gl_destroy_context(void *ctx);
bool stygian_cocoa_gl_make_current(void *win, void *ctx);
void stygian_cocoa_gl_swap_buffers(void *win);
void stygian_cocoa_gl_set_vsync(void *win, bool enabled);
void *stygian_cocoa_gl_get_proc_address(const char *name);

uint32_t stygian_cocoa_vk_get_instance_extensions(const char **out_exts,
                                                  uint32_t max_exts);
bool stygian_cocoa_vk_create_surface(void *win, void *vk_instance,
                                     void **out_surface);

#ifdef __cplusplus
}
#endif

#endif // __APPLE__
