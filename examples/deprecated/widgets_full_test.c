// widgets_full_test.c - Complete Widget & Tab System Test
// Tests: checkbox, radio buttons, tabs with reordering

#include "../include/stygian.h"
#include "../layout/stygian_tabs.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static double now_seconds(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  StygianWindowConfig win_cfg = {
      .title = "Stygian Complete Test - Widgets & Tabs",
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

  StygianFont font =
      stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");
  if (!font)
    printf("Warning: Font not loaded\n");

  // Initialize tab bar
  StygianTabBar *tab_bar = stygian_tab_bar_create(0, 0, 1400, 32);
  stygian_tab_bar_add(tab_bar, "Widgets", true); // Now closable
  stygian_tab_bar_add(tab_bar, "Settings", true);
  stygian_tab_bar_add(tab_bar, "Debug", true);
  stygian_tab_bar_add(tab_bar, "Info", true);

  // Widget state
  bool checkbox1 = false;
  bool checkbox2 = true;
  bool checkbox3 = false;
  int radio_selection = 0;
  float slider_value = 0.5f;
  float custom_scroll = 0.0f;
  bool perf_pos_init = false;
  StygianPerfWidget perf = {
      .x = 0.0f,
      .y = 0.0f,
      .w = 360.0f,
      .h = 240.0f,
      .renderer_name = "OpenGL",
      .enabled = true,
      .show_graph = true,
      .show_input = true,
      .auto_scale_graph = false,
      .history_window = 120u,
      .idle_hz = 30u,
      .active_hz = 30u,
      .text_hz = 5u,
      .max_stress_hz = 120u,
      .stress_mode = false,
      .compact_mode = false,
      .show_memory = true,
      .show_glyphs = true,
      .show_triad = true,
  };
  double perf_log_t = now_seconds();

  printf("=== Stygian Complete Test ===\n");
  printf("Testing:\n");
  printf("  1. Checkbox widgets\n");
  printf("  2. Radio button widgets\n");
  printf("  3. Tab system with reordering\n");
  printf("  4. Slider widget\n\n");

  const StygianScopeId k_scope_chrome = 0x1001u;
  const StygianScopeId k_scope_content = 0x1003u;
  const StygianScopeId k_scope_perf =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x1002u;

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    static bool first_frame = true;
    bool event_mutated = false;
    bool event_requested = false;
    bool event_eval = false;
    bool chrome_changed = false;
    bool content_changed = false;
    bool any_state_changed = false;
    uint32_t wait_ms = stygian_next_repaint_wait_ms(ctx, 250u);
    bool repaint_pending = stygian_has_pending_repaint(ctx);
    stygian_widgets_begin_frame(ctx);
    while (stygian_window_poll_event(window, &event)) {
      StygianWidgetEventImpact impact =
          stygian_widgets_process_event_ex(ctx, &event);
      if (impact & STYGIAN_IMPACT_MUTATED_STATE)
        event_mutated = true;
      if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
        event_requested = true;
      if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
        event_eval = true;
      if (event.type == STYGIAN_EVENT_CLOSE) {
        stygian_window_request_close(window);
      }
    }

    if (!event_mutated && !event_requested && !event_eval && !first_frame) {
      if (stygian_window_wait_event_timeout(window, &event, wait_ms)) {
        StygianWidgetEventImpact impact =
            stygian_widgets_process_event_ex(ctx, &event);
        if (impact & STYGIAN_IMPACT_MUTATED_STATE)
          event_mutated = true;
        if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
          event_requested = true;
        if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
          event_eval = true;
        if (event.type == STYGIAN_EVENT_CLOSE) {
          stygian_window_request_close(window);
        }
        while (stygian_window_poll_event(window, &event)) {
          StygianWidgetEventImpact queued_impact =
              stygian_widgets_process_event_ex(ctx, &event);
          if (queued_impact & STYGIAN_IMPACT_MUTATED_STATE)
            event_mutated = true;
          if (queued_impact & STYGIAN_IMPACT_REQUEST_REPAINT)
            event_requested = true;
          if (queued_impact & STYGIAN_IMPACT_REQUEST_EVAL)
            event_eval = true;
          if (event.type == STYGIAN_EVENT_CLOSE) {
            stygian_window_request_close(window);
          }
        }
      }
    }

    repaint_pending = stygian_has_pending_repaint(ctx);
    bool render_frame = first_frame || event_mutated || repaint_pending;
    bool eval_only_frame = (!render_frame && (event_eval || event_requested));
    if (!render_frame && !eval_only_frame) {
      continue;
    }
    if (!eval_only_frame && (repaint_pending || event_requested)) {
      stygian_scope_invalidate_now(ctx, k_scope_perf);
    }
    first_frame = false;

    int width, height;
    int mx, my;
    stygian_window_get_size(window, &width, &height);
    stygian_mouse_pos(window, &mx, &my);
    // tab_bar->w = (float)width; // Not accessible anymore, re-create or ignore
    // for now

    stygian_begin_frame_intent(
        ctx, width, height,
        eval_only_frame ? STYGIAN_FRAME_EVAL_ONLY : STYGIAN_FRAME_RENDER);
    stygian_scope_begin(ctx, k_scope_chrome);

    // Background
    stygian_rect(ctx, 0, 0, (float)width, (float)height, 0.08f, 0.08f, 0.08f,
                 1.0f);
    // Tab bar
    int tab_result = stygian_tab_bar_update(ctx, font, tab_bar);
    if (tab_result != 0) {
      chrome_changed = true;
      content_changed = true;
    }
    if (tab_result == 1) {
      printf("Tab switched to: %s\n",
             stygian_tab_bar_get_title(
                 tab_bar, stygian_tab_bar_get_active_index(tab_bar)));
    } else if (tab_result == 2) {
      printf("Tab closed. Remaining: %d\n", stygian_tab_bar_get_count(tab_bar));
    } else if (tab_result == 3) {
      printf("Tab reordered New order:\n");
      for (int i = 0; i < stygian_tab_bar_get_count(tab_bar); i++) {
        printf("  %d: %s\n", i, stygian_tab_bar_get_title(tab_bar, i));
      }
    }
    stygian_scope_end(ctx);

    stygian_scope_begin(ctx, k_scope_content);
    // Content area
    float content_y = 32.0f + 20;
    float content_h = height - content_y - 20;

    stygian_panel_begin(ctx, 20, content_y, width - 40, content_h);

    const char *active_title =
        stygian_tab_bar_get_count(tab_bar) > 0
            ? stygian_tab_bar_get_title(
                  tab_bar, stygian_tab_bar_get_active_index(tab_bar))
            : "No Tabs";

    char title[128];
    snprintf(title, sizeof(title), "Tab: %s", active_title);
    if (font) {
      stygian_text(ctx, font, title, 40, content_y + 20, 24.0f, 1.0f, 1.0f,
                   1.0f, 1.0f);
    }

    // Checkbox section (always render, text optional)
    if (font) {
      stygian_text(ctx, font, "Checkboxes:", 40, content_y + 70, 18.0f, 0.8f,
                   0.8f, 0.8f, 1.0f);
    }

    if (stygian_checkbox(ctx, font, "Option 1", 60, content_y + 100,
                         &checkbox1)) {
      content_changed = true;
      printf("Checkbox 1 toggled: %s\n", checkbox1 ? "ON" : "OFF");
    }
    if (stygian_checkbox(ctx, font, "Option 2", 60, content_y + 130,
                         &checkbox2)) {
      content_changed = true;
      printf("Checkbox 2 toggled: %s\n", checkbox2 ? "ON" : "OFF");
    }
    if (stygian_checkbox(ctx, font, "Option 3", 60, content_y + 160,
                         &checkbox3)) {
      content_changed = true;
      printf("Checkbox 3 toggled: %s\n", checkbox3 ? "ON" : "OFF");
    }

    // Radio button section
    if (font) {
      stygian_text(ctx, font, "Radio Buttons:", 40, content_y + 210, 18.0f,
                   0.8f, 0.8f, 0.8f, 1.0f);
    }

    if (stygian_radio_button(ctx, font, "Choice A", 60, content_y + 240,
                             &radio_selection, 0)) {
      content_changed = true;
      printf("Radio selected: Choice A\n");
    }
    if (stygian_radio_button(ctx, font, "Choice B", 60, content_y + 270,
                             &radio_selection, 1)) {
      content_changed = true;
      printf("Radio selected: Choice B\n");
    }
    if (stygian_radio_button(ctx, font, "Choice C", 60, content_y + 300,
                             &radio_selection, 2)) {
      content_changed = true;
      printf("Radio selected: Choice C\n");
    }

    // Slider section (always render)
    if (font) {
      stygian_text(ctx, font, "Slider:", 40, content_y + 350, 18.0f, 0.8f, 0.8f,
                   0.8f, 1.0f);
    }

    if (stygian_slider(ctx, 60, content_y + 380, 300, 20, &slider_value, 0.0f,
                       1.0f)) {
      content_changed = true;
      printf("Slider value: %.2f\n", slider_value);
    }

    if (font) {
      char slider_text[64];
      snprintf(slider_text, sizeof(slider_text), "Value: %.2f", slider_value);
      stygian_text(ctx, font, slider_text, 370, content_y + 380, 14.0f, 0.7f,
                   0.7f, 0.7f, 1.0f);

      // Instructions
      stygian_text(ctx, font, "Tab Instructions:", 500, content_y + 70, 18.0f,
                   0.8f, 0.8f, 0.8f, 1.0f);
      stygian_text(ctx, font, "- Click to switch tabs", 520, content_y + 100,
                   14.0f, 0.6f, 0.6f, 0.6f, 1.0f);
      stygian_text(ctx, font, "- Drag to reorder tabs", 520, content_y + 120,
                   14.0f, 0.6f, 0.6f, 0.6f, 1.0f);
      stygian_text(ctx, font, "- Click X to close (except 'Widgets')", 520,
                   content_y + 140, 14.0f, 0.6f, 0.6f, 0.6f, 1.0f);

      // State display
      stygian_text(ctx, font, "Current State:", 500, content_y + 190, 18.0f,
                   0.8f, 0.8f, 0.8f, 1.0f);

      char state[256];
      snprintf(state, sizeof(state), "Checkboxes: %s, %s, %s",
               checkbox1 ? "ON" : "OFF", checkbox2 ? "ON" : "OFF",
               checkbox3 ? "ON" : "OFF");
      stygian_text(ctx, font, state, 520, content_y + 220, 14.0f, 0.7f, 0.7f,
                   0.7f, 1.0f);

      const char *radio_labels[] = {"Choice A", "Choice B", "Choice C"};
      snprintf(state, sizeof(state), "Radio: %s",
               radio_labels[radio_selection]);
      stygian_text(ctx, font, state, 520, content_y + 245, 14.0f, 0.7f, 0.7f,
                   0.7f, 1.0f);

      // Custom scroll area using shared vertical scrollbar widget
      {
        float vx = 500.0f;
        float vy = content_y + 280.0f;
        float vw = 360.0f;
        float vh = 220.0f;
        float content_h = 72.0f * 18.0f;
        float max_scroll = content_h - vh;
        float wheel_dy = stygian_widgets_scroll_dy();
        float prev_scroll = custom_scroll;
        float line_y = vy + 8.0f - custom_scroll;
        int item;

        if (max_scroll < 0.0f)
          max_scroll = 0.0f;
        if (max_scroll > 0.0f) {
          stygian_widgets_register_region(vx, vy, vw, vh,
                                          STYGIAN_WIDGET_REGION_SCROLL);
        }
        if (max_scroll > 0.0f && wheel_dy != 0.0f && mx >= (int)vx &&
            mx <= (int)(vx + vw) && my >= (int)vy && my <= (int)(vy + vh)) {
          custom_scroll -= wheel_dy * 24.0f;
          if (custom_scroll < 0.0f)
            custom_scroll = 0.0f;
          if (custom_scroll > max_scroll)
            custom_scroll = max_scroll;
        }

        stygian_rect_rounded(ctx, vx, vy, vw, vh, 0.11f, 0.11f, 0.13f, 1.0f,
                             6.0f);
        stygian_clip_push(ctx, vx + 8.0f, vy + 8.0f, vw - 22.0f, vh - 16.0f);
        for (item = 0; item < 18; item++) {
          char line[64];
          snprintf(line, sizeof(line), "Scrollable item %d", item + 1);
          stygian_text(ctx, font, line, vx + 14.0f, line_y, 14.0f, 0.8f, 0.84f,
                       0.9f, 1.0f);
          line_y += 72.0f;
        }
        stygian_clip_pop(ctx);
        if (stygian_scrollbar_v(ctx, vx + vw - 10.0f, vy + 6.0f, 6.0f,
                                vh - 12.0f, content_h, &custom_scroll)) {
          content_changed = true;
        }
        if (custom_scroll != prev_scroll) {
          content_changed = true;
        }
      }
    }

    stygian_panel_end(ctx);
    stygian_scope_end(ctx);
    any_state_changed = chrome_changed || content_changed;
    if (any_state_changed) {
      // Schedule targeted scope rebuilds for the next frame.
      if (chrome_changed)
        stygian_scope_invalidate_next(ctx, k_scope_chrome);
      if (content_changed)
        stygian_scope_invalidate_next(ctx, k_scope_content);
      stygian_set_repaint_source(ctx, "mutation");
      stygian_request_repaint_after_ms(ctx, 0u);
    }

    stygian_scope_begin(ctx, k_scope_perf);
    if (!perf_pos_init) {
      perf.x = (float)width - perf.w - 20.0f;
      perf.y = 44.0f;
      perf_pos_init = true;
    }
    if (perf.x < 8.0f)
      perf.x = 8.0f;
    if (perf.y < 8.0f)
      perf.y = 8.0f;
    if (perf.x + perf.w > (float)width - 8.0f)
      perf.x = (float)width - perf.w - 8.0f;
    if (perf.y + perf.h > (float)height - 8.0f)
      perf.y = (float)height - perf.h - 8.0f;
    stygian_perf_widget(ctx, font, &perf);
    stygian_scope_end(ctx);

    stygian_widgets_commit_regions();
    stygian_end_frame(ctx);

    {
      double now = now_seconds();
      if (now - perf_log_t >= 10.0) {
        printf("[widgets_full_test] draw=%u elems=%u upload=%uB/%ur "
               "cpu(build=%.2f submit=%.2f present=%.2f) repaint=%s\n",
               stygian_get_last_frame_draw_calls(ctx),
               stygian_get_last_frame_element_count(ctx),
               stygian_get_last_frame_upload_bytes(ctx),
               stygian_get_last_frame_upload_ranges(ctx),
               stygian_get_last_frame_build_ms(ctx),
               stygian_get_last_frame_submit_ms(ctx),
               stygian_get_last_frame_present_ms(ctx),
               stygian_get_repaint_source(ctx));
        perf_log_t = now;
      }
    }
  }

  if (font)
    stygian_font_destroy(ctx, font);
  stygian_destroy(ctx);
  stygian_window_destroy(window);

  printf("\n=== Test Complete ===\n");
  printf("Final state:\n");
  printf("  Checkboxes: %d, %d, %d\n", checkbox1, checkbox2, checkbox3);
  printf("  Radio: %d\n", radio_selection);
  printf("  Slider: %.2f\n", slider_value);

  return 0;
}
