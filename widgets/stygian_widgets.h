// stygian_widgets.h - Widget API for Stygian UI Library
// High-level widgets built on top of Stygian rendering primitives
#ifndef STYGIAN_WIDGETS_H
#define STYGIAN_WIDGETS_H

#include "../include/stygian.h"
#include "../window/stygian_input.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Widget Configuration
// ============================================================================

typedef struct StygianWidgetStyle {
  float bg_color[4];
  float hover_color[4];
  float active_color[4];
  float text_color[4];
  float border_radius;
  float padding;
} StygianWidgetStyle;

// ============================================================================
// Button Widget
// ============================================================================

typedef struct StygianButton {
  float x, y, w, h;
  const char *label;
  bool hovered;
  bool pressed;
  bool clicked; // True for one frame when clicked
} StygianButton;

// Returns true if button was clicked this frame
bool stygian_button(StygianContext *ctx, StygianFont font, const char *label,
                    float x, float y, float w, float h);

// Button with custom style
bool stygian_button_ex(StygianContext *ctx, StygianFont font,
                       StygianButton *state, const StygianWidgetStyle *style);

// ============================================================================
// Slider Widget
// ============================================================================

typedef struct StygianSlider {
  float x, y, w, h;
  float value;    // Current value (0.0 - 1.0)
  float min, max; // Value range
  bool dragging;
} StygianSlider;

// Returns true if value changed
bool stygian_slider(StygianContext *ctx, float x, float y, float w, float h,
                    float *value, float min, float max);

// Slider with state management
bool stygian_slider_ex(StygianContext *ctx, StygianSlider *state,
                       const StygianWidgetStyle *style);

// ============================================================================
// Text Input Widget
// ============================================================================

typedef struct StygianTextInput {
  float x, y, w, h;
  char *buffer;
  int buffer_size;
  int cursor_pos;
  int selection_start;
  int selection_end;
  bool focused;
} StygianTextInput;

// Returns true if text changed
bool stygian_text_input(StygianContext *ctx, StygianFont font, float x, float y,
                        float w, float h, char *buffer, int buffer_size);

// Multiline Text Area with Scrolling & Selection
typedef struct StygianTextArea {
  float x, y, w, h;
  char *buffer;
  int buffer_size;
  int cursor_idx;      // Byte index
  int selection_start; // Byte index (-1 if no selection)
  int selection_end;   // Byte index (-1 if no selection)
  float scroll_y;
  float total_height; // Computed
  bool focused;
} StygianTextArea;

bool stygian_text_area(StygianContext *ctx, StygianFont font,
                       StygianTextArea *state);

// Vertical scrollbar for custom panels/areas.
// content_height: total scrollable content height in pixels.
// scroll_y: in/out scroll offset (0..content_height-viewport_height).
// Returns true when scroll_y changed.
bool stygian_scrollbar_v(StygianContext *ctx, float x, float y, float w,
                         float h, float content_height, float *scroll_y);

// ============================================================================
// Checkbox Widget
// ============================================================================

bool stygian_checkbox(StygianContext *ctx, StygianFont font, const char *label,
                      float x, float y, bool *checked);

// Radio button (returns true when clicked)
// selected: pointer to int holding current selection
// value: value this radio button represents
bool stygian_radio_button(StygianContext *ctx, StygianFont font,
                          const char *label, float x, float y, int *selected,
                          int value);

// ============================================================================
// Frame Management
// ============================================================================

typedef uint32_t StygianWidgetEventImpact;
#define STYGIAN_IMPACT_NONE 0u
#define STYGIAN_IMPACT_POINTER_ONLY (1u << 0)
#define STYGIAN_IMPACT_MUTATED_STATE (1u << 1)
#define STYGIAN_IMPACT_REQUEST_REPAINT (1u << 2)
#define STYGIAN_IMPACT_LAYOUT_CHANGED STYGIAN_IMPACT_MUTATED_STATE

typedef uint32_t StygianWidgetRegionFlags;
#define STYGIAN_WIDGET_REGION_POINTER_LEFT (1u << 0)
#define STYGIAN_WIDGET_REGION_POINTER_RIGHT (1u << 1)
#define STYGIAN_WIDGET_REGION_SCROLL (1u << 2)
#define STYGIAN_WIDGET_REGION_MUTATES (1u << 3)
#define STYGIAN_WIDGET_REGION_POINTER                                           \
  (STYGIAN_WIDGET_REGION_POINTER_LEFT | STYGIAN_WIDGET_REGION_POINTER_RIGHT)
