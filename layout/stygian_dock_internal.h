// stygian_dock_internal.h - Internal definitions for Stygian Docking
// Not for public consumption. Changes here do not affect public ABI.

#ifndef STYGIAN_DOCK_INTERNAL_H
#define STYGIAN_DOCK_INTERNAL_H

#include "stygian_dock.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Internal Struct Definitions
// ============================================================================

struct StygianDockPanel {
  char title[64];
  uint32_t id;
  void *user_data;

  bool closable;
  bool visible;
  bool dirty; // Needs re-render

  // DDI render callback
  StygianDockPanelRenderFn render;
};

struct StygianDockNode {
  uint32_t id;
  StygianDockSplit split_type;
  float split_ratio; // 0.0-1.0 for split nodes

  // Tree structure
  struct StygianDockNode *child_a; // Left or Top
  struct StygianDockNode *child_b; // Right or Bottom
  struct StygianDockNode *parent;

  // Tab container data (for leaf nodes)
  uint32_t panel_ids[STYGIAN_DOCK_MAX_TABS_PER_NODE];
  int panel_count;
  int active_panel; // Index into panel_ids

  // Computed layout (set during update)
  float x, y, w, h;

  // Interaction state
  bool splitter_hovered;
  bool splitter_dragging;
  StygianDockDropZone hot_zone;
};

struct StygianFloatingWindow {
  struct StygianWindow *window; // Stygian window (full object for lifecycle)
  struct StygianAPSurface *surface; // Render surface (via stygian_ap)
  void *native_handle;              // Cached native handle (HWND on Windows)

  StygianDockNode *root_node; // Root node of floating content

  float x, y, w, h; // Window position/size
  bool dragging;
  bool resizing;
  bool visible;
  bool prev_mouse_down; // Track mouse state per window

  // Metaball melting state
  float undock_progress; // 0.0 = attached, 1.0 = fully separated
  float blend_radius;    // Current metaball blend (for SDF)
  bool melting;          // In undock/redock transition
};

struct StygianDockSpace {
  // Panel registry
  StygianDockPanel panels[STYGIAN_DOCK_MAX_PANELS];
  int panel_count;
  uint32_t next_panel_id;

  // Node pool
  StygianDockNode nodes[STYGIAN_DOCK_MAX_NODES];
  int node_count;
  uint32_t next_node_id;

  // Root node of docked layout
  StygianDockNode *root;

  // Floating windows
  StygianFloatingWindow floating[STYGIAN_DOCK_MAX_FLOATING];
  int floating_count;

  // Main window context (for sharing with floating)
  void *main_gl_context;
  void *main_device_context;

  // Drag state
  uint32_t dragging_panel_id; // Panel being dragged (0 = none)
  float drag_start_x, drag_start_y;
  float drag_offset_x, drag_offset_y;
  StygianDockNode *drop_target;
  StygianDockDropZone drop_zone;
  bool drag_started;    // Passed drag threshold
  bool prev_mouse_down; // Previous frame mouse state (no static!)

  // Style
  float tab_height;
  float splitter_size;
  float min_panel_size;
  float drop_zone_size; // Size of edge drop zones

  // Metaball effect
  bool metaball_enabled;
  float metaball_blend_speed; // Animation speed

  // ===== O(1) OPTIMIZATIONS =====

  // Dirty flag - skip layout when tree unchanged
  bool layout_dirty;
  float last_layout_w, last_layout_h; // Detect resize

// Spatial hash for O(1) drop target lookup
// Grid of 64x64 pixel cells mapping to leaf nodes
#define STYGIAN_SPATIAL_GRID_CELL 64
#define STYGIAN_SPATIAL_GRID_COLS 32 // 2048 / 64
#define STYGIAN_SPATIAL_GRID_ROWS 16 // 1024 / 64
  StygianDockNode
      *spatial_grid[STYGIAN_SPATIAL_GRID_COLS * STYGIAN_SPATIAL_GRID_ROWS];
  bool spatial_dirty; // Rebuild grid when layout changes

  // Focus tracking
  StygianDockNode *focused_node; // Currently focused node (receives input)

  // ===== DOUBLE-CLICK DETECTION =====
  uint64_t last_click_time_ms; // For double-click detection
  float last_click_x, last_click_y;

  // ===== TAB REORDERING =====
  int reorder_src_idx; // Source tab index during reorder (-1 = not reordering)
  int reorder_dst_idx; // Destination tab index
  StygianDockNode *reorder_node; // Node being reordered

  // ===== GHOST WINDOW (external drag preview) =====
  void *ghost_hwnd;     // WS_POPUP window handle (platform-specific)
  bool ghost_visible;   // Ghost is currently showing
  int ghost_w, ghost_h; // Ghost dimensions

  // ===== FLOATING WINDOW DRAG =====
  bool dragging_from_floating; // Dragging started from a floating window
  int dragging_floating_idx;   // Index of source floating window
};

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_DOCK_INTERNAL_H
