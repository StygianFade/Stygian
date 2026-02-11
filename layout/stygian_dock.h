// stygian_dock.h - Modern Docking System with Data Driven Immediate (DDI)
// Integration Part of Layout System - Multiviewport, Tabbed Containers,
// Floating Windows
//
// Architecture:
//   - Binary tree of DockNodes (splits or tab containers)
//   - Each panel renders via DDI callbacks (immediate mode)
//   - Floating windows use shared OpenGL contexts
//   - Metaball SDF melting effect on undock/redock

#ifndef STYGIAN_DOCK_H
#define STYGIAN_DOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct StygianContext StygianContext;
typedef uint32_t StygianFont;

// ============================================================================
// Constants
// ============================================================================

// Real-world panel counts (research):
//   VS Code: 8-15, Unreal: 12-25, Maya: 12-30, Blender: 8-18
//   Power users rarely exceed 25 panels. 32 is generous.
#define STYGIAN_DOCK_MAX_PANELS 32
#define STYGIAN_DOCK_MAX_NODES 64        // 2x panels for splits
#define STYGIAN_DOCK_MAX_TABS_PER_NODE 8 // Rarely need >8 tabs in one node
#define STYGIAN_DOCK_MAX_FLOATING 8      // Floating windows rare

// ============================================================================
// Enums
// ============================================================================

typedef enum StygianDockSplit {
  STYGIAN_DOCK_SPLIT_NONE = 0,   // Leaf node (tab container)
  STYGIAN_DOCK_SPLIT_HORIZONTAL, // Top/Bottom children
  STYGIAN_DOCK_SPLIT_VERTICAL    // Left/Right children
} StygianDockSplit;

typedef enum StygianDockDropZone {
  STYGIAN_DROP_NONE = 0,
  STYGIAN_DROP_CENTER, // Tab into existing container
  STYGIAN_DROP_LEFT,   // Split and insert left
  STYGIAN_DROP_RIGHT,  // Split and insert right
  STYGIAN_DROP_TOP,    // Split and insert top
  STYGIAN_DROP_BOTTOM, // Split and insert bottom
  STYGIAN_DROP_FLOAT   // Detach as floating window
} StygianDockDropZone;

// ============================================================================
// Dock Panel - Content with FBO render target
// ============================================================================

// ============================================================================
// Dock Panel - Content
// ============================================================================

typedef struct StygianDockPanel StygianDockPanel; // Opaque

// DDI render callback - your immediate-mode UI code goes here
typedef void (*StygianDockPanelRenderFn)(StygianDockPanel *panel,
                                         StygianContext *ctx, StygianFont font,
                                         float x, float y, float w, float h);

// ============================================================================
// Dock Node - Binary tree node
// ============================================================================

typedef struct StygianDockNode StygianDockNode; // Opaque

// ============================================================================
// Floating Window
// ============================================================================

typedef struct StygianFloatingWindow StygianFloatingWindow; // Opaque

// ============================================================================
// Dock Space - Root container
// ============================================================================

typedef struct StygianDockSpace StygianDockSpace; // Opaque

// ============================================================================
// API - Initialization & Lifecycle
// ============================================================================

// Create a new dock space on the heap
StygianDockSpace *stygian_dock_create(void *main_gl_context,
                                      void *main_device_context);

// Destroy dock space and free all resources
void stygian_dock_destroy(StygianDockSpace *dock);

// ============================================================================
// API - Panel Management
// ============================================================================

// Register a new panel with DDI render callback
// Returns panel ID, or 0 on failure
uint32_t stygian_dock_register_panel(StygianDockSpace *dock, const char *title,
                                     bool closable,
                                     StygianDockPanelRenderFn render,
                                     void *user_data);

// Unregister and remove panel
void stygian_dock_unregister_panel(StygianDockSpace *dock, uint32_t panel_id);

// Mark panel as needing re-render
void stygian_dock_mark_dirty(StygianDockSpace *dock, uint32_t panel_id);

// Get panel by ID (returns NULL if not found)
StygianDockPanel *stygian_dock_get_panel(StygianDockSpace *dock,
                                         uint32_t panel_id);

// ============================================================================
// API - Layout Building
// ============================================================================

// Add panel to a node (creates tab if node already has panels)
void stygian_dock_add_panel_to_node(StygianDockSpace *dock,
                                    StygianDockNode *node, uint32_t panel_id);

// Split a node, returns the two child nodes
// The original panels stay in child_a
void stygian_dock_split(StygianDockSpace *dock, StygianDockNode *node,
                        StygianDockSplit direction, float ratio,
                        StygianDockNode **out_a, StygianDockNode **out_b);

// Get or create root node
StygianDockNode *stygian_dock_get_root(StygianDockSpace *dock);

// ============================================================================
// API - Floating Windows
// ============================================================================

// Float a panel (creates new window with render surface)
void stygian_dock_float_panel(StygianContext *ctx, StygianDockSpace *dock,
                              uint32_t panel_id, float x, float y, float w,
                              float h);

// Dock a floating window back into main dock space
void stygian_dock_dock_floating(StygianContext *ctx, StygianDockSpace *dock,
                                int floating_idx, StygianDockNode *target,
                                StygianDockDropZone zone);

// ============================================================================
// API - Rendering & Interaction
// ============================================================================

// Update layout, handle input, render to FBOs
void stygian_dock_update(StygianContext *ctx, StygianFont font,
                         StygianDockSpace *dock, float x, float y, float w,
                         float h);

// Composite FBOs to main window (call after stygian_dock_update)
void stygian_dock_composite_main(StygianDockSpace *dock);

// Render floating windows (call for each floating window's message loop)
void stygian_dock_render_floating(StygianDockSpace *dock, int floating_idx);

// ============================================================================
// API - Serialization
// ============================================================================

// Save layout to JSON file
bool stygian_dock_save(StygianDockSpace *dock, const char *path);

// Load layout from JSON file
bool stygian_dock_load(StygianDockSpace *dock, const char *path);

// Apply preset layout
void stygian_dock_preset_ide(
    StygianDockSpace *dock); // Editor + Console + Files
void stygian_dock_preset_3d_editor(
    StygianDockSpace *dock); // Viewport + Props + Hierarchy

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_DOCK_H
