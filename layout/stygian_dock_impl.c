// stygian_dock.c - Modern Docking System Implementation
// Part of Layout System - Multiviewport, Tabbed Containers, Floating Windows

#include "../backends/stygian_ap.h"
#include "../include/stygian.h"
#include "../src/stygian_internal.h" // For ctx->elements access
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include "stygian_dock.h"
#include "stygian_dock_internal.h"
#include "stygian_tabs.h"

#include <stdio.h>
#include <string.h>

// Platform time helper for double-click detection
#ifdef _WIN32
#include <windows.h>
static uint64_t get_time_ms(void) { return GetTickCount64(); }
#else
#include <time.h>
static uint64_t get_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

// Double-click timing (ms)
#define DOUBLE_CLICK_TIME_MS 400
#define DOUBLE_CLICK_DISTANCE 5.0f

// Ghost window dimensions
#define GHOST_WIDTH 300
#define GHOST_HEIGHT 200

// ============================================================================
// Ghost Window (WS_POPUP for external drag preview)
// ============================================================================

#ifdef _WIN32
static void ghost_create(StygianDockSpace *dock) {
  if (dock->ghost_hwnd)
    return;

  // Register window class (once)
  static bool class_registered = false;
  if (!class_registered) {
    WNDCLASSEXA wc = {
        .cbSize = sizeof(WNDCLASSEXA),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = DefWindowProcA,
        .hInstance = GetModuleHandle(NULL),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = "StygianGhostClass",
    };
    RegisterClassExA(&wc);
    class_registered = true;
  }

  // Create layered popup window
  // WS_EX_TRANSPARENT: Click-through and DWM optimization
  dock->ghost_hwnd = CreateWindowExA(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
      "StygianGhostClass", NULL, WS_POPUP, 0, 0, GHOST_WIDTH, GHOST_HEIGHT,
      NULL, NULL, GetModuleHandle(NULL), NULL);

  dock->ghost_w = GHOST_WIDTH;
  dock->ghost_h = GHOST_HEIGHT;

  // Set translucent (50% opacity)
  SetLayeredWindowAttributes((HWND)dock->ghost_hwnd, 0, 128, LWA_ALPHA);
}

static void ghost_destroy(StygianDockSpace *dock) {
  if (dock->ghost_hwnd) {
    DestroyWindow((HWND)dock->ghost_hwnd);
    dock->ghost_hwnd = NULL;
  }
  dock->ghost_visible = false;
}

static void ghost_show(StygianDockSpace *dock, int screen_x, int screen_y) {
  if (!dock->ghost_hwnd)
    ghost_create(dock);

  // Center ghost on cursor
  int x = screen_x - dock->ghost_w / 2;
  int y = screen_y - dock->ghost_h / 2;

  SetWindowPos((HWND)dock->ghost_hwnd, HWND_TOPMOST, x, y, 0, 0,
               SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  dock->ghost_visible = true;
}

static void ghost_hide(StygianDockSpace *dock) {
  if (dock->ghost_hwnd && dock->ghost_visible) {
    ShowWindow((HWND)dock->ghost_hwnd, SW_HIDE);
    dock->ghost_visible = false;
  }
}

static void ghost_move(StygianDockSpace *dock, int screen_x, int screen_y) {
  if (dock->ghost_hwnd && dock->ghost_visible) {
    int x = screen_x - dock->ghost_w / 2;
    int y = screen_y - dock->ghost_h / 2;

    // Optimization: Only update if changed
    // (Assuming dock stores last ghost pos, or just checking vs window rect)
    RECT rc;
    GetWindowRect((HWND)dock->ghost_hwnd, &rc);
    if (rc.left != x || rc.top != y) {
      SetWindowPos((HWND)dock->ghost_hwnd, HWND_TOPMOST, x, y, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
}
#else
// Stubs for non-Windows platforms
static void ghost_create(StygianDockSpace *dock) { (void)dock; }
static void ghost_destroy(StygianDockSpace *dock) { (void)dock; }
static void ghost_show(StygianDockSpace *dock, int x, int y) {
  (void)dock;
  (void)x;
  (void)y;
}
static void ghost_hide(StygianDockSpace *dock) { (void)dock; }
static void ghost_move(StygianDockSpace *dock, int x, int y) {
  (void)dock;
  (void)x;
  (void)y;
}
#endif

// ============================================================================
// Internal Helpers
// ============================================================================

static StygianDockNode *alloc_node(StygianDockSpace *dock) {
  if (dock->node_count >= STYGIAN_DOCK_MAX_NODES) {
    return NULL;
  }
  StygianDockNode *node = &dock->nodes[dock->node_count++];
  memset(node, 0, sizeof(StygianDockNode));
  node->id = ++dock->next_node_id;
  node->split_ratio = 0.5f;
  return node;
}

static StygianDockPanel *find_panel(StygianDockSpace *dock, uint32_t panel_id) {
  for (int i = 0; i < dock->panel_count; i++) {
    if (dock->panels[i].id == panel_id) {
      return &dock->panels[i];
    }
  }
  return NULL;
}

// NOTE: Docked panels use pure DDI - render directly to backbuffer.
// NO FBO allocation for docked panels (memory optimization).
// FBOs are ONLY used for floating windows (separate OS window surface).

// ============================================================================
// API - Initialization
// ============================================================================

StygianDockSpace *stygian_dock_create(void *main_gl_context,
                                      void *main_device_context) {
  StygianDockSpace *dock =
      (StygianDockSpace *)calloc(1, sizeof(StygianDockSpace));
  if (!dock)
    return NULL;

  dock->main_gl_context = main_gl_context;
  dock->main_device_context = main_device_context;

  // Defaults
  dock->tab_height = 28.0f;
  dock->splitter_size = 4.0f;
  dock->min_panel_size = 50.0f;
  dock->drop_zone_size = 40.0f;

  dock->metaball_enabled = true;
  dock->last_click_y = 0.0f;

  // Tab reorder
  dock->reorder_src_idx = -1;
  dock->reorder_dst_idx = -1;
  dock->reorder_node = NULL;

  printf("[Stygian Dock] Initialized\n");
  return dock;
}

void stygian_dock_shutdown(StygianDockSpace *dock) {
  // Delete all panel FBOs
  for (int i = 0; i < dock->panel_count; i++) {
    // StygianDockPanel *panel = &dock->panels[i];
    // No FBOs to delete (DDI mode)
  }

  // Close floating windows
  for (int i = 0; i < dock->floating_count; i++) {
    StygianFloatingWindow *fw = &dock->floating[i];
    if (fw->native_handle) {
      // TODO: DestroyWindow, wglDeleteContext
    }
  }

  // Destroy ghost window
  ghost_destroy(dock);

  memset(dock, 0, sizeof(StygianDockSpace));
  printf("[Stygian Dock] Shutdown\n");
}

void stygian_dock_destroy(StygianDockSpace *dock) {
  if (!dock)
    return;

  stygian_dock_shutdown(dock);
  free(dock);
}

// ============================================================================
// Panel Management
// ============================================================================

uint32_t stygian_dock_register_panel(StygianDockSpace *dock, const char *title,
                                     bool closable,
                                     StygianDockPanelRenderFn render,
                                     void *user_data) {
  if (dock->panel_count >= STYGIAN_DOCK_MAX_PANELS) {
    printf("[Stygian Dock] Error: Max panels reached\n");
    return 0;
  }

  StygianDockPanel *panel = &dock->panels[dock->panel_count++];
  memset(panel, 0, sizeof(StygianDockPanel));

  panel->id = ++dock->next_panel_id;
  strncpy(panel->title, title, sizeof(panel->title) - 1);
  panel->closable = closable;
  panel->visible = true;
  panel->dirty = true;
  panel->render = render;
  panel->user_data = user_data;

  printf("[Stygian Dock] Registered panel: %s (id=%u)\n", title, panel->id);
  return panel->id;
}

void stygian_dock_unregister_panel(StygianDockSpace *dock, uint32_t panel_id) {
  for (int i = 0; i < dock->panel_count; i++) {
    if (dock->panels[i].id == panel_id) {
      // Delete FBO
      // Delete FBO
      // if (dock->panels[i].fbo != 0) { ... }

      // Shift remaining panels
      for (int j = i; j < dock->panel_count - 1; j++) {
        dock->panels[j] = dock->panels[j + 1];
      }
      dock->panel_count--;

      // TODO: Remove from any nodes that reference this panel
      printf("[Stygian Dock] Unregistered panel id=%u\n", panel_id);
      return;
    }
  }
}

void stygian_dock_mark_dirty(StygianDockSpace *dock, uint32_t panel_id) {
  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (panel) {
    panel->dirty = true;
  }
}

StygianDockPanel *stygian_dock_get_panel(StygianDockSpace *dock,
                                         uint32_t panel_id) {
  return find_panel(dock, panel_id);
}

// ============================================================================
// Layout Building
// ============================================================================

StygianDockNode *stygian_dock_get_root(StygianDockSpace *dock) {
  if (!dock->root) {
    dock->root = alloc_node(dock);
    if (dock->root) {
      dock->root->split_type = STYGIAN_DOCK_SPLIT_NONE;
      printf("[Stygian Dock] Created root node (id=%u)\n", dock->root->id);
    }
  }
  return dock->root;
}

void stygian_dock_add_panel_to_node(StygianDockSpace *dock,
                                    StygianDockNode *node, uint32_t panel_id) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE) {
    printf("[Stygian Dock] Error: Can only add panels to leaf nodes\n");
    return;
  }

  if (node->panel_count >= STYGIAN_DOCK_MAX_TABS_PER_NODE) {
    printf("[Stygian Dock] Error: Max tabs in node reached\n");
    return;
  }

  node->panel_ids[node->panel_count++] = panel_id;
  node->active_panel = node->panel_count - 1;

  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (panel) {
    printf("[Stygian Dock] Added panel '%s' to node %u\n", panel->title,
           node->id);
  }
}

void stygian_dock_split(StygianDockSpace *dock, StygianDockNode *node,
                        StygianDockSplit direction, float ratio,
                        StygianDockNode **out_a, StygianDockNode **out_b) {
  if (!node)
    return;

  // Clamp ratio
  if (ratio < 0.1f)
    ratio = 0.1f;
  if (ratio > 0.9f)
    ratio = 0.9f;

  // Create two child nodes
  StygianDockNode *child_a = alloc_node(dock);
  StygianDockNode *child_b = alloc_node(dock);

  if (!child_a || !child_b) {
    printf("[Stygian Dock] Error: Failed to allocate child nodes\n");
    return;
  }

  // Move existing panels to child_a
  child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;
  memcpy(child_a->panel_ids, node->panel_ids, sizeof(node->panel_ids));
  child_a->panel_count = node->panel_count;
  child_a->active_panel = node->active_panel;
  child_a->parent = node;

  // child_b starts empty (leaf)
  child_b->split_type = STYGIAN_DOCK_SPLIT_NONE;
  child_b->parent = node;

  // Convert node to split node
  node->split_type = direction;
  node->split_ratio = ratio;
  node->child_a = child_a;
  node->child_b = child_b;
  node->panel_count = 0;
  memset(node->panel_ids, 0, sizeof(node->panel_ids));

  printf("[Stygian Dock] Split node %u (%s), ratio=%.2f\n", node->id,
         direction == STYGIAN_DOCK_SPLIT_HORIZONTAL ? "H" : "V", ratio);

  // Mark layout dirty - tree structure changed
  dock->layout_dirty = true;

  if (out_a)
    *out_a = child_a;
  if (out_b)
    *out_b = child_b;
}

// ============================================================================
// Layout Calculation (recursive)
// ============================================================================
// OPTIMIZATION IMPLEMENTED: Dirty flag skips recalc when tree unchanged.
// Set layout_dirty=true in: split(), execute_drop(), collapse_node().

static void calculate_layout_recursive(StygianDockSpace *dock,
                                       StygianDockNode *node, float x, float y,
                                       float w, float h) {
  if (!node)
    return;

  node->x = x;
  node->y = y;
  node->w = w;
  node->h = h;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    // Leaf node - pure DDI, no FBO needed
    // Panel content is rendered directly in render_node_recursive
  } else if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
    // Top/Bottom split
    float split_y = y + h * node->split_ratio;
    float splitter = dock->splitter_size;

    calculate_layout_recursive(dock, node->child_a, x, y, w,
                               split_y - y - splitter / 2);
    calculate_layout_recursive(dock, node->child_b, x, split_y + splitter / 2,
                               w, y + h - split_y - splitter / 2);
  } else if (node->split_type == STYGIAN_DOCK_SPLIT_VERTICAL) {
    // Left/Right split
    float split_x = x + w * node->split_ratio;
    float splitter = dock->splitter_size;

    calculate_layout_recursive(dock, node->child_a, x, y,
                               split_x - x - splitter / 2, h);
    calculate_layout_recursive(dock, node->child_b, split_x + splitter / 2, y,
                               x + w - split_x - splitter / 2, h);
  }
}

