#include "../include/stygian.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define HARNESS_MAX_ROWS 2048
static const StygianScopeId k_scope_base = 0x2001ull;
static const StygianScopeId k_scope_perf = 0x2002ull;

static bool g_row_enabled[HARNESS_MAX_ROWS];
static float g_row_weight[HARNESS_MAX_ROWS];

static double now_seconds(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int main(void) {
  int i;
  StygianWindowConfig win_cfg = {
      .title = "Stygian Widgets Stress Harness",
      .width = 1500,
      .height = 900,
      .flags = STYGIAN_WINDOW_OPENGL | STYGIAN_WINDOW_RESIZABLE,
  };
  StygianWindow *win = stygian_window_create(&win_cfg);
  StygianConfig cfg = {0};
  StygianContext *ctx;
  StygianFont font;
  bool running = true;
  float list_scroll_y = 0.0f;
  float rows_f = 600.0f;
  int rows = 600;
  bool show_overlays = true;
  bool show_perf = true;
  bool perf_pos_init = false;
  StygianContextMenu menu = {
      .open = false, .x = 0.0f, .y = 0.0f, .w = 190.0f, .item_h = 28.0f};
  StygianModal modal = {.open = false,
                        .close_on_backdrop = true,
                        .w = 520.0f,
                        .h = 250.0f,
                        .title = "Harness Modal"};
  StygianPerfWidget perf = {
      .x = 0.0f,
      .y = 0.0f,
      .w = 420.0f,
      .h = 220.0f,
      .renderer_name = "OpenGL",
      .enabled = true,
      .show_graph = true,
      .show_input = true,
      .auto_scale_graph = true,
      .history_window = 120u,
      .idle_hz = 30u,
      .active_hz = 60u,
      .text_hz = 5u,
      .max_stress_hz = 120u,
      .stress_mode = false,
      .compact_mode = false,
      .show_memory = true,
      .show_glyphs = true,
      .show_triad = true,
  };
  double perf_log_t = now_seconds();

  if (!win) {
    fprintf(stderr, "[stress] Failed to create window\n");
    return 1;
  }

  cfg.backend = STYGIAN_BACKEND_OPENGL;
  cfg.window = win;
  cfg.max_elements = 65536u;
  ctx = stygian_create(&cfg);
  if (!ctx) {
    fprintf(stderr, "[stress] Failed to create context\n");
    stygian_window_destroy(win);
    return 1;
  }

  font = stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");
  if (!font) {
    fprintf(stderr, "[stress] Warning: default font not loaded\n");
  }

  for (i = 0; i < HARNESS_MAX_ROWS; ++i) {
    g_row_enabled[i] = (i % 3) == 0;
    g_row_weight[i] = (float)(i % 100) / 100.0f;
  }

  while (running && !stygian_window_should_close(win)) {
    StygianEvent ev;
    bool event_mutated = false;
    bool event_requested = false;
    bool ui_state_changed = false;
    bool repaint_pending;
    uint32_t wait_ms = stygian_next_repaint_wait_ms(ctx, 250u);
    static bool first_frame = true;
    int ww, wh;
    int mx, my;
    float panel_x = 20.0f;
    float panel_y = 100.0f;
    float panel_w;
    float panel_h;
    float row_h = 32.0f;
    float content_h;
    float max_scroll;
    float prev_scroll_y;
    float wheel_dy;
    int start_row;
    int end_row;
    bool panel_hovered;

    stygian_widgets_begin_frame(ctx);
    while (stygian_window_poll_event(win, &ev)) {
      StygianWidgetEventImpact impact =
          stygian_widgets_process_event_ex(ctx, &ev);
      if (impact & STYGIAN_IMPACT_MUTATED_STATE)
        event_mutated = true;
      if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
        event_requested = true;
      if (ev.type == STYGIAN_EVENT_CLOSE)
        running = false;
    }

    if (!event_mutated && !event_requested && !first_frame) {
      if (stygian_window_wait_event_timeout(win, &ev, wait_ms)) {
        StygianWidgetEventImpact impact =
            stygian_widgets_process_event_ex(ctx, &ev);
        if (impact & STYGIAN_IMPACT_MUTATED_STATE)
          event_mutated = true;
        if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
          event_requested = true;
        if (ev.type == STYGIAN_EVENT_CLOSE)
          running = false;
        while (stygian_window_poll_event(win, &ev)) {
          StygianWidgetEventImpact queued_impact =
              stygian_widgets_process_event_ex(ctx, &ev);
          if (queued_impact & STYGIAN_IMPACT_MUTATED_STATE)
            event_mutated = true;
          if (queued_impact & STYGIAN_IMPACT_REQUEST_REPAINT)
            event_requested = true;
          if (ev.type == STYGIAN_EVENT_CLOSE)
            running = false;
        }
      }
    }

    repaint_pending = stygian_has_pending_repaint(ctx);
    if (!event_mutated && !event_requested && !first_frame && !repaint_pending) {
      continue;
    }
    if (event_mutated) {
      // Event-driven mutations must rebuild this scope now, not next frame.
      stygian_scope_invalidate_now(ctx, k_scope_base);
      stygian_set_repaint_source(ctx, "event-mutation");
    }
    first_frame = false;

    stygian_window_get_size(win, &ww, &wh);
    stygian_mouse_pos(win, &mx, &my);
    panel_w = (float)ww - 40.0f;
    panel_h = (float)wh - panel_y - 20.0f;
    content_h = (float)rows * row_h + 8.0f;
    max_scroll = content_h - panel_h;
    if (max_scroll < 0.0f)
      max_scroll = 0.0f;
    wheel_dy = stygian_widgets_scroll_dy();
    panel_hovered = (mx >= (int)panel_x && mx <= (int)(panel_x + panel_w) &&
                     my >= (int)panel_y && my <= (int)(panel_y + panel_h));
    if (max_scroll > 0.0f) {
      stygian_widgets_register_region(panel_x, panel_y, panel_w, panel_h,
                                      STYGIAN_WIDGET_REGION_SCROLL);
    }
    prev_scroll_y = list_scroll_y;
    if (max_scroll > 0.0f && panel_hovered && wheel_dy != 0.0f) {
      list_scroll_y -= wheel_dy * 24.0f;
      if (list_scroll_y < 0.0f)
        list_scroll_y = 0.0f;
      if (list_scroll_y > max_scroll)
        list_scroll_y = max_scroll;
    }
    if (list_scroll_y != prev_scroll_y) {
      ui_state_changed = true;
    }

    if (show_perf && repaint_pending) {
      stygian_scope_invalidate_now(ctx, k_scope_perf);
    }
    stygian_begin_frame(ctx, ww, wh);
    stygian_scope_begin(ctx, k_scope_base);

    stygian_rect(ctx, 0.0f, 0.0f, (float)ww, (float)wh, 0.07f, 0.08f, 0.10f,
                 1.0f);
    stygian_rect_rounded(ctx, 14.0f, 14.0f, (float)ww - 28.0f, 72.0f, 0.11f,
                         0.12f, 0.15f, 0.95f, 8.0f);

    if (font) {
      stygian_text(
          ctx, font,
          "Stress Harness - Tab/Shift+Tab navigation, right-click menu, "
          "modal, tooltip, scrollbar",
          26.0f, 20.0f, 15.0f, 0.93f, 0.95f, 0.98f, 1.0f);
    }

    if (stygian_button(ctx, font, show_perf ? "Perf: ON" : "Perf: OFF", 26.0f,
                       46.0f, 110.0f, 28.0f)) {
      show_perf = !show_perf;
      ui_state_changed = true;
    }
    if (stygian_button(ctx, font,
                       show_overlays ? "Overlays: ON" : "Overlays: OFF", 144.0f,
                       46.0f, 130.0f, 28.0f)) {
      show_overlays = !show_overlays;
      ui_state_changed = true;
    }
    if (stygian_button(ctx, font, "Open Modal", 282.0f, 46.0f, 120.0f, 28.0f)) {
      modal.open = true;
      ui_state_changed = true;
    }

    if (stygian_slider(ctx, 420.0f, 52.0f, 240.0f, 18.0f, &rows_f, 64.0f,
                       (float)HARNESS_MAX_ROWS)) {
      rows = (int)rows_f;
      if (rows < 64)
        rows = 64;
      if (rows > HARNESS_MAX_ROWS)
        rows = HARNESS_MAX_ROWS;
      ui_state_changed = true;
    }
    if (font) {
      char rows_text[64];
      snprintf(rows_text, sizeof(rows_text), "Rows: %d", rows);
      stygian_text(ctx, font, rows_text, 668.0f, 49.0f, 14.0f, 0.84f, 0.90f,
                   0.95f, 1.0f);
    }

    if (stygian_context_menu_trigger_region(ctx, &menu, panel_x, panel_y,
                                            panel_w, panel_h)) {
      ui_state_changed = true;
    }
    stygian_rect_rounded(ctx, panel_x, panel_y, panel_w, panel_h, 0.10f, 0.11f,
                         0.13f, 0.94f, 8.0f);
    stygian_clip_push(ctx, panel_x + 8.0f, panel_y + 8.0f, panel_w - 24.0f,
                      panel_h - 16.0f);

    start_row = (int)(list_scroll_y / row_h);
    if (start_row < 0)
      start_row = 0;
    end_row = start_row + (int)((panel_h + row_h) / row_h) + 1;
    if (end_row > rows)
      end_row = rows;

    for (i = start_row; i < end_row; ++i) {
      float ry = panel_y + 8.0f + (float)i * row_h - list_scroll_y;
      char id_text[64];

      stygian_rect(ctx, panel_x + 8.0f, ry, panel_w - 26.0f, row_h - 2.0f,
                   0.16f + ((i % 2) ? 0.02f : 0.0f),
                   0.17f + ((i % 2) ? 0.02f : 0.0f), 0.20f, 0.86f);

      snprintf(id_text, sizeof(id_text), "Row %d", i);
      if (stygian_checkbox(ctx, font, id_text, panel_x + 14.0f, ry + 6.0f,
                           &g_row_enabled[i])) {
        ui_state_changed = true;
      }
      if (stygian_slider(ctx, panel_x + 210.0f, ry + 8.0f, 220.0f, 14.0f,
                         &g_row_weight[i], 0.0f, 1.0f)) {
        ui_state_changed = true;
      }
      if (stygian_button(ctx, font, "Ping", panel_x + 450.0f, ry + 4.0f, 64.0f,
                         22.0f)) {
        printf("[stress] Ping row=%d enabled=%d weight=%.3f\n", i,
               g_row_enabled[i] ? 1 : 0, g_row_weight[i]);
      }
    }

    stygian_clip_pop(ctx);
    if (stygian_scrollbar_v(ctx, panel_x + panel_w - 11.0f, panel_y + 6.0f,
                            7.0f, panel_h - 12.0f, content_h, &list_scroll_y)) {
      ui_state_changed = true;
    }

    if (show_overlays && panel_hovered && font) {
      StygianTooltip tip = {
          .text = "Right-click: context menu | Tab: keyboard navigation",
          .x = (float)mx,
          .y = (float)my,
          .max_w = 380.0f,
          .show = true,
      };
      stygian_tooltip(ctx, font, &tip);
    }

    if (stygian_context_menu_begin(ctx, font, &menu, 3)) {
      if (stygian_context_menu_item(ctx, font, &menu, "Open modal", 0)) {
        modal.open = true;
        ui_state_changed = true;
      }
      if (stygian_context_menu_item(ctx, font, &menu, "Reset scroll", 1)) {
        list_scroll_y = 0.0f;
        ui_state_changed = true;
      }
      if (stygian_context_menu_item(ctx, font, &menu, "Randomize weights", 2)) {
        for (i = 0; i < rows; ++i) {
          uint32_t seed = (uint32_t)(i * 2654435761u + 0x9e3779b9u);
          g_row_weight[i] = (float)(seed % 1000u) / 1000.0f;
        }
        ui_state_changed = true;
      }
      stygian_context_menu_end(ctx, &menu);
    }

    if (modal.open) {
      if (stygian_modal_begin(ctx, font, &modal, (float)ww, (float)wh)) {
        float mx0 = ((float)ww - modal.w) * 0.5f;
        float my0 = ((float)wh - modal.h) * 0.5f;
        if (font) {
          stygian_text(ctx, font, "Modal content area", mx0 + 18.0f,
                       my0 + 46.0f, 16.0f, 0.90f, 0.93f, 0.97f, 1.0f);
          stygian_text(ctx, font, "This validates clip stacking + focus flow.",
                       mx0 + 18.0f, my0 + 68.0f, 14.0f, 0.78f, 0.84f, 0.92f,
                       1.0f);
        }
        if (stygian_button(ctx, font, "Close", mx0 + modal.w - 94.0f,
                           my0 + modal.h - 42.0f, 74.0f, 28.0f)) {
          modal.open = false;
          ui_state_changed = true;
        }
        stygian_modal_end(ctx, &modal);
      }
    }

    stygian_scope_end(ctx);
    if (ui_state_changed && !event_mutated) {
      // Programmatic mutations (non-input) schedule a rebuild on next frame.
      stygian_scope_invalidate_next(ctx, k_scope_base);
      stygian_set_repaint_source(ctx, "mutation");
      stygian_request_repaint_after_ms(ctx, 1u);
    }

    if (show_perf) {
      stygian_scope_begin(ctx, k_scope_perf);
      if (!perf_pos_init) {
        perf.x = (float)ww - perf.w - 18.0f;
        perf.y = 22.0f;
        perf_pos_init = true;
      }
      if (perf.x < 8.0f)
        perf.x = 8.0f;
      if (perf.y < 8.0f)
        perf.y = 8.0f;
      if (perf.x + perf.w > (float)ww - 8.0f)
        perf.x = (float)ww - perf.w - 8.0f;
      if (perf.y + perf.h > (float)wh - 8.0f)
        perf.y = (float)wh - perf.h - 8.0f;
      stygian_perf_widget(ctx, font, &perf);
      stygian_scope_end(ctx);
    }

    stygian_widgets_commit_regions();
    stygian_end_frame(ctx);

    {
      double now = now_seconds();
      if (now - perf_log_t >= 10.0) {
        printf("[widgets_stress] draw=%u elems=%u upload=%uB/%ur "
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
  stygian_window_destroy(win);
  return 0;
}
