// widgets_demo.c - Comprehensive Stygian Demo with Advanced Features
// Showcases all widget categories including docking and tabs

#include "../include/stygian.h"
#include "../layout/stygian_layout.h"
#include "../layout/stygian_tabs.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <stdio.h>
#include <string.h>

// Advanced Features - Tabs
static StygianTabBar *tab_bar;

// Advanced Features - Split Panel
static StygianSplitPanel main_split = {.vertical = true, .split_ratio = 0.7f};

// Advanced Features - Menu & Toolbar
static const char *menu_labels[] = {"File", "Edit", "View", "Tools", "Help"};
static StygianMenuBar menu_bar = {.menu_labels = menu_labels,
                                  .menu_count = 5,
                                  .hot_menu = -1,
                                  .open_menu = -1};

static const char *tool_icons[] = {
    "S", "M", "R", "|", "P"}; // Select, Move, Rotate, separator, Play
static const char *tool_tips[] = {"Select", "Move", "Rotate", "", "Play"};
static StygianToolbar toolbar = {.tool_icons = tool_icons,
                                 .tool_tooltips = tool_tips,
                                 .tool_count = 5,
                                 .active_tool = 0};

// Game Engine Data (simplified from previous demo)
static StygianSceneNode scene_root = {"Scene", true, false, 0, NULL, NULL};
static StygianSceneHierarchy scene_hierarchy = {.root = &scene_root};
static StygianProperty props[] = {{"Position", "0, 0, 0", 0}};
static StygianInspector inspector = {
    .object_name = "Camera", .properties = props, .property_count = 1};
static StygianConsoleLog console_log = {.log_buffer =
                                            "[INFO] Advanced demo loaded\n"};

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  StygianWindowConfig win_cfg = {
      .title = "Stygian Advanced Demo - Docking & Tabs",
      .width = 1600,
      .height = 900,
      .flags = STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_OPENGL,
  };

  StygianWindow *window = stygian_window_create(&win_cfg);
  if (!window)
    return 1;

  StygianConfig cfg = {.backend = STYGIAN_BACKEND_OPENGL, .window = window};
  StygianContext *ctx = stygian_create(&cfg);
  if (!ctx)
    return 1;

  StygianFont font = stygian_font_load(ctx, "assets/fonts/inter_atlas.png",
                                       "assets/fonts/inter_atlas.json");
  if (!font)
    printf("Warning: Failed to load font\n");

  // Initialize tabs
  tab_bar = stygian_tab_bar_create(0, 0, 400, 32);
  stygian_tab_bar_add(tab_bar, "Scene", false);
  stygian_tab_bar_add(tab_bar, "Prefab", true);
  stygian_tab_bar_add(tab_bar, "Material", true);

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    stygian_widgets_begin_frame(ctx);
    while (stygian_window_poll_event(window, &event)) {
      stygian_widgets_process_event(ctx, &event);
      if (event.type == STYGIAN_EVENT_CLOSE)
        stygian_window_request_close(window);
    }

    int width, height;
    stygian_window_get_size(window, &width, &height);

    stygian_begin_frame(ctx, width, height);

    // Background
    stygian_rect(ctx, 0, 0, (float)width, (float)height, 0.08f, 0.08f, 0.08f,
                 1.0f);

    // Layout with Advanced Features
    if (stygian_tab_bar_get_count(tab_bar) > 0) {
      const char *active_title = stygian_tab_bar_get_title(
          tab_bar, stygian_tab_bar_get_active_index(tab_bar));
    }
    float menu_h = 24.0f;
    float toolbar_h = 36.0f;
    float tab_h = 28.0f;
    float console_h = 150.0f;

    // Menu Bar
    menu_bar.x = 0;
    menu_bar.y = 0;
    menu_bar.w = (float)width;
    menu_bar.h = menu_h;
    stygian_menu_bar(ctx, font, &menu_bar);

    // Toolbar
    toolbar.x = 0;
    toolbar.y = menu_h;
    toolbar.w = (float)width;
    toolbar.h = toolbar_h;
    int tool = stygian_toolbar(ctx, font, &toolbar);
    if (tool >= 0) {
      toolbar.active_tool = tool;
      printf("Tool selected: %d\n", tool);
    }

    float content_y = menu_h + toolbar_h;
    float content_h = height - content_y - console_h - 10;

    // Main Split Panel (Left: Hierarchy, Right: Viewport+Inspector)
    main_split.x = 10;
    main_split.y = content_y + 10;
    main_split.w = width - 20;
    main_split.h = content_h;

    float left_x, left_y, left_w, left_h;
    float right_x, right_y, right_w, right_h;

    stygian_split_panel(ctx, &main_split, &left_x, &left_y, &left_w, &left_h,
                        &right_x, &right_y, &right_w, &right_h);

    // Left Panel - Scene Hierarchy
    scene_hierarchy.x = left_x;
    scene_hierarchy.y = left_y;
    scene_hierarchy.w = left_w;
    scene_hierarchy.h = left_h;
    stygian_scene_hierarchy(ctx, font, &scene_hierarchy);

    // Right Panel - Tabbed Viewport/Inspector
    stygian_tab_bar_set_layout(tab_bar, right_x, right_y, right_w, tab_h);

    int clicked_tab = stygian_tab_bar_update(ctx, font, tab_bar);
    if (clicked_tab >= 0) {
      // tab switch handled internally
      printf("Tab switched to: %s\n",
             stygian_tab_bar_get_title(
                 tab_bar, stygian_tab_bar_get_active_index(tab_bar)));
    }

    // Tab Content Area
    float tab_content_y = right_y + tab_h + 4;
    float tab_content_h = right_h - tab_h - 4;

    // Show different content based on active tab
    if (stygian_tab_bar_get_active_index(tab_bar) == 0) {
      // Scene tab - show viewport placeholder
      stygian_rect(ctx, right_x, tab_content_y, right_w, tab_content_h, 0.05f,
                   0.05f, 0.05f, 1.0f);
      if (font) {
        stygian_text(ctx, font, "3D Viewport (Scene Tab)", right_x + 20,
                     tab_content_y + 20, 16.0f, 0.5f, 0.5f, 0.5f, 1.0f);
      }
    } else {
      // Other tabs - show inspector
      inspector.x = right_x;
      inspector.y = tab_content_y;
      inspector.w = right_w;
      inspector.h = tab_content_h;
      stygian_inspector(ctx, font, &inspector);
    }

    // Bottom Console
    console_log.x = 10;
    console_log.y = height - console_h - 10;
    console_log.w = width - 20;
    console_log.h = console_h;
    stygian_console_log(ctx, font, &console_log);

    stygian_end_frame(ctx);
    stygian_window_swap_buffers(window);
  }

  stygian_destroy(ctx);
  stygian_window_destroy(window);
  return 0;
}