// ============================================================================
// Rendering
// ============================================================================

static void render_node_recursive(StygianContext *ctx, StygianFont font,
                                  StygianDockSpace *dock,
                                  StygianDockNode *node) {
  if (!node)
    return;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    // Skip empty leaf nodes entirely (they should be pruned)
    if (node->panel_count == 0) {
      return; // Empty node - don't render
    }

    // Render tab bar background
    stygian_rect(ctx, node->x, node->y, node->w, dock->tab_height, 0.12f, 0.12f,
                 0.12f, 1.0f);

    // Render tabs
    float tab_x = node->x + 2.0f;
    float tab_w = 120.0f;
    if (node->panel_count > 0) {
      tab_w = (node->w - 4.0f) / node->panel_count;
      if (tab_w > 150.0f)
        tab_w = 150.0f;
      if (tab_w < 60.0f)
        tab_w = 60.0f;
    }

    for (int i = 0; i < node->panel_count; i++) {
      StygianDockPanel *panel = find_panel(dock, node->panel_ids[i]);
      if (!panel)
        continue;

      bool active = (i == node->active_panel);

      // Tab background
      float r = active ? 0.2f : 0.15f;
      float g = active ? 0.25f : 0.15f;
      float b = active ? 0.35f : 0.15f;

      stygian_rect_rounded(ctx, tab_x, node->y + 2, tab_w - 4,
                           dock->tab_height - 4, r, g, b, 1.0f, 4.0f);

      // Tab title
      if (font) {
        stygian_text(ctx, font, panel->title, tab_x + 8, node->y + 6, 14.0f,
                     0.9f, 0.9f, 0.9f, 1.0f);
      }

      // Close button (right side of tab)
      if (panel->closable) {
        float close_size = 14.0f;
        float close_x = tab_x + tab_w - close_size - 8;
        float close_y = node->y + (dock->tab_height - close_size) / 2.0f;

        // Circle background (subtle)
        stygian_rect_rounded(ctx, close_x, close_y, close_size, close_size,
                             0.4f, 0.3f, 0.3f, 0.8f, close_size / 2);

        // X icon using proper SDF icon element (centered in bounds)
        StygianElement close_icon = stygian_element_transient(ctx);
        stygian_set_bounds(ctx, close_icon, close_x, close_y, close_size,
                           close_size);
        stygian_set_type(ctx, close_icon, STYGIAN_ICON_CLOSE);
        stygian_set_color(ctx, close_icon, 1.0f, 1.0f, 1.0f, 0.9f);
      }

      tab_x += tab_w;
    }

    // Render content area background
    float content_y = node->y + dock->tab_height;
    float content_h = node->h - dock->tab_height;
    stygian_rect(ctx, node->x, content_y, node->w, content_h, 0.08f, 0.08f,
                 0.08f, 1.0f);

    // Focus highlight (border around content when focused)
    if (dock->focused_node == node) {
      float border = 2.0f;
      // Top edge
      stygian_rect(ctx, node->x, content_y, node->w, border, 0.3f, 0.5f, 0.9f,
                   0.8f);
      // Bottom edge
      stygian_rect(ctx, node->x, content_y + content_h - border, node->w,
                   border, 0.3f, 0.5f, 0.9f, 0.8f);
      // Left edge
      stygian_rect(ctx, node->x, content_y, border, content_h, 0.3f, 0.5f, 0.9f,
                   0.8f);
      // Right edge
      stygian_rect(ctx, node->x + node->w - border, content_y, border,
                   content_h, 0.3f, 0.5f, 0.9f, 0.8f);
    }

    // Render active panel content (DDI callback)
    if (node->panel_count > 0 && node->active_panel >= 0 &&
        node->active_panel < node->panel_count) {
      StygianDockPanel *panel =
          find_panel(dock, node->panel_ids[node->active_panel]);
      if (panel && panel->render) {
        // TODO: Bind FBO, render, unbind
        // For now, render directly (no FBO)
        panel->render(panel, ctx, font, node->x, content_y, node->w, content_h);
        panel->dirty = false;
      }
    }

  } else {
    // Split node - render splitter
    float splitter = dock->splitter_size;
    float r = node->splitter_hovered ? 0.4f : 0.2f;

    if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
      float split_y = node->y + node->h * node->split_ratio;
      stygian_rect(ctx, node->x, split_y - splitter / 2, node->w, splitter, r,
                   r, r, 1.0f);
    } else {
      float split_x = node->x + node->w * node->split_ratio;
      stygian_rect(ctx, split_x - splitter / 2, node->y, splitter, node->h, r,
                   r, r, 1.0f);
    }

    // Render children
    render_node_recursive(ctx, font, dock, node->child_a);
    render_node_recursive(ctx, font, dock, node->child_b);
  }
}

// ============================================================================
// Spatial Hash Grid - O(1) Drop Target Lookup
// ============================================================================

