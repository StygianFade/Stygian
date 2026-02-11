// stygian_window.h - Platform-Agnostic Window API
// Part of Stygian UI Library
#ifndef STYGIAN_WINDOW_H
#define STYGIAN_WINDOW_H

#include "stygian_input.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Window Handle
// ============================================================================

typedef struct StygianWindow StygianWindow;

// ============================================================================
// Window Configuration
// ============================================================================

typedef enum StygianWindowFlags {
  STYGIAN_WINDOW_NONE = 0,
  STYGIAN_WINDOW_RESIZABLE = (1 << 0),
  STYGIAN_WINDOW_BORDERLESS = (1 << 1),
  STYGIAN_WINDOW_TRANSPARENT = (1 << 2), // Per-pixel alpha
  STYGIAN_WINDOW_ALWAYS_ON_TOP = (1 << 3),
  STYGIAN_WINDOW_MAXIMIZED = (1 << 4),
  STYGIAN_WINDOW_CENTERED = (1 << 5),
  STYGIAN_WINDOW_OPENGL = (1 << 6), // Create GL context
  STYGIAN_WINDOW_VULKAN = (1 << 7), // Prepare for Vulkan
} StygianWindowFlags;

typedef enum StygianWindowRole {
  STYGIAN_ROLE_MAIN = 0,    // Standard app window
  STYGIAN_ROLE_TOOL = 1,    // Floating tool palette (no taskbar)
  STYGIAN_ROLE_POPUP = 2,   // Transient popup (menu, combo)
  STYGIAN_ROLE_TOOLTIP = 3, // Tooltip / Overlay (no focus)
} StygianWindowRole;

typedef struct StygianWindowConfig {
  int width, height;
  const char *title;
  uint32_t flags;         // StygianWindowFlags
  StygianWindowRole role; // Semantic role
  int gl_major, gl_minor; // OpenGL version (if OPENGL flag)
} StygianWindowConfig;

// ============================================================================
// Window Lifecycle
// ============================================================================

// Create window with config
StygianWindow *stygian_window_create(const StygianWindowConfig *config);

// Create window with defaults (800x600, resizable, OpenGL 4.3)
StygianWindow *stygian_window_create_simple(int w, int h, const char *title);

// Wrap an existing native window handle (HWND, NSWindow*, etc.)
// Use this when the application manages its own window but needs Stygian
// integration The wrapped window is NOT destroyed by stygian_window_destroy()
StygianWindow *stygian_window_from_native(void *native_handle);

// Pixel density (DPI scale)
float stygian_window_get_scale(StygianWindow *win);

// Convert screen coordinates to client coordinates
void stygian_window_screen_to_client(StygianWindow *win, int screen_x,
                                     int screen_y, int *client_x,
                                     int *client_y);

// Destroy window and free resources
// Note: Windows created via stygian_window_from_native() are NOT destroyed
void stygian_window_destroy(StygianWindow *win);

// ============================================================================
// Window State
// ============================================================================

bool stygian_window_should_close(StygianWindow *win);
void stygian_window_request_close(StygianWindow *win);

void stygian_window_get_size(StygianWindow *win, int *w, int *h);
void stygian_window_set_size(StygianWindow *win, int w, int h);

void stygian_window_get_position(StygianWindow *win, int *x, int *y);
void stygian_window_set_position(StygianWindow *win, int x, int y);

void stygian_window_set_title(StygianWindow *win, const char *title);

void stygian_window_minimize(StygianWindow *win);
void stygian_window_maximize(StygianWindow *win);
void stygian_window_restore(StygianWindow *win);
bool stygian_window_is_maximized(StygianWindow *win);
bool stygian_window_is_minimized(StygianWindow *win);

void stygian_window_focus(StygianWindow *win);
bool stygian_window_is_focused(StygianWindow *win);

// ============================================================================
// Event Processing
// ============================================================================

// Poll events (non-blocking)
// Returns true if event was retrieved
bool stygian_window_poll_event(StygianWindow *win, StygianEvent *event);

// Wait for event (blocking)
void stygian_window_wait_event(StygianWindow *win, StygianEvent *event);

// Wait for event up to timeout (milliseconds). Returns true if an event
// was received, false on timeout.
bool stygian_window_wait_event_timeout(StygianWindow *win, StygianEvent *event,
                                       uint32_t timeout_ms);

// Process all pending events (call callbacks)
void stygian_window_process_events(StygianWindow *win);

// ============================================================================
// OpenGL Context
// ============================================================================

void stygian_window_make_current(StygianWindow *win);
void stygian_window_swap_buffers(StygianWindow *win);
void stygian_window_set_vsync(StygianWindow *win, bool enabled);
void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx);
void stygian_window_gl_destroy_context(void *ctx);
bool stygian_window_gl_make_current(StygianWindow *win, void *ctx);
void stygian_window_gl_swap_buffers(StygianWindow *win);
void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled);
bool stygian_window_gl_set_pixel_format(StygianWindow *win);
void *stygian_window_gl_get_proc_address(const char *name);

// ============================================================================
// Vulkan Surface
// ============================================================================

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts);
bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **vk_surface);

// ============================================================================
// Native Handle (Platform-Specific)
// ============================================================================

// Returns platform-specific handle:
// - Windows: HWND
// - Linux/X11: Window (X11 window ID)
// - macOS: NSWindow*
void *stygian_window_native_handle(StygianWindow *win);

// Returns platform-specific graphics context:
// - Windows: HDC (for OpenGL) or HWND (for Vulkan)
// - Linux/X11: Display* (for OpenGL/Vulkan)
// - macOS: CAMetalLayer* (for Metal/Vulkan)
void *stygian_window_native_context(StygianWindow *win);

// ============================================================================
// Cursor
// ============================================================================

typedef enum StygianCursor {
  STYGIAN_CURSOR_ARROW = 0,
  STYGIAN_CURSOR_IBEAM,
  STYGIAN_CURSOR_CROSSHAIR,
  STYGIAN_CURSOR_HAND,
  STYGIAN_CURSOR_RESIZE_H,
  STYGIAN_CURSOR_RESIZE_V,
  STYGIAN_CURSOR_RESIZE_NWSE,
  STYGIAN_CURSOR_RESIZE_NESW,
  STYGIAN_CURSOR_RESIZE_ALL,
  STYGIAN_CURSOR_NOT_ALLOWED,
} StygianCursor;

void stygian_window_set_cursor(StygianWindow *win, StygianCursor cursor);
void stygian_window_hide_cursor(StygianWindow *win);
void stygian_window_show_cursor(StygianWindow *win);

// ============================================================================
// High-DPI Support
// ============================================================================

float stygian_window_get_dpi_scale(StygianWindow *win);
void stygian_window_get_framebuffer_size(StygianWindow *win, int *w, int *h);

// Clipboard API
void stygian_clipboard_write(StygianWindow *win, const char *text);
char *stygian_clipboard_read(
    StygianWindow *win); // Returns heap-allocated string (caller must free)

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_WINDOW_H
