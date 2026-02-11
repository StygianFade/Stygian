// layout_test.c - Comprehensive Layout System Test
// Tests both stygian_layout.c (flexbox) and stygian_docking.c (tabs/splits)

#include "../include/stygian.h"
#include "../layout/stygian_layout.h"
#include "../layout/stygian_tabs.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <stdio.h>
#include <string.h>

// Test state
static StygianSplitPanel main_split = {.vertical = true, .split_ratio = 0.25f};
static StygianSplitPanel right_split = {.vertical = false, .split_ratio = 0.7f};

static StygianTabBar *tab_bar;

static const char *menu_labels[] = {"File", "Edit", "View", "Help"};
static StygianMenuBar menu_bar = {.menu_labels = menu_labels, .menu_count = 4};

static const char *tool_icons[] = {"S", "M", "R", "P"};
static StygianToolbar toolbar = {
    .tool_icons = tool_icons, .tool_count = 4, .active_tool = 0};

// Widget state for flexbox test
static bool checkbox_state = false;
static float slider_value = 0.5f;

static int button_clicks = 0;
static bool show_debug = false;

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  StygianWindowConfig win_cfg = {
      .title = "Stygian Layout System Test",
      .width = 1400,
      .height = 800,
      .flags = STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_OPENGL,
  };

  StygianWindow *window = stygian_window_create(&win_cfg);
  if (!window) {
    fprintf(stderr, "Failed to create window\n");
    return 1;
  }

  StygianConfig cfg = {.backend = STYGIAN_BACKEND_OPENGL, .window = window};
  StygianContext *ctx = stygian_create(&cfg);
  if (!ctx) {
    fprintf(stderr, "Failed to create Stygian context\n");
    return 1;
  }

  StygianFont font = stygian_font_load(ctx, "assets/fonts/inter_atlas.png",
                                       "assets/fonts/inter_atlas.json");
  if (!font)
    printf("Warning: Font not loaded\n");

  // Initialize tabs
  tab_bar = stygian_tab_bar_create(0, 0, 400, 32);
  stygian_tab_bar_add(tab_bar, "Properties", false);
  stygian_tab_bar_add(tab_bar, "Settings", true);
  stygian_tab_bar_add(tab_bar, "Debug", true);

  printf("=== Stygian Layout System Test ===\n");
  printf("Testing:\n");
  printf("  - Flexbox Layout (stygian_layout.c)\n");
  printf("  - Docking System (stygian_docking.c)\n");
  printf("  - Split Panels (draggable)\n");
  printf("  - Tab Bar\n");
  printf("  - Menu Bar\n");
  printf("  - Toolbar\n\n");

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    stygian_widgets_begin_frame(ctx);
    while (stygian_window_poll_event(window, &event)) {
      stygian_widgets_process_event(ctx, &event);
      if (event.type == STYGIAN_EVENT_CLOSE) {
        stygian_window_request_close(window);
      } else if (event.type == STYGIAN_EVENT_KEY_UP &&
                 event.key.key == STYGIAN_KEY_F1) {
        show_debug = !show_debug;
        printf("Debug Overlay: %s\n", show_debug ? "ON" : "OFF");
      }
    }

    int width, height;
    stygian_window_get_size(window, &width, &height);

    stygian_begin_frame(ctx, width, height);

    // Background
    stygian_rect(ctx, 0, 0, (float)width, (float)height, 0.08f, 0.08f, 0.08f,
                 1.0f);

    // === Top: Menu Bar ===
    float menu_h = 24.0f;
    menu_bar.x = 0;
    menu_bar.y = 0;
    menu_bar.w = (float)width;
    menu_bar.h = menu_h;
    stygian_menu_bar(ctx, font, &menu_bar);

    // === Below Menu: Toolbar ===
    float toolbar_h = 36.0f;
    toolbar.x = 0;
    toolbar.y = menu_h;
    toolbar.w = (float)width;
    toolbar.h = toolbar_h;
    int tool = stygian_toolbar(ctx, font, &toolbar);
    if (tool >= 0) {
      toolbar.active_tool = tool;
      printf("Tool selected: %d\n", tool);
    }

    // === Main Content Area with Split Panels ===
    float content_y = menu_h + toolbar_h + 10;
    float content_h = height - content_y - 10;

    // Main vertical split (Left sidebar | Right area)
    main_split.x = 10;
    main_split.y = content_y;
    main_split.w = width - 20;
    main_split.h = content_h;

    float left_x, left_y, left_w, left_h;
    float right_x, right_y, right_w, right_h;

    stygian_split_panel(ctx, &main_split, &left_x, &left_y, &left_w, &left_h,
                        &right_x, &right_y, &right_w, &right_h);

    // === LEFT PANEL: Flexbox Layout Test ===
    stygian_panel_begin(ctx, left_x, left_y, left_w, left_h);

    if (font) {
      stygian_text(ctx, font, "Flexbox Layout Test", left_x + 10, left_y + 10,
                   16.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Create vertical layout
    StygianLayout *vlayout = stygian_layout_begin(ctx, left_x + 10, left_y + 40,
                                                  left_w - 20, left_h - 50);
    stygian_layout_dir(vlayout, STYGIAN_LAYOUT_COLUMN);
    stygian_layout_gap(vlayout, 8.0f);
    stygian_layout_align(vlayout, STYGIAN_ALIGN_STRETCH);

    // Add widgets using flexbox
    for (int i = 0; i < 3; i++) {
      float btn_x, btn_y;
      stygian_layout_next(vlayout, 0, 32, &btn_x, &btn_y);

      char label[32];
      snprintf(label, sizeof(label), "Button %d", i + 1);

      if (stygian_button(ctx, font, label, btn_x, btn_y, left_w - 20, 32)) {
        button_clicks++;
        printf("Flexbox button %d clicked (total: %d)\n", i + 1, button_clicks);
      }
    }

    // Checkbox
    float check_x, check_y;
    stygian_layout_next(vlayout, 0, 24, &check_x, &check_y);
    stygian_checkbox(ctx, font, "Enable feature", check_x + 5, check_y,
                     &checkbox_state);

    // Slider
    float slider_x, slider_y;
    stygian_layout_next(vlayout, 0, 24, &slider_x, &slider_y);
    stygian_slider(ctx, slider_x, slider_y, left_w - 20, 20, &slider_value,
                   0.0f, 1.0f);

    stygian_layout_end(ctx, vlayout);
    stygian_panel_end(ctx);

    // === RIGHT PANEL: Horizontal split (Top viewport | Bottom tabs) ===
    right_split.x = right_x;
    right_split.y = right_y;
    right_split.w = right_w;
    right_split.h = right_h;

    float top_x, top_y, top_w, top_h;
    float bottom_x, bottom_y, bottom_w, bottom_h;

    stygian_split_panel(ctx, &right_split, &top_x, &top_y, &top_w, &top_h,
                        &bottom_x, &bottom_y, &bottom_w, &bottom_h);

    // Top: Viewport placeholder
    stygian_rect(ctx, top_x, top_y, top_w, top_h, 0.05f, 0.05f, 0.05f, 1.0f);
    if (font) {
      stygian_text(ctx, font, "Viewport Area", top_x + 20, top_y + 20, 18.0f,
                   0.5f, 0.5f, 0.5f, 1.0f);

      char info[128];
      snprintf(info, sizeof(info),
               "Split Ratio: %.2f (drag splitter to adjust)",
               right_split.split_ratio);
      stygian_text(ctx, font, info, top_x + 20, top_y + 50, 14.0f, 0.4f, 0.4f,
                   0.4f, 1.0f);
    }

    // Bottom: Tabbed panel
    float tab_h = 28.0f;
    stygian_tab_bar_set_layout(tab_bar, bottom_x, bottom_y, bottom_w, tab_h);

    int clicked_tab = stygian_tab_bar_update(ctx, font, tab_bar);
    if (clicked_tab >= 0) {
      // Tab switch handled internally
      printf("Tab switched to: %s\n",
             stygian_tab_bar_get_title(tab_bar, clicked_tab));
    }

    // Tab content
    float tab_content_y = bottom_y + tab_h + 4;
    float tab_content_h = bottom_h - tab_h - 4;

    stygian_panel_begin(ctx, bottom_x, tab_content_y, bottom_w, tab_content_h);

    if (font) {
      char content[64];
      snprintf(content, sizeof(content), "Content for: %s",
               stygian_tab_bar_get_title(
                   tab_bar, stygian_tab_bar_get_active_index(tab_bar)));
      stygian_text(ctx, font, content, bottom_x + 20, tab_content_y + 20, 14.0f,
                   0.8f, 0.8f, 0.8f, 1.0f);
    }

    stygian_panel_end(ctx);

    // Event loop handling for F1 toggle
    // (See event loop above)

    if (show_debug) {
      stygian_debug_overlay_draw(ctx);
    }

    stygian_end_frame(ctx);
    stygian_window_swap_buffers(window);
  }

  if (font)
    stygian_font_destroy(ctx, font);
  stygian_destroy(ctx);
  stygian_window_destroy(window);

  printf("\n=== Test Complete ===\n");
  printf("Total button clicks: %d\n", button_clicks);
  printf("Checkbox state: %s\n", checkbox_state ? "checked" : "unchecked");
  printf("Slider value: %.2f\n", slider_value);

  return 0;
}