// Populate a single leaf node into the spatial grid
static void populate_spatial_grid_leaf(StygianDockSpace *dock,
                                       StygianDockNode *node) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE)
    return;
  if (node->panel_count == 0)
    return; // Skip empty nodes

  int cell_size = STYGIAN_SPATIAL_GRID_CELL;
  int start_col = (int)(node->x) / cell_size;
  int end_col = (int)(node->x + node->w) / cell_size;
  int start_row = (int)(node->y) / cell_size;
  int end_row = (int)(node->y + node->h) / cell_size;

  // Clamp to grid bounds
  if (start_col < 0)
    start_col = 0;
  if (start_row < 0)
    start_row = 0;
  if (end_col >= STYGIAN_SPATIAL_GRID_COLS)
    end_col = STYGIAN_SPATIAL_GRID_COLS - 1;
  if (end_row >= STYGIAN_SPATIAL_GRID_ROWS)
    end_row = STYGIAN_SPATIAL_GRID_ROWS - 1;

  for (int row = start_row; row <= end_row; row++) {
    for (int col = start_col; col <= end_col; col++) {
      dock->spatial_grid[row * STYGIAN_SPATIAL_GRID_COLS + col] = node;
    }
  }
}

// Recursively populate spatial grid from all leaf nodes
static void populate_spatial_grid_recursive(StygianDockSpace *dock,
                                            StygianDockNode *node) {
  if (!node)
    return;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    populate_spatial_grid_leaf(dock, node);
  } else {
    populate_spatial_grid_recursive(dock, node->child_a);
    populate_spatial_grid_recursive(dock, node->child_b);
  }
}

// Rebuild the spatial hash grid (call when layout changes)
static void rebuild_spatial_hash(StygianDockSpace *dock) {
  memset(dock->spatial_grid, 0, sizeof(dock->spatial_grid));
  populate_spatial_grid_recursive(dock, dock->root);
  dock->spatial_dirty = false;
}

// O(1) drop target lookup using spatial hash
static StygianDockNode *find_drop_target_fast(StygianDockSpace *dock, float mx,
                                              float my) {
  int col = (int)(mx) / STYGIAN_SPATIAL_GRID_CELL;
  int row = (int)(my) / STYGIAN_SPATIAL_GRID_CELL;

  // Bounds check
  if (col < 0 || col >= STYGIAN_SPATIAL_GRID_COLS || row < 0 ||
      row >= STYGIAN_SPATIAL_GRID_ROWS) {
    return NULL;
  }

  return dock->spatial_grid[row * STYGIAN_SPATIAL_GRID_COLS + col];
}

// ============================================================================
// Drop Zone Detection - Precise Proportional Zones
// ============================================================================

// Drop zone is 25% of panel dimension, with CENTER in the middle 50%
// Visual and detection MUST match exactly

static StygianDockDropZone detect_drop_zone(StygianDockSpace *dock,
                                            StygianDockNode *node, float mx,
                                            float my) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE)
    return STYGIAN_DROP_NONE;

  // Full node bounds (including tab bar)
  float nx = node->x;
  float ny = node->y;
  float nw = node->w;
  float nh = node->h;

  // Check if mouse is within node bounds at all
  if (mx < nx || mx > nx + nw || my < ny || my > ny + nh) {
    return STYGIAN_DROP_NONE;
  }

  // Tab bar area = CENTER drop (add as tab)
  if (my < ny + dock->tab_height) {
    return STYGIAN_DROP_CENTER;
  }

  // Content area (below tab bar)
  float content_y = ny + dock->tab_height;
  float content_h = nh - dock->tab_height;

  // Proportional zones: 25% edges, 50% center
  float edge_ratio = 0.25f;
  float left_edge = nx + nw * edge_ratio;
  float right_edge = nx + nw * (1.0f - edge_ratio);
  float top_edge = content_y + content_h * edge_ratio;
  float bottom_edge = content_y + content_h * (1.0f - edge_ratio);

  // Determine zone based on position
  // Priority: strongest axis wins
  float dx_left = mx - nx;
  float dx_right = (nx + nw) - mx;
  float dy_top = my - content_y;
  float dy_bottom = (content_y + content_h) - my;

  // Find minimum distance to edge
  float min_x = dx_left < dx_right ? dx_left : dx_right;
  float min_y = dy_top < dy_bottom ? dy_top : dy_bottom;

  // If in center region, return CENTER
  if (mx >= left_edge && mx <= right_edge && my >= top_edge &&
      my <= bottom_edge) {
    return STYGIAN_DROP_CENTER;
  }

  // Otherwise, determine edge zone by closest edge
  if (min_x < min_y) {
    // Horizontal edge is closer
    return dx_left < dx_right ? STYGIAN_DROP_LEFT : STYGIAN_DROP_RIGHT;
  } else {
    // Vertical edge is closer
    return dy_top < dy_bottom ? STYGIAN_DROP_TOP : STYGIAN_DROP_BOTTOM;
  }
}

// NOTE: find_drop_target_recursive() removed - replaced by O(1) spatial hash

// ============================================================================
// Drop Zone Rendering - Matches Detection EXACTLY
// ============================================================================

static void render_drop_zone_overlay(StygianContext *ctx,
                                     StygianDockSpace *dock,
                                     StygianDockNode *node,
                                     StygianDockDropZone zone) {
  if (!node || zone == STYGIAN_DROP_NONE)
    return;

  float nx = node->x;
  float ny = node->y;
  float nw = node->w;
  float nh = node->h;
  float content_y = ny + dock->tab_height;
  float content_h = nh - dock->tab_height;

  // Highlight color (semi-transparent blue)
  float r = 0.2f, g = 0.5f, b = 0.9f, a = 0.35f;

  // Proportional zones: 25% edges
  float edge_ratio = 0.25f;

  switch (zone) {
  case STYGIAN_DROP_LEFT:
    stygian_rect(ctx, nx, content_y, nw * edge_ratio, content_h, r, g, b, a);
    break;
  case STYGIAN_DROP_RIGHT:
    stygian_rect(ctx, nx + nw * (1.0f - edge_ratio), content_y, nw * edge_ratio,
                 content_h, r, g, b, a);
    break;
  case STYGIAN_DROP_TOP:
    stygian_rect(ctx, nx, content_y, nw, content_h * edge_ratio, r, g, b, a);
    break;
  case STYGIAN_DROP_BOTTOM:
    stygian_rect(ctx, nx, content_y + content_h * (1.0f - edge_ratio), nw,
                 content_h * edge_ratio, r, g, b, a);
    break;
  case STYGIAN_DROP_CENTER:
    // Highlight center region AND tab bar
    stygian_rect(ctx, nx, ny, nw, dock->tab_height, r, g, b, a);
    stygian_rect(ctx, nx + nw * edge_ratio, content_y + content_h * edge_ratio,
                 nw * (1.0f - 2.0f * edge_ratio),
                 content_h * (1.0f - 2.0f * edge_ratio), r, g, b, a);
    break;
  default:
    break;
  }
}

// ============================================================================
// Execute Drop
// ============================================================================

static void execute_drop(StygianDockSpace *dock, uint32_t panel_id,
                         StygianDockNode *target, StygianDockDropZone zone) {
  if (!target || zone == STYGIAN_DROP_NONE)
    return;

  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (!panel)
    return;

  printf("[Stygian Dock] Drop panel '%s' into node %u, zone=%d\n", panel->title,
         target->id, zone);

  if (zone == STYGIAN_DROP_CENTER) {
    // Add as tab to existing container
    stygian_dock_add_panel_to_node(dock, target, panel_id);
  } else {
    // Split the target node
    // Key insight: stygian_dock_split() puts existing content in child_a
    // We need to control where the NEW panel goes

    StygianDockSplit split_dir;
    bool new_panel_in_a; // True if new panel goes to child_a (LEFT/TOP)

    switch (zone) {
    case STYGIAN_DROP_LEFT:
      split_dir = STYGIAN_DOCK_SPLIT_VERTICAL;
      new_panel_in_a = true; // New panel on LEFT (child_a)
      break;
    case STYGIAN_DROP_RIGHT:
      split_dir = STYGIAN_DOCK_SPLIT_VERTICAL;
      new_panel_in_a = false; // New panel on RIGHT (child_b)
      break;
    case STYGIAN_DROP_TOP:
      split_dir = STYGIAN_DOCK_SPLIT_HORIZONTAL;
      new_panel_in_a = true; // New panel on TOP (child_a)
      break;
    case STYGIAN_DROP_BOTTOM:
      split_dir = STYGIAN_DOCK_SPLIT_HORIZONTAL;
      new_panel_in_a = false; // New panel on BOTTOM (child_b)
      break;
    default:
      return;
    }

    // Save target's existing panels before split
    uint32_t existing_panel_ids[STYGIAN_DOCK_MAX_TABS_PER_NODE];
    int existing_count = target->panel_count;
    int existing_active = target->active_panel;
    memcpy(existing_panel_ids, target->panel_ids, sizeof(existing_panel_ids));

    // Clear target before split (so split doesn't copy anything)
    target->panel_count = 0;
    memset(target->panel_ids, 0, sizeof(target->panel_ids));

    // Split
    StygianDockNode *child_a, *child_b;
    stygian_dock_split(dock, target, split_dir, 0.5f, &child_a, &child_b);

    // Place panels in correct children
    if (new_panel_in_a) {
      // New panel in child_a (LEFT/TOP), existing in child_b (RIGHT/BOTTOM)
      stygian_dock_add_panel_to_node(dock, child_a, panel_id);
      for (int i = 0; i < existing_count; i++) {
        stygian_dock_add_panel_to_node(dock, child_b, existing_panel_ids[i]);
      }
      child_b->active_panel = existing_active;
    } else {
      // Existing in child_a (LEFT/TOP), new panel in child_b (RIGHT/BOTTOM)
      for (int i = 0; i < existing_count; i++) {
        stygian_dock_add_panel_to_node(dock, child_a, existing_panel_ids[i]);
      }
      child_a->active_panel = existing_active;
      stygian_dock_add_panel_to_node(dock, child_b, panel_id);
    }
  }
}

