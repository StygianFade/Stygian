// stygian_tabs_internal.h - Internal definitions for Stygian Tabs
// Not for public consumption. Changes here do not affect public ABI.

#ifndef STYGIAN_TABS_INTERNAL_H
#define STYGIAN_TABS_INTERNAL_H

#include "stygian_tabs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Internal Struct Definitions
// ============================================================================

struct StygianTabItem {
  char title[64];
  bool closable;
  bool pinned; // Pinned tabs can't be closed or reordered
  void *user_data;

  // Internal state (managed by tab bar)
  float x, y, w, h;  // Current position/size (for animation)
  float target_x;    // Target position (for smooth animation)
  int visual_index;  // Visual position (during drag)
  int logical_index; // Logical position in array
};

struct StygianTabBar {
  float x, y, w, h;

  struct StygianTabItem tabs[STYGIAN_MAX_TABS];
  int tab_count;
  int active_tab; // Logical index of active tab

  // Drag state
  int dragging_tab;      // Index of tab being dragged (-1 if none)
  float drag_offset_x;   // Mouse offset from tab origin
  int drag_target_index; // Target index for drop

  // Visual state
  float tab_width;     // Current tab width (dynamic based on count)
  float min_tab_width; // Minimum tab width
  float max_tab_width; // Maximum tab width

  // Interaction state
  int hot_tab;          // Tab under mouse
  int hot_close_button; // Close button under mouse
};

// ============================================================================
// MultiViewport Internal
// ============================================================================

struct StygianViewport {
  char name[32];
  StygianViewportType type;
  float x, y, w, h;
  uint32_t framebuffer_texture; // External texture from rendering backend
  bool active;
  bool show_grid;
  bool show_gizmo;
  void *user_data;
};

struct StygianMultiViewport {
  struct StygianViewport viewports[STYGIAN_MAX_VIEWPORTS];
  int viewport_count;
  int active_viewport;

  // Layout mode
  enum {
    STYGIAN_VIEWPORT_LAYOUT_SINGLE,
    STYGIAN_VIEWPORT_LAYOUT_SPLIT_H,
    STYGIAN_VIEWPORT_LAYOUT_SPLIT_V,
    STYGIAN_VIEWPORT_LAYOUT_QUAD,
    STYGIAN_VIEWPORT_LAYOUT_CUSTOM
  } layout_mode;

  // Split ratios (for split modes)
  float split_ratio_h;
  float split_ratio_v;
};

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_TABS_INTERNAL_H