#define STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES                              \
  (STYGIAN_WIDGET_REGION_POINTER_LEFT | STYGIAN_WIDGET_REGION_MUTATES)
#define STYGIAN_WIDGET_REGION_POINTER_RIGHT_MUTATES                             \
  (STYGIAN_WIDGET_REGION_POINTER_RIGHT | STYGIAN_WIDGET_REGION_MUTATES)

// Call at start of frame to update widget input state
void stygian_widgets_begin_frame(StygianContext *ctx);

// Process input events (call for every event in window loop)
void stygian_widgets_process_event(StygianContext *ctx, StygianEvent *e);
StygianWidgetEventImpact
stygian_widgets_process_event_ex(StygianContext *ctx, const StygianEvent *e);

// Register custom interactive regions (captured this frame, used for next-frame
// input routing). Use this for app-defined scroll/click areas that are not
// built from stock widgets.
// Note: pointer events only report mutation impact when the hit region includes
// STYGIAN_WIDGET_REGION_MUTATES.
void stygian_widgets_register_region(float x, float y, float w, float h,
                                     StygianWidgetRegionFlags flags);

// Commit captured regions after a rendered frame so routing can use the last
// valid frame snapshot even while render is skipped during idle.
void stygian_widgets_commit_regions(void);

// Per-frame scroll delta from processed input events.
float stygian_widgets_scroll_dx(void);
float stygian_widgets_scroll_dy(void);

// Request repaint cadence for this frame (max request wins).
// hz=0 is ignored.
void stygian_widgets_request_repaint_hz(uint32_t hz);

// Compute wait timeout for event-driven loops.
// Returns idle_wait_ms when no repaint is requested, otherwise clamp(1000/hz,
// 1..idle_wait_ms) with the highest requested hz.
uint32_t stygian_widgets_repaint_wait_ms(uint32_t idle_wait_ms);

// True when any widget requests a repaint (e.g., animated diagnostics graph).
bool stygian_widgets_wants_repaint(void);

// ============================================================================
// Diagnostics Widgets
// ============================================================================

#define STYGIAN_PERF_HISTORY_MAX 180

typedef struct StygianPerfWidget {
  float x, y, w, h;
  const char *renderer_name;
  bool enabled;
  bool show_graph;
  bool show_input;
  bool auto_scale_graph;
  uint32_t history_count;
  uint32_t history_head;
  float history_ms[STYGIAN_PERF_HISTORY_MAX];
  double last_sample_seconds;
  double last_render_seconds;
  float fps_smoothed;
  float fps_wall_smoothed;
  uint32_t history_window; // 0 = auto(120), else 60/120/180...
  uint32_t idle_hz;        // 0 = default 30
  uint32_t active_hz;      // 0 = default 60
  uint32_t text_hz;        // 0 = default 5
  uint32_t graph_max_segments; // 0 = default 64
  uint32_t max_stress_hz;  // 0 = disabled, else upper bound (e.g. 120/144)
  bool stress_mode;        // when true, use max_stress_hz for diagnostics tick
  uint32_t budget_miss_count;
  bool compact_mode;
  bool show_memory;
  bool show_glyphs;
  bool show_triad;
} StygianPerfWidget;

void stygian_perf_widget(StygianContext *ctx, StygianFont font,
                         StygianPerfWidget *state);
void stygian_perf_widget_set_rates(StygianPerfWidget *state, uint32_t graph_hz,
                                   uint32_t text_hz);
void stygian_perf_widget_set_enabled(StygianPerfWidget *state, bool enabled);

// ============================================================================
// Overlay Widgets
// ============================================================================

typedef struct StygianTooltip {
  const char *text;
  float x, y;
  float max_w;
  bool show;
} StygianTooltip;

void stygian_tooltip(StygianContext *ctx, StygianFont font,
                     const StygianTooltip *tooltip);

typedef struct StygianContextMenu {
  bool open;
  float x, y;
  float w;
  float item_h;
} StygianContextMenu;

bool stygian_context_menu_trigger_region(StygianContext *ctx,
                                         StygianContextMenu *state, float x,
                                         float y, float w, float h);
bool stygian_context_menu_begin(StygianContext *ctx, StygianFont font,
                                StygianContextMenu *state, int item_count);
bool stygian_context_menu_item(StygianContext *ctx, StygianFont font,
                               StygianContextMenu *state, const char *label,
                               int item_index);