// ============================================================================
// Tab Reordering (within same node)
// ============================================================================

static void reorder_tabs_in_node(StygianDockNode *node, int from_idx,
                                 int to_idx) {
  if (!node || from_idx == to_idx || from_idx < 0 || to_idx < 0 ||
      from_idx >= node->panel_count || to_idx >= node->panel_count) {
    return;
  }

  uint32_t panel_id = node->panel_ids[from_idx];

  // Shift elements
  if (from_idx < to_idx) {
    // Moving right: shift left
    for (int i = from_idx; i < to_idx; i++) {
      node->panel_ids[i] = node->panel_ids[i + 1];
    }
  } else {
    // Moving left: shift right
    for (int i = from_idx; i > to_idx; i--) {
      node->panel_ids[i] = node->panel_ids[i - 1];
    }
  }
  node->panel_ids[to_idx] = panel_id;
  node->active_panel = to_idx;

  printf("[Stygian Dock] Reordered tab %d -> %d\n", from_idx, to_idx);
}

// ============================================================================
// Remove Panel from Node + Tree Pruning
// ============================================================================

// Forward declaration for recursive pruning
static void prune_empty_nodes(StygianDockSpace *dock, StygianDockNode *node);

// Collapse a split node by promoting child to parent's position
static void collapse_node_into_parent(StygianDockSpace *dock,
                                      StygianDockNode *node,
                                      StygianDockNode *surviving_child) {
  if (!node || !surviving_child)
    return;

  StygianDockNode *parent = node->parent;

  printf("[Stygian Dock] Collapsing node %u, promoting child %u\n", node->id,
         surviving_child->id);

  if (!parent) {
    // Node is root - surviving child becomes new root
    dock->root = surviving_child;
    surviving_child->parent = NULL;
    printf("[Stygian Dock] Child %u is now root\n", surviving_child->id);
  } else {
    // Replace node with surviving_child in parent
    if (parent->child_a == node) {
      parent->child_a = surviving_child;
    } else if (parent->child_b == node) {
      parent->child_b = surviving_child;
    }
    surviving_child->parent = parent;
  }

  // Clear the old node (node pool reuse not implemented yet, just mark dead)
  memset(node, 0, sizeof(StygianDockNode));

  // Mark layout dirty - tree structure changed
  dock->layout_dirty = true;
}

// Check if a node is empty (leaf with no panels)
static bool is_node_empty(StygianDockNode *node) {
  if (!node)
    return true;
  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    return node->panel_count == 0;
  }
  return false;
}

// Recursively prune empty nodes from the tree
static void prune_empty_nodes(StygianDockSpace *dock, StygianDockNode *node) {
  if (!node)
    return;

  if (node->split_type != STYGIAN_DOCK_SPLIT_NONE) {
    // Check if either child is empty leaf
    bool a_empty = is_node_empty(node->child_a);
    bool b_empty = is_node_empty(node->child_b);

    if (a_empty && b_empty) {
      // Both children empty - this split node becomes an empty leaf
      printf(
          "[Stygian Dock] Both children of node %u empty, converting to leaf\n",
          node->id);
      node->split_type = STYGIAN_DOCK_SPLIT_NONE;
      node->child_a = NULL;
      node->child_b = NULL;
      node->panel_count = 0;

      // Now this node is empty, prune upward
      if (node->parent) {
        prune_empty_nodes(dock, node->parent);
      }
    } else if (a_empty) {
      // Only child_a is empty - promote child_b
      StygianDockNode *survivor = node->child_b;
      collapse_node_into_parent(dock, node, survivor);

      // Continue pruning from survivor's parent
      if (survivor->parent) {
        prune_empty_nodes(dock, survivor->parent);
      }
    } else if (b_empty) {
      // Only child_b is empty - promote child_a
      StygianDockNode *survivor = node->child_a;
      collapse_node_into_parent(dock, node, survivor);

      // Continue pruning from survivor's parent
      if (survivor->parent) {
        prune_empty_nodes(dock, survivor->parent);
      }
    }
  }
}

static void remove_panel_from_node(StygianDockSpace *dock,
                                   StygianDockNode *node, uint32_t panel_id) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE)
    return;

  for (int i = 0; i < node->panel_count; i++) {
    if (node->panel_ids[i] == panel_id) {
      // Shift remaining panels
      for (int j = i; j < node->panel_count - 1; j++) {
        node->panel_ids[j] = node->panel_ids[j + 1];
      }
      node->panel_count--;

      // Adjust active panel
      if (node->active_panel >= node->panel_count && node->panel_count > 0) {
        node->active_panel = node->panel_count - 1;
      }

      printf("[Stygian Dock] Removed panel from node %u (remaining: %d)\n",
             node->id, node->panel_count);

      // CRITICAL: Prune tree if this node is now empty
      if (node->panel_count == 0 && node->parent) {
        prune_empty_nodes(dock, node->parent);
      }

      return;
    }
  }
}

static StygianDockNode *find_node_with_panel(StygianDockNode *node,
                                             uint32_t panel_id) {
  if (!node)
    return NULL;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    for (int i = 0; i < node->panel_count; i++) {
      if (node->panel_ids[i] == panel_id) {
        return node;
      }
    }
    return NULL;
  }

  StygianDockNode *found = find_node_with_panel(node->child_a, panel_id);
  if (found)
    return found;
  return find_node_with_panel(node->child_b, panel_id);
}

// ============================================================================
// Input Handling
// ============================================================================

static void handle_input_recursive(StygianContext *ctx, StygianDockSpace *dock,
                                   StygianDockNode *node, int mx, int my,
                                   bool mouse_down, bool was_down) {
  if (!node)
    return;

  if (node->split_type != STYGIAN_DOCK_SPLIT_NONE) {
    // Handle splitter interaction
    float splitter = dock->splitter_size;
    bool over_splitter = false;

    if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
      float split_y = node->y + node->h * node->split_ratio;
      over_splitter = mx >= node->x && mx < node->x + node->w &&
                      my >= split_y - splitter && my < split_y + splitter;
    } else {
      float split_x = node->x + node->w * node->split_ratio;
      over_splitter = mx >= split_x - splitter && mx < split_x + splitter &&
                      my >= node->y && my < node->y + node->h;
    }

    node->splitter_hovered = over_splitter;

    // Double-click splitter = reset to 50%
    if (over_splitter && mouse_down && !was_down && !dock->dragging_panel_id) {
      uint64_t now = get_time_ms();
      float dx = (float)mx - dock->last_click_x;
      float dy = (float)my - dock->last_click_y;
      float dist = dx * dx + dy * dy;

      if (now - dock->last_click_time_ms < DOUBLE_CLICK_TIME_MS &&
          dist < DOUBLE_CLICK_DISTANCE * DOUBLE_CLICK_DISTANCE) {
        // Double-click detected - reset to 50%
        node->split_ratio = 0.5f;
        dock->layout_dirty = true;
        printf("[Stygian Dock] Splitter double-click: reset to 50%%\n");
      } else {
        // Single click - start dragging
        node->splitter_dragging = true;
      }

      dock->last_click_time_ms = now;
      dock->last_click_x = (float)mx;
      dock->last_click_y = (float)my;
    }

    // Continue dragging splitter
    if (node->splitter_dragging && mouse_down) {
      if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
        node->split_ratio = (my - node->y) / node->h;
      } else {
        node->split_ratio = (mx - node->x) / node->w;
      }
      // Clamp
      if (node->split_ratio < 0.1f)
        node->split_ratio = 0.1f;
      if (node->split_ratio > 0.9f)
        node->split_ratio = 0.9f;

      // CRITICAL: Mark layout dirty so children resize
      dock->layout_dirty = true;
    }

    // Stop dragging splitter
    if (!mouse_down) {
      node->splitter_dragging = false;
    }

    // Recurse to children
    handle_input_recursive(ctx, dock, node->child_a, mx, my, mouse_down,
                           was_down);
    handle_input_recursive(ctx, dock, node->child_b, mx, my, mouse_down,
                           was_down);

  } else {
    // Leaf node - handle tab clicks and drag start
    if (mouse_down && !was_down && !dock->dragging_panel_id) {
      // Check if click is in tab bar
      if (my >= node->y && my < node->y + dock->tab_height && mx >= node->x &&
          mx < node->x + node->w) {

        // Find which tab was clicked
        float tab_x = node->x + 2.0f;
        float tab_w = 120.0f;
        if (node->panel_count > 0) {
          tab_w = (node->w - 4.0f) / node->panel_count;
          if (tab_w > 150.0f)
            tab_w = 150.0f;
          if (tab_w < 60.0f)
            tab_w = 60.0f;
        }

        for (int i = 0; i < node->panel_count; i++) {
          if (mx >= tab_x && mx < tab_x + tab_w) {
            StygianDockPanel *panel = find_panel(dock, node->panel_ids[i]);

            // Check if close button was clicked
            if (panel && panel->closable) {
              float close_size = 14.0f;
              float close_x = tab_x + tab_w - close_size - 8;
              float close_y = node->y + (dock->tab_height - close_size) / 2.0f;

              if (mx >= close_x && mx < close_x + close_size && my >= close_y &&
                  my < close_y + close_size) {
                // Close button clicked - remove panel
                printf("[Stygian Dock] Closing panel: %s\n", panel->title);
                remove_panel_from_node(dock, node, node->panel_ids[i]);
                dock->layout_dirty = true;
                break;
              }
            }

            // Not close button - select tab and start potential drag
            node->active_panel = i;
            dock->focused_node = node; // Set focus to this panel

            dock->dragging_panel_id = node->panel_ids[i];
            dock->drag_start_x = (float)mx;
            dock->drag_start_y = (float)my;
            dock->drag_offset_x = (float)mx - tab_x;
            dock->drag_offset_y = (float)my - node->y;
            dock->drag_started = false;

            if (panel) {
              printf("[Stygian Dock] Clicked panel: %s\n", panel->title);
            }
            break;
          }
          tab_x += tab_w;
        }
      }
    }
  }
}

