#include "../include/stygian.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include "mini_perf_harness.h"
#include <stdio.h>
#include <string.h>

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_MINI_BACKEND STYGIAN_BACKEND_VULKAN
#define STYGIAN_MINI_WINDOW_FLAGS                                               \
  (STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_VULKAN)
#define STYGIAN_MINI_RENDERER_NAME "Vulkan"
#else
#define STYGIAN_MINI_BACKEND STYGIAN_BACKEND_OPENGL
#define STYGIAN_MINI_WINDOW_FLAGS                                               \
  (STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_OPENGL)
#define STYGIAN_MINI_RENDERER_NAME "OpenGL"
#endif

int main(void) {
  const StygianScopeId k_scope_chrome = 0x4201u;
  const StygianScopeId k_scope_month_grid = 0x4202u;
  const StygianScopeId k_scope_badges = 0x4203u;
  const StygianScopeId k_scope_popover =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4204u;
  const StygianScopeId k_scope_perf =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4205u;

  StygianWindowConfig win_cfg = {
      .title = "Stygian Calendar Mini",
      .width = 1080,
      .height = 760,
      .flags = STYGIAN_MINI_WINDOW_FLAGS,
  };
  StygianWindow *window = stygian_window_create(&win_cfg);
  if (!window)
    return 1;

  StygianConfig cfg = {.backend = STYGIAN_MINI_BACKEND, .window = window};
  StygianContext *ctx = stygian_create(&cfg);
  if (!ctx)
    return 1;

  StygianFont font =
      stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");
  StygianMiniPerfHarness perf;
  bool first_frame = true;
  bool show_perf = true;
  int selected_day = 0;
  bool popover_open = false;
  int month_offset = 0;
  bool day_has_event[32];

  memset(day_has_event, 0, sizeof(day_has_event));
  for (int i = 1; i <= 31; i++) {
    day_has_event[i] = (i % 3 == 0 || i % 5 == 0);
  }
  stygian_mini_perf_init(&perf, "calendar_mini");
  perf.widget.renderer_name = STYGIAN_MINI_RENDERER_NAME;

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    bool event_mutated = false;
    bool event_requested = false;
    bool event_eval_requested = false;
    bool chrome_changed = false;
    bool grid_changed = false;
    bool badge_changed = false;
    bool popover_changed = false;
    uint32_t wait_ms = stygian_next_repaint_wait_ms(ctx, 250u);

    stygian_widgets_begin_frame(ctx);

    while (stygian_window_poll_event(window, &event)) {
      StygianWidgetEventImpact impact =
          stygian_widgets_process_event_ex(ctx, &event);
      if (impact & STYGIAN_IMPACT_MUTATED_STATE)
        event_mutated = true;
      if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
        event_requested = true;
      if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
        event_eval_requested = true;
      if (event.type == STYGIAN_EVENT_CLOSE)
        stygian_window_request_close(window);
    }

    if (!first_frame && !event_mutated && !event_requested &&
        !event_eval_requested) {
      if (stygian_window_wait_event_timeout(window, &event, wait_ms)) {
        StygianWidgetEventImpact impact =
            stygian_widgets_process_event_ex(ctx, &event);
        if (impact & STYGIAN_IMPACT_MUTATED_STATE)
          event_mutated = true;
        if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
          event_requested = true;
        if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
          event_eval_requested = true;
        if (event.type == STYGIAN_EVENT_CLOSE)
          stygian_window_request_close(window);
      }
    }

    {
      bool repaint_pending = stygian_has_pending_repaint(ctx);
      bool render_frame = first_frame || event_mutated || repaint_pending;
      bool eval_only_frame =
          (!render_frame && (event_eval_requested || event_requested));
      int width, height;
      float grid_x = 24.0f;
      float grid_y = 84.0f;
      float grid_w = 760.0f;
      float grid_h = 620.0f;
      float cell_w;
      float cell_h;

      if (!render_frame && !eval_only_frame)
        continue;
      first_frame = false;

      stygian_window_get_size(window, &width, &height);
      cell_w = (grid_w - 16.0f) / 7.0f;
      cell_h = (grid_h - 24.0f) / 6.0f;

      stygian_begin_frame_intent(
          ctx, width, height,
          eval_only_frame ? STYGIAN_FRAME_EVAL_ONLY : STYGIAN_FRAME_RENDER);

      stygian_scope_begin(ctx, k_scope_chrome);
      stygian_rect(ctx, 0.0f, 0.0f, (float)width, (float)height, 0.08f, 0.09f,
                   0.11f, 1.0f);
      stygian_rect(ctx, 0.0f, 0.0f, (float)width, 52.0f, 0.11f, 0.13f, 0.16f,
                   1.0f);
      if (font) {
        char month_label[64];
        snprintf(month_label, sizeof(month_label), "Stygian Calendar Mini  (M%+d)",
                 month_offset);
        stygian_text(ctx, font, month_label, 14.0f, 15.0f, 17.0f, 0.94f, 0.95f,
                     0.98f, 1.0f);
      }
      if (stygian_button(ctx, font, "<", 420.0f, 11.0f, 34.0f, 30.0f)) {
        month_offset--;
        chrome_changed = true;
        grid_changed = true;
      }
      if (stygian_button(ctx, font, ">", 460.0f, 11.0f, 34.0f, 30.0f)) {
        month_offset++;
        chrome_changed = true;
        grid_changed = true;
      }
      if (stygian_button(ctx, font, show_perf ? "Perf: ON" : "Perf: OFF",
                         (float)width - 120.0f, 12.0f, 104.0f, 28.0f)) {
        show_perf = !show_perf;
        chrome_changed = true;
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_month_grid);
      stygian_rect_rounded(ctx, grid_x, grid_y, grid_w, grid_h, 0.13f, 0.15f,
                           0.18f, 1.0f, 8.0f);
      for (int d = 1; d <= 31; d++) {
        int row = (d - 1) / 7;
        int col = (d - 1) % 7;
        float cx = grid_x + 8.0f + col * cell_w;
        float cy = grid_y + 14.0f + row * cell_h;
        float cw = cell_w - 6.0f;
        float ch = cell_h - 8.0f;
        char label[8];
        snprintf(label, sizeof(label), "%d", d);
        if (d == selected_day) {
          stygian_rect_rounded(ctx, cx, cy, cw, ch, 0.2f, 0.34f, 0.52f, 0.95f,
                               6.0f);
        } else {
          stygian_rect_rounded(ctx, cx, cy, cw, ch, 0.1f, 0.12f, 0.15f, 1.0f,
                               5.0f);
        }
        if (stygian_button(ctx, font, label, cx + 8.0f, cy + 8.0f, 32.0f,
                           24.0f)) {
          selected_day = d;
          popover_open = true;
          grid_changed = true;
          popover_changed = true;
        }
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_badges);
      for (int d = 1; d <= 31; d++) {
        int row = (d - 1) / 7;
        int col = (d - 1) % 7;
        float cx = grid_x + 8.0f + col * cell_w;
        float cy = grid_y + 14.0f + row * cell_h;
        float cw = cell_w - 6.0f;
        if (day_has_event[d]) {
          stygian_rect(ctx, cx + cw - 16.0f, cy + 10.0f, 8.0f, 8.0f, 0.28f,
                       0.78f, 0.43f, 1.0f);
        }
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_popover);
      if (popover_open && selected_day > 0) {
        float px = grid_x + grid_w + 18.0f;
        float py = grid_y + 12.0f;
        stygian_rect_rounded(ctx, px, py, 250.0f, 180.0f, 0.12f, 0.14f, 0.17f,
                             1.0f, 8.0f);
        if (font) {
          char title[64];
          snprintf(title, sizeof(title), "Day %d details", selected_day);
          stygian_text(ctx, font, title, px + 14.0f, py + 18.0f, 16.0f, 0.94f,
                       0.95f, 0.98f, 1.0f);
          stygian_text(ctx, font,
                       day_has_event[selected_day] ? "Event badge: active"
                                                   : "Event badge: none",
                       px + 14.0f, py + 50.0f, 14.0f, 0.77f, 0.82f, 0.9f, 1.0f);
        }
        if (stygian_button(ctx, font, "Close", px + 14.0f, py + 130.0f, 90.0f,
                           30.0f)) {
          popover_open = false;
          popover_changed = true;
        }
      }
      stygian_scope_end(ctx);

      if (show_perf) {
        stygian_scope_begin(ctx, k_scope_perf);
        stygian_mini_perf_draw(ctx, font, &perf, width, height);
        stygian_scope_end(ctx);
      }

      if (chrome_changed)
        stygian_scope_invalidate_next(ctx, k_scope_chrome);
      if (grid_changed || event_mutated)
        stygian_scope_invalidate_next(ctx, k_scope_month_grid);
      if (badge_changed)
        stygian_scope_invalidate_next(ctx, k_scope_badges);
      if (popover_changed)
        stygian_scope_invalidate_next(ctx, k_scope_popover);
      if (!show_perf)
        stygian_scope_invalidate_next(ctx, k_scope_perf);

      if (chrome_changed || grid_changed || badge_changed || popover_changed ||
          event_mutated) {
        stygian_set_repaint_source(ctx, "mutation");
        stygian_request_repaint_after_ms(ctx, 0u);
      }

      stygian_widgets_commit_regions();
      stygian_end_frame(ctx);
      stygian_mini_perf_accumulate(&perf, eval_only_frame);
      stygian_mini_perf_log(ctx, &perf);
    }
  }

  if (font)
    stygian_font_destroy(ctx, font);
  stygian_destroy(ctx);
  stygian_window_destroy(window);
  return 0;
}