void stygian_context_menu_end(StygianContext *ctx, StygianContextMenu *state);

typedef struct StygianModal {
  bool open;
  bool close_on_backdrop;
  float w, h;
  const char *title;
} StygianModal;

bool stygian_modal_begin(StygianContext *ctx, StygianFont font,
                         StygianModal *state, float viewport_w,
                         float viewport_h);
void stygian_modal_end(StygianContext *ctx, StygianModal *state);

// ============================================================================
// Panel Widget
// ============================================================================

void stygian_panel_begin(StygianContext *ctx, float x, float y, float w,
                         float h);
void stygian_panel_end(StygianContext *ctx);

// ============================================================================
// IDE Widgets - File Navigation
// ============================================================================

typedef struct StygianFileEntry {
  char name[64];
  bool is_directory;
  bool is_expanded;
  bool is_selected;
  int depth;
  // Internal use
  struct StygianFileEntry *next;
  struct StygianFileEntry *children;
} StygianFileEntry;

typedef struct StygianFileExplorer {
  float x, y, w, h;
  const char *root_path;
  char selected_path[256];
  float scroll_y;
  // Callback for file selection/opening
  void (*on_file_select)(const char *path);
  void (*on_file_open)(const char *path);
} StygianFileExplorer;

// Returns true if a file was selected/opened this frame
bool stygian_file_explorer(StygianContext *ctx, StygianFont font,
                           StygianFileExplorer *state);

typedef struct StygianBreadcrumb {
  float x, y, w, h;
  const char *path; // e.g., "src/widgets/ide/file_explorer.c"
  char separator;   // e.g., '/' or '>'
} StygianBreadcrumb;

// Returns true if a path segment was clicked.
// out_path will be filled with the path up to the clicked segment.
bool stygian_breadcrumb(StygianContext *ctx, StygianFont font,
                        StygianBreadcrumb *state, char *out_path, int max_len);

// ============================================================================
// IDE Widgets - Output & Diagnostics
// ============================================================================

typedef struct StygianOutputPanel {
  float x, y, w, h;
  const char *title;
  // Circular buffer or pointer to log data
  // For demo, standard C string
  const char *text_buffer;
  float scroll_y;
  bool auto_scroll;
} StygianOutputPanel;

void stygian_output_panel(StygianContext *ctx, StygianFont font,
                          StygianOutputPanel *state);

typedef struct StygianProblem {
  int line;
  int column;
  int severity; // 0=Info, 1=Warning, 2=Error
  char message[128];
  char file[64];
} StygianProblem;

typedef struct StygianProblemsPanel {
  float x, y, w, h;
  StygianProblem *problems;
  int problem_count;
  float scroll_y;
  int selected_index;
} StygianProblemsPanel;

bool stygian_problems_panel(StygianContext *ctx, StygianFont font,
                            StygianProblemsPanel *state);

// ============================================================================
// IDE Widgets - Debugging
// ============================================================================

typedef struct StygianDebugToolbar {
  float x, y, w, h;
  bool is_paused;
  // Callback
  void (*on_action)(
      int action_id); // 0=cont, 1=step_over, 2=step_into, 3=step_out, 4=stop
} StygianDebugToolbar;

void stygian_debug_toolbar(StygianContext *ctx, StygianFont font,
                           StygianDebugToolbar *state);

typedef struct StygianStackFrame {
  char function[64];
  char file[64];
  int line;
  uintptr_t address;
} StygianStackFrame;

typedef struct StygianCallStack {
  float x, y, w, h;
  StygianStackFrame *frames;
  int frame_count;
  int selected_frame;
} StygianCallStack;

bool stygian_call_stack(StygianContext *ctx, StygianFont font,
                        StygianCallStack *state);

// ============================================================================
// CAD Widgets - Precision Input
// ============================================================================

typedef struct StygianCoordinateInput {
  float x_val, y_val, z_val;
  bool locked_x, locked_y, locked_z;
  float x, y, w, h;
  const char *label; // Optional group label
} StygianCoordinateInput;

// Returns true if values changed
bool stygian_coordinate_input(StygianContext *ctx, StygianFont font,
                              StygianCoordinateInput *state);

typedef struct StygianSnapSettings {
  bool grid_snap;
  float grid_size;
  bool angel_snap;
  float angle_step;
  bool object_snap; // Endpoints, midpoints, etc.
  float x, y, w, h;
} StygianSnapSettings;

bool stygian_snap_settings(StygianContext *ctx, StygianFont font,
                           StygianSnapSettings *state);