// ============================================================================
// Main Update
// ============================================================================

void stygian_dock_update(StygianContext *ctx, StygianFont font,
                         StygianDockSpace *dock, float x, float y, float w,
                         float h) {
  if (!dock->root) {
    stygian_dock_get_root(dock);
    dock->layout_dirty = true;
  }

  // Process floating windows - event pump, render, auto-close empty
  StygianAP *ap = stygian_get_ap(ctx);
  bool any_floating_down = false;
  for (int i = dock->floating_count - 1; i >= 0; i--) {
    StygianFloatingWindow *fw = &dock->floating[i];

    // Process window events
    if (fw->window) {
      stygian_window_process_events(fw->window);

      // Check if window was closed
      if (stygian_window_should_close(fw->window)) {
        // Window closed by user - move panel back to main dock
        if (fw->root_node && fw->root_node->panel_count > 0) {
          for (int p = 0; p < fw->root_node->panel_count; p++) {
            stygian_dock_add_panel_to_node(dock, dock->root,
                                           fw->root_node->panel_ids[p]);
          }
        }
        // Destroy surface first
        if (fw->surface && ap) {
          stygian_ap_surface_destroy(ap, fw->surface);
          fw->surface = NULL;
        }
        // Destroy window and remove
        stygian_window_destroy(fw->window);
        for (int j = i; j < dock->floating_count - 1; j++) {
          dock->floating[j] = dock->floating[j + 1];
        }
        dock->floating_count--;
        continue;
      }

      // Render floating window content
      if (fw->surface && fw->root_node && ap) {
        int fw_w, fw_h; // Logical size for layout
        stygian_window_get_size(fw->window, &fw_w, &fw_h);

        int fb_w, fb_h; // Physical size for render target
        stygian_window_get_framebuffer_size(fw->window, &fb_w, &fb_h);

        // Handle input for floating window (for tab dragging)
        int fw_mx, fw_my;
        stygian_mouse_pos(fw->window, &fw_mx, &fw_my);
        bool fw_mouse_down = stygian_mouse_down(fw->window, STYGIAN_MOUSE_LEFT);
        if (fw_mouse_down)
          any_floating_down = true;

        // Calculate layout for floating window's content (Logical)
        fw->root_node->x = 0;
        fw->root_node->y = 0;
        fw->root_node->w = (float)fw_w;
        fw->root_node->h = (float)fw_h;

        // DEBUG: Print sizes periodically
        static int debug_counter = 0;
        if (debug_counter++ % 600 == 0) {
          printf("[Stygian Dock] Float Win %d: Logical %dx%d, Physical %dx%d\n",
                 i, fw_w, fw_h, fb_w, fb_h);
        }

        // Check for tab click/drag in floating window
        if (fw->root_node->panel_count > 0 && fw_mouse_down &&
            !fw->prev_mouse_down) {
          // Check if clicking in tab bar area
          if (fw_my >= 0 && fw_my < dock->tab_height) {
            // Find which tab was clicked
            float tab_w = 120.0f;
            if (fw->root_node->panel_count > 0) {
              tab_w = (fw->root_node->w - 4.0f) / fw->root_node->panel_count;
              if (tab_w > 150.0f)
                tab_w = 150.0f;
              if (tab_w < 60.0f)
                tab_w = 60.0f;
            }
            int clicked_tab = (int)((fw_mx - 2.0f) / tab_w);
            if (clicked_tab >= 0 && clicked_tab < fw->root_node->panel_count) {
              // Start dragging this panel
              dock->dragging_panel_id = fw->root_node->panel_ids[clicked_tab];
              dock->drag_start_x = (float)fw_mx;
              dock->drag_start_y = (float)fw_my;
              dock->drag_started = false;
              dock->dragging_from_floating = true;
              dock->dragging_floating_idx = i;
              printf(
                  "[Stygian Dock] Started dragging from floating: panel %u\n",
                  dock->dragging_panel_id);
            }
          }
        }
        fw->prev_mouse_down = fw_mouse_down;

        // Save current element state (we'll render to floating surface)
        uint32_t saved_element_count = ctx->element_count;

        // Reset element count for fresh floating window render
        ctx->element_count = 0;

        fw->root_node->x = 0;
        fw->root_node->y = 0;
        fw->root_node->w = (float)fw_w;
        fw->root_node->h = (float)fw_h;

        // Render the floating window's dock node content
        render_node_recursive(ctx, font, dock, fw->root_node);

        // Begin rendering to floating window's surface (pass PHYSICAL size)
        // Note: Using a modified surface_begin signature or assuming checks
        stygian_ap_surface_begin(ap, fw->surface, fb_w, fb_h);

        // Submit elements to the floating surface
        if (ctx->element_count > 0) {
          stygian_ap_surface_submit(ap, fw->surface, ctx->elements,
                                    ctx->element_count);
        }

        stygian_ap_surface_end(ap, fw->surface);
        stygian_ap_surface_swap(ap, fw->surface);

        // Restore element count for main window render
        ctx->element_count = saved_element_count;
      }
    }

    // Auto-close empty floating windows
    if (!fw->root_node || fw->root_node->panel_count == 0) {
      printf("[Stygian Dock] Auto-closing empty floating window\n");
      // Destroy surface first
      if (fw->surface && ap) {
        stygian_ap_surface_destroy(ap, fw->surface);
        fw->surface = NULL;
      }
      if (fw->window) {
        stygian_window_destroy(fw->window);
      }
      // Remove from array
      for (int j = i; j < dock->floating_count - 1; j++) {
        dock->floating[j] = dock->floating[j + 1];
      }
      dock->floating_count--;
    }
  }

  // Check if resize happened - forces layout recalc
  if (w != dock->last_layout_w || h != dock->last_layout_h) {
    dock->layout_dirty = true;
    dock->last_layout_w = w;
    dock->last_layout_h = h;
  }

  // O(1) optimization: only recalculate layout when dirty
  if (dock->layout_dirty) {
    calculate_layout_recursive(dock, dock->root, x, y, w, h);
    dock->layout_dirty = false;
    dock->spatial_dirty = true; // Layout changed, rebuild spatial hash
  }

  // O(1) optimization: rebuild spatial hash when layout changes
  if (dock->spatial_dirty) {
    rebuild_spatial_hash(dock);
  }

  // Get input (no static - use struct for thread safety)
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  bool mouse_down = stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
  bool global_mouse_down = mouse_down || any_floating_down;
  bool was_down = dock->prev_mouse_down;

  // Override mouse position for hit testing if dragging from floating (or just
  // always to be safe) This ensures we detect drops even if valid main window
  // events aren't firing
  if (dock->dragging_from_floating || any_floating_down) {
#ifdef _WIN32
    POINT pt;
    if (GetCursorPos(&pt)) {
      int client_x, client_y;
      stygian_window_screen_to_client(win, pt.x, pt.y, &client_x, &client_y);
      mx = client_x;
      my = client_y;
    }
#endif
  }

  // ESC = cancel drag
  if (dock->dragging_panel_id && stygian_key_down(win, STYGIAN_KEY_ESCAPE)) {
    printf("[Stygian Dock] Drag cancelled (ESC)\n");
    dock->dragging_panel_id = 0;
    dock->drag_started = false;
    dock->drop_target = NULL;
    dock->drop_zone = STYGIAN_DROP_NONE;
  }

  // Handle drag continuation
  if (dock->dragging_panel_id && global_mouse_down) {
    float dx, dy;

    // When dragging from floating, use screen-space coordinates
    if (dock->dragging_from_floating) {
#ifdef _WIN32
      POINT pt;
      GetCursorPos(&pt);
      // Use screen-space delta (set drag_started immediately for floating)
      if (!dock->drag_started) {
        dock->drag_started = true;
        // Store screen position as new reference
        dock->drag_start_x = (float)pt.x;
        dock->drag_start_y = (float)pt.y;
        StygianDockPanel *panel = find_panel(dock, dock->dragging_panel_id);
        if (panel) {
          printf("[Stygian Dock] Dragging from floating: %s\n", panel->title);
        }
      }
      dx = (float)pt.x - dock->drag_start_x;
      dy = (float)pt.y - dock->drag_start_y;
#else
      dx = 0;
      dy = 0;
      dock->drag_started = true;
#endif
    } else {
      dx = (float)mx - dock->drag_start_x;
      dy = (float)my - dock->drag_start_y;
    }
    float drag_dist = dx * dx + dy * dy;

    // Check if drag threshold exceeded (5 pixels) - only for main dock drags
    if (!dock->drag_started && !dock->dragging_from_floating &&
        drag_dist > 25.0f) {
      dock->drag_started = true;
      StygianDockPanel *panel = find_panel(dock, dock->dragging_panel_id);
      if (panel) {
        printf("[Stygian Dock] Started dragging: %s\n", panel->title);
      }
    }

    // Find drop target using O(1) spatial hash
    if (dock->drag_started) {
      dock->drop_zone = STYGIAN_DROP_NONE;

      // Edge detection: if near window edge, allow floating (clear drop target)
      int win_w, win_h;
      stygian_window_get_size(win, &win_w, &win_h);
      float edge_margin = 30.0f; // 30px from edge = floating zone
      bool near_edge = (mx < edge_margin || mx > win_w - edge_margin ||
                        my < edge_margin || my > win_h - edge_margin);

      if (!near_edge) {
        // O(1) lookup: get node from spatial grid
        StygianDockNode *target =
            find_drop_target_fast(dock, (float)mx, (float)my);
        if (target) {
          // Now determine which drop zone within this node
          dock->drop_zone =
              detect_drop_zone(dock, target, (float)mx, (float)my);
          dock->drop_target =
              (dock->drop_zone != STYGIAN_DROP_NONE) ? target : NULL;
        } else {
          dock->drop_target = NULL;
        }
        // Hide ghost when not near edge
        ghost_hide(dock);
      } else {
        // Near edge = float zone - show external ghost
        dock->drop_target = NULL;

        // Get screen-space mouse position for ghost window
#ifdef _WIN32
        POINT pt;
        GetCursorPos(&pt);
        ghost_show(dock, pt.x, pt.y);
#else
        // Fallback: use window-relative coords
        ghost_show(dock, mx, my);
#endif
      }
    }
  }

  // Handle drop
  if (dock->dragging_panel_id && !global_mouse_down && was_down) {
    if (dock->drag_started && dock->drop_target &&
        dock->drop_zone != STYGIAN_DROP_NONE) {
      // Drop on valid target
      StygianDockNode *source_node = NULL;

      // Find source - either from floating window or main dock
      if (dock->dragging_from_floating) {
        // Source is in a floating window
        if (dock->dragging_floating_idx >= 0 &&
            dock->dragging_floating_idx < dock->floating_count) {
          StygianFloatingWindow *fw =
              &dock->floating[dock->dragging_floating_idx];
          source_node = fw->root_node;

          // Remove panel from floating window
          remove_panel_from_node(dock, source_node, dock->dragging_panel_id);

          // Execute drop to main dock
          execute_drop(dock, dock->dragging_panel_id, dock->drop_target,
                       dock->drop_zone);
          printf("[Stygian Dock] Redocked panel from floating window\n");
        }
      } else {
        // Source is in main dock
        source_node = find_node_with_panel(dock->root, dock->dragging_panel_id);

        if (source_node && source_node != dock->drop_target) {
          // Different node - normal cross-panel drop
          remove_panel_from_node(dock, source_node, dock->dragging_panel_id);
          execute_drop(dock, dock->dragging_panel_id, dock->drop_target,
                       dock->drop_zone);
        } else if (source_node == dock->drop_target) {
          // Same node - self-drop handling
          if (dock->drop_zone == STYGIAN_DROP_CENTER) {
            // Center drop on same node - check if reordering tabs
            // Calculate which tab position mouse is over
            float tab_w = 120.0f;
            if (source_node->panel_count > 0) {
              tab_w = (source_node->w - 4.0f) / source_node->panel_count;
              if (tab_w > 150.0f)
                tab_w = 150.0f;
              if (tab_w < 60.0f)
                tab_w = 60.0f;
            }

            // Find source index
            int src_idx = -1;
            for (int i = 0; i < source_node->panel_count; i++) {
              if (source_node->panel_ids[i] == dock->dragging_panel_id) {
                src_idx = i;
                break;
              }
            }

            // Find target index based on mouse position
            int dst_idx = (int)((mx - source_node->x - 2.0f) / tab_w);
            if (dst_idx < 0)
              dst_idx = 0;
            if (dst_idx >= source_node->panel_count)
              dst_idx = source_node->panel_count - 1;

            if (src_idx >= 0 && src_idx != dst_idx) {
              reorder_tabs_in_node(source_node, src_idx, dst_idx);
            } else {
              printf("[Stygian Dock] Self-drop to center - no-op\n");
            }
          } else if (source_node->panel_count > 1) {
            // Edge drop on same node with multiple tabs - split this panel
            printf("[Stygian Dock] Self-split: '%s' to zone %d\n",
                   find_panel(dock, dock->dragging_panel_id)->title,
                   dock->drop_zone);

            // Remove the panel first (before split messes with structure)
            remove_panel_from_node(dock, source_node, dock->dragging_panel_id);

            // Now execute drop on the (now modified) source node
            execute_drop(dock, dock->dragging_panel_id, source_node,
                         dock->drop_zone);
          } else {
            // Only one panel in node - splitting would create empty sibling
            printf("[Stygian Dock] Self-drop on single-panel node - no-op\n");
          }
        }
      }
    } else if (dock->drag_started && !dock->drop_target) {
      // Dropped outside any dock zone - float the panel
      // Position at SCREEN mouse location with default size
      float float_w = 400.0f;
      float float_h = 300.0f;
      float screen_x, screen_y;
#ifdef _WIN32
      POINT pt;
      GetCursorPos(&pt);
      screen_x = (float)pt.x - float_w / 2;
      screen_y = (float)pt.y - float_h / 2;
#else
      screen_x = (float)mx - float_w / 2;
      screen_y = (float)my - float_h / 2;
#endif
      stygian_dock_float_panel(ctx, dock, dock->dragging_panel_id, screen_x,
                               screen_y, float_w, float_h);
    }

    // Reset drag state
    dock->dragging_panel_id = 0;
    dock->drag_started = false;
    dock->drop_target = NULL;
    dock->drop_zone = STYGIAN_DROP_NONE;
    dock->dragging_from_floating = false;
    dock->dragging_floating_idx = -1;

    // Hide ghost window
    ghost_hide(dock);
  }

  // Handle regular input (clicks, splitter drag)
  handle_input_recursive(ctx, dock, dock->root, mx, my, global_mouse_down,
                         was_down);
  dock->prev_mouse_down = global_mouse_down; // Update for next frame

  // Render dock layout
  render_node_recursive(ctx, font, dock, dock->root);

  // Render drag preview
  if (dock->dragging_panel_id && dock->drag_started) {
    StygianDockPanel *panel = find_panel(dock, dock->dragging_panel_id);
    if (panel) {
      // Dragged tab preview (semi-transparent)
      float preview_w = 120.0f;
      float preview_h = dock->tab_height;
      float preview_x = (float)mx - dock->drag_offset_x;
      float preview_y = (float)my - dock->drag_offset_y;

      stygian_rect(ctx, preview_x, preview_y, preview_w, preview_h, 0.3f, 0.4f,
                   0.6f, 0.7f);
      if (font) {
        stygian_text(ctx, font, panel->title, preview_x + 8, preview_y + 6,
                     14.0f, 1.0f, 1.0f, 1.0f, 1.0f);
      }
    }

    // Render drop zone highlight or floating preview
    if (dock->drop_target) {
      render_drop_zone_overlay(ctx, dock, dock->drop_target, dock->drop_zone);
    } else {
      // No drop target = floating zone - show floating window preview
      float float_w = 400.0f;
      float float_h = 300.0f;
      float float_x = (float)mx - float_w / 2;
      float float_y = (float)my - float_h / 2;

      // Window-like preview (darker, larger)
      stygian_rect(ctx, float_x, float_y, float_w, float_h, 0.15f, 0.15f, 0.18f,
                   0.85f);
      // Tab bar at top
      stygian_rect(ctx, float_x, float_y, float_w, dock->tab_height, 0.2f,
                   0.22f, 0.25f, 0.9f);
      // Title
      if (font) {
        stygian_text(ctx, font, panel->title, float_x + 10, float_y + 6, 14.0f,
                     0.9f, 0.9f, 0.9f, 1.0f);
      }
      // "Float" indicator
      if (font) {
        stygian_text(ctx, font, "(FLOAT)", float_x + float_w - 60, float_y + 6,
                     12.0f, 0.5f, 0.7f, 1.0f, 1.0f);
      }
    }
  }

  // Restore main window context so subsequent rendering works
  // Restore main window context so subsequent rendering works
  if (ap) {
    stygian_ap_make_current(ap);

    // Also restore viewport (CRITICAL for fixing "small main window" bug)
    // Use framebuffer size to match physical pixels
    StygianWindow *win = stygian_get_window(ctx);
    if (win) {
      int fb_w, fb_h;
      stygian_window_get_framebuffer_size(win, &fb_w, &fb_h);
      stygian_ap_set_viewport(ap, fb_w, fb_h);
    }
  }
}

