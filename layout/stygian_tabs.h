// stygian_tabs.h - Production Tab System with Chrome-like behavior
// Part of Layout System

#ifndef STYGIAN_TABS_H
#define STYGIAN_TABS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct StygianContext StygianContext;
typedef uint32_t StygianFont; // Use definition from stygian.h

// ============================================================================
// Tab System - Chrome-like draggable, reorderable, closable tabs
// ============================================================================

// ============================================================================
// Tab System - Structs
// ============================================================================

#define STYGIAN_MAX_TABS 64

typedef struct StygianTabItem StygianTabItem; // Opaque
typedef struct StygianTabBar StygianTabBar;   // Opaque

// ============================================================================
// Tab System - API
// ============================================================================

// Create a new tab bar
StygianTabBar *stygian_tab_bar_create(float x, float y, float w, float h);

// Destroy tab bar
void stygian_tab_bar_destroy(StygianTabBar *bar);

// Set layout bounds
void stygian_tab_bar_set_layout(StygianTabBar *bar, float x, float y, float w,
                                float h);

// Set layout bounds
void stygian_tab_bar_set_layout(StygianTabBar *bar, float x, float y, float w,
                                float h);

// Add tab (returns index, or -1 if full)
int stygian_tab_bar_add(StygianTabBar *bar, const char *title, bool closable);

// Remove tab by index
void stygian_tab_bar_remove(StygianTabBar *bar, int index);

// Render and handle interaction
// Returns: 0 = no action, 1 = tab switched, 2 = tab closed, 3 = tab reordered
int stygian_tab_bar_update(StygianContext *ctx, StygianFont font,
                           StygianTabBar *bar);

// Get active tab user data
void *stygian_tab_bar_get_active_data(StygianTabBar *bar);

// Get active tab index
int stygian_tab_bar_get_active_index(StygianTabBar *bar);

// Get tab count
int stygian_tab_bar_get_count(StygianTabBar *bar);

// Get tab title
const char *stygian_tab_bar_get_title(StygianTabBar *bar, int index);

// ============================================================================
// Multiviewport System
// ============================================================================

typedef enum StygianViewportType {
  STYGIAN_VIEWPORT_3D,
  STYGIAN_VIEWPORT_2D,
  STYGIAN_VIEWPORT_TEXT,
  STYGIAN_VIEWPORT_CUSTOM
} StygianViewportType;

#define STYGIAN_MAX_VIEWPORTS 16

typedef struct StygianViewport StygianViewport;           // Opaque
typedef struct StygianMultiViewport StygianMultiViewport; // Opaque

// Create multiviewport system
StygianMultiViewport *stygian_multiviewport_create(void);

// Destroy multiviewport system
void stygian_multiviewport_destroy(StygianMultiViewport *mv);

// Add viewport (returns index, or -1 if full)
int stygian_multiviewport_add(StygianMultiViewport *mv, const char *name,
                              StygianViewportType type);

// Set layout mode
void stygian_multiviewport_set_layout(StygianMultiViewport *mv,
                                      int layout_mode);

// Render viewports
void stygian_multiviewport_render(StygianContext *ctx, StygianFont font,
                                  StygianMultiViewport *mv);

// Get viewport under mouse (returns index or -1)
int stygian_multiviewport_hit_test(StygianMultiViewport *mv, int mouse_x,
                                   int mouse_y);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_TABS_H