// ============================================================================
// CAD Widgets - 3D Manipulation
// ============================================================================

typedef enum StygianGizmoMode {
  STYGIAN_GIZMO_TRANSLATE,
  STYGIAN_GIZMO_ROTATE,
  STYGIAN_GIZMO_SCALE
} StygianGizmoMode;

typedef struct StygianCADGizmo {
  float x, y, w, h; // 2D Screen rect usually, but here just a panel control
  StygianGizmoMode mode;
  bool local_space;
  // Interaction state
  int active_axis; // -1=none, 0=X, 1=Y, 2=Z
} StygianCADGizmo;

void stygian_cad_gizmo_controls(StygianContext *ctx, StygianFont font,
                                StygianCADGizmo *state);

typedef struct StygianLayer {
  char name[32];
  bool visible;
  bool locked;
  struct StygianLayer *next;
} StygianLayer;

typedef struct StygianLayerManager {
  float x, y, w, h;
  StygianLayer *layers;
  int layer_count;
  int active_layer_index;
  float scroll_y;
} StygianLayerManager;

bool stygian_layer_manager(StygianContext *ctx, StygianFont font,
                           StygianLayerManager *state);

// ============================================================================
// Game Engine Widgets - Viewport & Scene
// ============================================================================

typedef struct StygianSceneViewport {
  float x, y, w, h;
  uint32_t framebuffer_texture; // External texture ID from game engine
  bool show_grid;
  bool show_gizmo;
} StygianSceneViewport;

void stygian_scene_viewport(StygianContext *ctx, StygianSceneViewport *state);

typedef struct StygianSceneNode {
  char name[64];
  bool visible;
  bool selected;
  int depth;
  struct StygianSceneNode *next;
  struct StygianSceneNode *children;
} StygianSceneNode;

typedef struct StygianSceneHierarchy {
  float x, y, w, h;
  StygianSceneNode *root;
  int selected_node_id;
  float scroll_y;
} StygianSceneHierarchy;

bool stygian_scene_hierarchy(StygianContext *ctx, StygianFont font,
                             StygianSceneHierarchy *state);

typedef struct StygianProperty {
  char name[32];
  char value[64];
  int type; // 0=string, 1=float, 2=int, 3=bool, 4=color
} StygianProperty;

typedef struct StygianInspector {
  float x, y, w, h;
  const char *object_name;
  StygianProperty *properties;
  int property_count;
  float scroll_y;
} StygianInspector;

bool stygian_inspector(StygianContext *ctx, StygianFont font,
                       StygianInspector *state);

// ============================================================================
// Game Engine Widgets - Assets & Console
// ============================================================================

typedef struct StygianAsset {
  char name[64];
  int type; // 0=texture, 1=model, 2=material, 3=script, etc.
  bool selected;
} StygianAsset;

typedef struct StygianAssetBrowser {
  float x, y, w, h;
  StygianAsset *assets;
  int asset_count;
  float scroll_y;
  int selected_index;
} StygianAssetBrowser;

bool stygian_asset_browser(StygianContext *ctx, StygianFont font,
                           StygianAssetBrowser *state);

typedef struct StygianConsoleLog {
  float x, y, w, h;
  const char *log_buffer; // Newline-separated log entries
  float scroll_y;
  bool auto_scroll;
} StygianConsoleLog;

void stygian_console_log(StygianContext *ctx, StygianFont font,
                         StygianConsoleLog *state);

// ============================================================================
// Advanced Features - Docking & Layout
// NOTE: TabBar moved to layout/stygian_tabs.h (production version)
// ============================================================================

typedef struct StygianSplitPanel {
  float x, y, w, h;
  bool vertical;     // true=vertical split, false=horizontal
  float split_ratio; // 0.0-1.0
  bool dragging;
} StygianSplitPanel;

// Returns true if split ratio changed
bool stygian_split_panel(StygianContext *ctx, StygianSplitPanel *state,
                         float *out_left_x, float *out_left_y,
                         float *out_left_w, float *out_left_h,
                         float *out_right_x, float *out_right_y,
                         float *out_right_w, float *out_right_h);

typedef struct StygianMenuBar {
  float x, y, w, h;
  const char **menu_labels;
  int menu_count;
  int hot_menu;
  int open_menu;
} StygianMenuBar;

void stygian_menu_bar(StygianContext *ctx, StygianFont font,
                      StygianMenuBar *state);