void stygian_dock_composite_main(StygianDockSpace *dock) {
  // TODO: Blit all docked panel FBOs to main window
}

void stygian_dock_render_floating(StygianDockSpace *dock, int floating_idx) {
  // TODO: Blit floating window panel FBO
}

// ============================================================================
// Floating Windows
// ============================================================================

void stygian_dock_float_panel(StygianContext *ctx, StygianDockSpace *dock,
                              uint32_t panel_id, float x, float y, float w,
                              float h) {
  if (!ctx || !dock || dock->floating_count >= STYGIAN_DOCK_MAX_FLOATING) {
    printf("[Stygian Dock] Cannot float panel: invalid args or max reached\n");
    return;
  }

  // Find panel
  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (!panel) {
    printf("[Stygian Dock] Cannot float panel %u: not found\n", panel_id);
    return;
  }

  // Find and remove from current node
  StygianDockNode *source_node = find_node_with_panel(dock->root, panel_id);
  if (source_node) {
    remove_panel_from_node(dock, source_node, panel_id);
  }

  // Create floating window
  StygianFloatingWindow *fw = &dock->floating[dock->floating_count];
  memset(fw, 0, sizeof(StygianFloatingWindow));

  // Create native window
  StygianWindowConfig cfg = {.width = (int)w,
                             .height = (int)h,
                             .title = panel->title,
                             .flags = STYGIAN_WINDOW_RESIZABLE,
                             .gl_major = 4,
                             .gl_minor = 3};

  StygianWindow *win = stygian_window_create(&cfg);
  if (!win) {
    printf("[Stygian Dock] Failed to create floating window\n");
    return;
  }

  // Position window
  stygian_window_set_position(win, (int)x, (int)y);

  // Store in floating window struct
  fw->window = win;
  fw->native_handle = stygian_window_native_handle(win);
  fw->x = x;
  fw->y = y;
  fw->w = w;
  fw->h = h;

  // Create render surface for this window via AP
  StygianAP *ap = stygian_get_ap(ctx);
  if (ap) {
    fw->surface = stygian_ap_surface_create(ap, win);
    if (!fw->surface) {
      printf("[Stygian Dock] Warning: Failed to create render surface\n");
    }
  }

  // Create a root node for this floating window's content
  if (dock->node_count < STYGIAN_DOCK_MAX_NODES) {
    StygianDockNode *node = alloc_node(dock);
    node->split_type = STYGIAN_DOCK_SPLIT_NONE;
    stygian_dock_add_panel_to_node(dock, node, panel_id);
    fw->root_node = node;
  }

  // Metaball animation state
  fw->undock_progress = 0.0f;
  fw->blend_radius = 100.0f;
  fw->melting = true;
  fw->visible = true;

  dock->floating_count++;
  dock->layout_dirty = true;

  printf("[Stygian Dock] Floated panel '%s' at (%.0f, %.0f) size %.0fx%.0f\n",
         panel->title, x, y, w, h);
}

void stygian_dock_dock_floating(StygianContext *ctx, StygianDockSpace *dock,
                                int floating_idx, StygianDockNode *target,
                                StygianDockDropZone zone) {
  if (!dock || floating_idx < 0 || floating_idx >= dock->floating_count) {
    printf("[Stygian Dock] Invalid floating index %d\n", floating_idx);
    return;
  }

  StygianFloatingWindow *fw = &dock->floating[floating_idx];

  // Get panel from floating window's root node
  if (fw->root_node && fw->root_node->panel_count > 0) {
    uint32_t panel_id = fw->root_node->panel_ids[0];
    StygianDockPanel *panel = find_panel(dock, panel_id);

    if (panel) {
      printf("[Stygian Dock] Re-docking panel '%s' to zone %d\n", panel->title,
             zone);

      // Add panel to target
      execute_drop(dock, panel_id, target, zone);

      // Clear floating window's node
      fw->root_node->panel_count = 0;
    }
  }

  // Destroy render surface first
  if (fw->surface && ctx) {
    StygianAP *ap = stygian_get_ap(ctx);
    if (ap) {
      stygian_ap_surface_destroy(ap, fw->surface);
    }
    fw->surface = NULL;
  }

  // Destroy window
  if (fw->window) {
    stygian_window_destroy(fw->window);
    fw->window = NULL;
  }

  // Remove from floating array (shift remaining)
  for (int i = floating_idx; i < dock->floating_count - 1; i++) {
    dock->floating[i] = dock->floating[i + 1];
  }
  dock->floating_count--;
  dock->layout_dirty = true;
}

// ============================================================================
// Serialization - JSON Format
// ============================================================================

// Simple JSON writer (no external deps)
typedef struct {
  FILE *f;
  int indent;
} JsonWriter;

static void json_indent(JsonWriter *w) {
  for (int i = 0; i < w->indent; i++)
    fprintf(w->f, "  ");
}

static void json_begin_obj(JsonWriter *w) {
  fprintf(w->f, "{\n");
  w->indent++;
}

static void json_end_obj(JsonWriter *w, bool comma) {
  w->indent--;
  json_indent(w);
  fprintf(w->f, "}%s\n", comma ? "," : "");
}

static void json_key(JsonWriter *w, const char *key) {
  json_indent(w);
  fprintf(w->f, "\"%s\": ", key);
}

static void json_str(JsonWriter *w, const char *key, const char *val,
                     bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": \"%s\"%s\n", key, val, comma ? "," : "");
}

static void json_int(JsonWriter *w, const char *key, int val, bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": %d%s\n", key, val, comma ? "," : "");
}

static void json_float(JsonWriter *w, const char *key, float val, bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": %.4f%s\n", key, val, comma ? "," : "");
}

static void json_bool(JsonWriter *w, const char *key, bool val, bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": %s%s\n", key, val ? "true" : "false",
          comma ? "," : "");
}

// Recursive node serialization
static void serialize_node(JsonWriter *w, StygianDockNode *node, bool comma) {
  if (!node) {
    json_indent(w);
    fprintf(w->f, "null%s\n", comma ? "," : "");
    return;
  }

  json_indent(w);
  json_begin_obj(w);

  json_int(w, "id", node->id, true);
  json_int(w, "split_type", node->split_type, true);
  json_float(w, "split_ratio", node->split_ratio, true);

  // Panel IDs array
  json_key(w, "panel_ids");
  fprintf(w->f, "[");
  for (int i = 0; i < node->panel_count; i++) {
    fprintf(w->f, "%d%s", node->panel_ids[i],
            i < node->panel_count - 1 ? ", " : "");
  }
  fprintf(w->f, "],\n");

  json_int(w, "active_panel", node->active_panel, true);

  // Children
  json_key(w, "child_a");
  serialize_node(w, node->child_a, true);
  json_key(w, "child_b");
  serialize_node(w, node->child_b, false);

  json_end_obj(w, comma);
}

bool stygian_dock_save(StygianDockSpace *dock, const char *path) {
  if (!dock || !path)
    return false;

  FILE *f = fopen(path, "w");
  if (!f) {
    printf("[Stygian Dock] Failed to open %s for writing\n", path);
    return false;
  }

  JsonWriter w = {.f = f, .indent = 0};

  json_begin_obj(&w);

  // Version
  json_int(&w, "version", 1, true);

  // Root node tree
  json_key(&w, "root");
  serialize_node(&w, dock->root, true);

  // Floating windows
  json_key(&w, "floating");
  fprintf(f, "[\n");
  w.indent++;
  for (int i = 0; i < dock->floating_count; i++) {
    StygianFloatingWindow *fw = &dock->floating[i];
    json_indent(&w);
    json_begin_obj(&w);

    json_float(&w, "x", fw->x, true);
    json_float(&w, "y", fw->y, true);
    json_float(&w, "w", fw->w, true);
    json_float(&w, "h", fw->h, true);

    // Serialize floating window's root node
    json_key(&w, "root");
    serialize_node(&w, fw->root_node, false);

    json_end_obj(&w, i < dock->floating_count - 1);
  }
  w.indent--;
  json_indent(&w);
  fprintf(f, "]\n");

  json_end_obj(&w, false);

  fclose(f);
  printf("[Stygian Dock] Saved layout to %s\n", path);
  return true;
}