typedef struct StygianToolbar {
  float x, y, w, h;
  const char **tool_icons;
  const char **tool_tooltips;
  int tool_count;
  int active_tool;
} StygianToolbar;

int stygian_toolbar(StygianContext *ctx, StygianFont font,
                    StygianToolbar *state);

// ============================================================================
// Node Graph Editor (Spatial JIT Architecture)
// ============================================================================

typedef struct StygianNodeBuffers {
  float *x;
  float *y;
  float *w;
  float *h;
  // Optional
  int *type_id;
  bool *selected;
  // Pointers to your flat arrays
} StygianNodeBuffers;

typedef struct StygianGraphState {
  float x, y, w, h; // Widget rect
  float pan_x, pan_y;
  float zoom;
  bool snap_enabled;
  float snap_size;
  float pin_y_offset;
  float pin_size;
  int wire_style;
  int selection_count;
  // Internal spatial hash
  void *spatial_grid;
  // Internal JIT iterator
  int iter_idx;
  int visible_ids[8192]; // Max visible nodes per frame (cull buffer) -
                         // Increased for robustness using stack/struct size
  int visible_count;

  // Interaction
  int hovered_id;
  int dragging_id;
  float drag_offset_x, drag_offset_y;
} StygianGraphState;

// 1. Begin the graph frame. Uploads/Culls visible nodes.
void stygian_node_graph_begin(StygianContext *ctx, StygianGraphState *state,
                              StygianNodeBuffers *data, int count);

// 2. Iterate ONLY visible nodes. Returns true if next node is ready.
bool stygian_node_graph_next(StygianContext *ctx, StygianGraphState *state,
                             int *out_index);

// 3. Helper to draw a node (User calls this inside the loop, or writes their
// own) Returns true if node interaction occurred
bool stygian_node_def(StygianContext *ctx, const char *title, float x, float y,
                      float w, float h, bool selected);

// 4. End the graph frame. Handles background grid, selection logic.
void stygian_node_graph_end(StygianContext *ctx, StygianGraphState *state);

// 5. Connections (User iterates visible connections manually or uses this
// helper)
void stygian_node_link(StygianContext *ctx, float x1, float y1, float x2,
                       float y2, float thick, float color[4]);
void stygian_graph_link(StygianContext *ctx, const StygianGraphState *state,
                        float x1, float y1, float x2, float y2, float thick,
                        float color[4]);

// Grid snapping helpers (world space)
void stygian_graph_set_snap(StygianGraphState *state, bool enabled,
                            float size);
void stygian_graph_snap_pos(const StygianGraphState *state, float *x, float *y);

// Graph coordinate helpers
void stygian_graph_world_to_screen(const StygianGraphState *state, float wx,
                                   float wy, float *sx, float *sy);
void stygian_graph_screen_to_world(const StygianGraphState *state, float sx,
                                   float sy, float *wx, float *wy);
void stygian_graph_node_screen_rect(const StygianGraphState *state, float wx,
                                    float wy, float ww, float wh, float *sx,
                                    float *sy, float *sw, float *sh);
void stygian_graph_pin_center_world(const StygianGraphState *state, float wx,
                                    float wy, float ww, bool output, float *px,
                                    float *py);
void stygian_graph_pin_rect_screen(const StygianGraphState *state, float wx,
                                   float wy, float ww, bool output, float *x,
                                   float *y, float *w, float *h);
bool stygian_graph_pin_hit_test(const StygianGraphState *state, float wx,
                                float wy, float ww, bool output, float mx,
                                float my);
bool stygian_graph_link_visible(const StygianGraphState *state, float ax,
                                float ay, float bx, float by, float padding);
bool stygian_graph_link_visible_bezier(const StygianGraphState *state, float x1,
                                       float y1, float x2, float y2,
                                       float padding);
void stygian_graph_draw_grid(StygianContext *ctx,
                             const StygianGraphState *state, float major,
                             float minor, float r, float g, float b,
                             float a);
bool stygian_graph_node_hit_test(const StygianGraphState *state, float wx,
                                 float wy, float ww, float wh, float mx,
                                 float my);
int stygian_graph_pick_node(const StygianGraphState *state,
                            const StygianNodeBuffers *data, float mx,
                            float my);

// Wire style
enum {
  STYGIAN_WIRE_SMOOTH = 0,
  STYGIAN_WIRE_SHARP = 1,
};
void stygian_graph_set_wire_style(StygianGraphState *state, int style);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_WIDGETS_H