// ============================================================================
// JSON Parsing (Simple)
// ============================================================================

typedef struct {
  const char *p;
  const char *end;
} JsonReader;

static void skip_ws(JsonReader *r) {
  while (r->p < r->end &&
         (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r'))
    r->p++;
}

static bool match_char(JsonReader *r, char c) {
  skip_ws(r);
  if (r->p < r->end && *r->p == c) {
    r->p++;
    return true;
  }
  return false;
}

static bool parse_string(JsonReader *r, char *out, int max_len) {
  skip_ws(r);
  if (*r->p != '"')
    return false;
  r->p++;

  int i = 0;
  while (r->p < r->end && *r->p != '"' && i < max_len - 1) {
    out[i++] = *r->p++;
  }
  out[i] = '\0';

  if (*r->p == '"')
    r->p++;
  return true;
}

static bool parse_int(JsonReader *r, int *out) {
  skip_ws(r);
  char *end;
  *out = (int)strtol(r->p, &end, 10);
  if (end == r->p)
    return false;
  r->p = end;
  return true;
}

static bool parse_float(JsonReader *r, float *out) {
  skip_ws(r);
  char *end;
  *out = strtof(r->p, &end);
  if (end == r->p)
    return false;
  r->p = end;
  return true;
}

static StygianDockNode *parse_node(JsonReader *r, StygianDockSpace *dock);

static bool parse_key_value(JsonReader *r, StygianDockSpace *dock,
                            StygianDockNode *node, StygianFloatingWindow *fw) {
  char key[64];
  if (!parse_string(r, key, sizeof(key)))
    return false;
  if (!match_char(r, ':'))
    return false;

  skip_ws(r);

  if (node) {
    if (strcmp(key, "id") == 0) {
      parse_int(r, (int *)&node->id);
    } else if (strcmp(key, "split_type") == 0) {
      int st;
      parse_int(r, &st);
      node->split_type = (StygianDockSplit)st;
    } else if (strcmp(key, "split_ratio") == 0) {
      parse_float(r, &node->split_ratio);
    } else if (strcmp(key, "active_panel") == 0) {
      parse_int(r, &node->active_panel);
    } else if (strcmp(key, "panel_ids") == 0) {
      if (!match_char(r, '['))
        return false;
      node->panel_count = 0;
      while (!match_char(r, ']')) {
        int id;
        if (parse_int(r, &id)) {
          if (node->panel_count < STYGIAN_DOCK_MAX_TABS_PER_NODE) {
            node->panel_ids[node->panel_count++] = id;
          }
        }
        match_char(r, ',');
      }
    } else if (strcmp(key, "child_a") == 0) {
      node->child_a = parse_node(r, dock);
      if (node->child_a)
        node->child_a->parent = node;
    } else if (strcmp(key, "child_b") == 0) {
      node->child_b = parse_node(r, dock);
      if (node->child_b)
        node->child_b->parent = node;
    }
  }

  if (fw) {
    if (strcmp(key, "x") == 0)
      parse_float(r, &fw->x);
    else if (strcmp(key, "y") == 0)
      parse_float(r, &fw->y);
    else if (strcmp(key, "w") == 0)
      parse_float(r, &fw->w);
    else if (strcmp(key, "h") == 0)
      parse_float(r, &fw->h);
    else if (strcmp(key, "root") == 0)
      fw->root_node = parse_node(r, dock);
  }

  return true;
}

static StygianDockNode *parse_node(JsonReader *r, StygianDockSpace *dock) {
  skip_ws(r);

  // Check for null
  if (r->p + 4 <= r->end && strncmp(r->p, "null", 4) == 0) {
    r->p += 4;
    return NULL;
  }

  if (!match_char(r, '{'))
    return NULL;

  StygianDockNode *node = alloc_node(dock);
  if (!node)
    return NULL;

  while (!match_char(r, '}')) {
    parse_key_value(r, dock, node, NULL);
    match_char(r, ',');
  }

  return node;
}

bool stygian_dock_load(StygianDockSpace *dock, const char *path) {
  if (!dock || !path)
    return false;

  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("[Stygian Dock] Failed to open %s for reading\n", path);
    return false;
  }

  // Read entire file
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *data = (char *)malloc(size + 1);
  if (!data) {
    fclose(f);
    return false;
  }
  fread(data, 1, size, f);
  data[size] = '\0';
  fclose(f);

  // Reset dock (keep panels, clear tree)
  dock->node_count = 0;
  dock->root = NULL;
  dock->floating_count = 0;

  JsonReader r = {.p = data, .end = data + size};

  if (!match_char(&r, '{')) {
    free(data);
    return false;
  }

  while (!match_char(&r, '}')) {
    char key[64];
    if (!parse_string(&r, key, sizeof(key)))
      break;
    if (!match_char(&r, ':'))
      break;

    skip_ws(&r);

    if (strcmp(key, "version") == 0) {
      int version;
      parse_int(&r, &version);
      if (version != 1) {
        printf("[Stygian Dock] Unsupported version: %d\n", version);
        free(data);
        return false;
      }
    } else if (strcmp(key, "root") == 0) {
      dock->root = parse_node(&r, dock);
    } else if (strcmp(key, "floating") == 0) {
      if (!match_char(&r, '['))
        break;

      while (!match_char(&r, ']')) {
        if (dock->floating_count >= STYGIAN_DOCK_MAX_FLOATING)
          break;

        if (!match_char(&r, '{'))
          break;

        StygianFloatingWindow *fw = &dock->floating[dock->floating_count++];
        memset(fw, 0, sizeof(StygianFloatingWindow));

        while (!match_char(&r, '}')) {
          parse_key_value(&r, dock, NULL, fw);
          match_char(&r, ',');
        }

        match_char(&r, ',');
      }
    }

    match_char(&r, ',');
  }

  free(data);
  dock->layout_dirty = true;
  dock->spatial_dirty = true;

  printf("[Stygian Dock] Loaded layout from %s\n", path);
  return true;
}

// ============================================================================
// Presets
// ============================================================================

void stygian_dock_preset_ide(StygianDockSpace *dock) {
  // Clear existing layout
  dock->node_count = 0;
  dock->floating_count = 0;

  // Create root split: Files(20%) | Editor+Console
  dock->root = alloc_node(dock);
  dock->root->split_type = STYGIAN_DOCK_SPLIT_HORIZONTAL;
  dock->root->split_ratio = 0.2f;

  // Left: Files panel
  dock->root->child_a = alloc_node(dock);
  dock->root->child_a->parent = dock->root;
  dock->root->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;
  // Panels assigned by name mapping

  // Right: Editor(70%) + Console(30%)
  dock->root->child_b = alloc_node(dock);
  dock->root->child_b->parent = dock->root;
  dock->root->child_b->split_type = STYGIAN_DOCK_SPLIT_VERTICAL;
  dock->root->child_b->split_ratio = 0.7f;

  dock->root->child_b->child_a = alloc_node(dock);
  dock->root->child_b->child_a->parent = dock->root->child_b;
  dock->root->child_b->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->root->child_b->child_b = alloc_node(dock);
  dock->root->child_b->child_b->parent = dock->root->child_b;
  dock->root->child_b->child_b->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->layout_dirty = true;
  dock->spatial_dirty = true;
  printf("[Stygian Dock] Applied IDE preset\n");
}

void stygian_dock_preset_3d_editor(StygianDockSpace *dock) {
  // Clear existing layout
  dock->node_count = 0;
  dock->floating_count = 0;

  // Create root split: Hierarchy(20%) | Viewport+Props
  dock->root = alloc_node(dock);
  dock->root->split_type = STYGIAN_DOCK_SPLIT_HORIZONTAL;
  dock->root->split_ratio = 0.2f;

  // Left: Hierarchy
  dock->root->child_a = alloc_node(dock);
  dock->root->child_a->parent = dock->root;
  dock->root->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;

  // Right: Viewport(75%) | Inspector(25%)
  dock->root->child_b = alloc_node(dock);
  dock->root->child_b->parent = dock->root;
  dock->root->child_b->split_type = STYGIAN_DOCK_SPLIT_HORIZONTAL;
  dock->root->child_b->split_ratio = 0.75f;

  dock->root->child_b->child_a = alloc_node(dock);
  dock->root->child_b->child_a->parent = dock->root->child_b;
  dock->root->child_b->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->root->child_b->child_b = alloc_node(dock);
  dock->root->child_b->child_b->parent = dock->root->child_b;
  dock->root->child_b->child_b->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->layout_dirty = true;
  dock->spatial_dirty = true;
  printf("[Stygian Dock] Applied 3D Editor preset\n");
}
