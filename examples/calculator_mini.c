#include "../include/stygian.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include "mini_perf_harness.h"
#include <math.h>
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

static double calc_parse_display(const char *display) {
  double value = 0.0;
  if (!display || display[0] == '\0')
    return 0.0;
  sscanf(display, "%lf", &value);
  return value;
}

static void calc_push_history(char history[8][64], int *count,
                              const char *line) {
  int i;
  if (!count || !line)
    return;
  if (*count < 8) {
    snprintf(history[*count], 64, "%s", line);
    (*count)++;
    return;
  }
  for (i = 1; i < 8; i++) {
    snprintf(history[i - 1], 64, "%s", history[i]);
  }
  snprintf(history[7], 64, "%s", line);
}

static void calc_apply_op(double *accum, char op, double rhs) {
  if (!accum)
    return;
  switch (op) {
  case '+':
    *accum += rhs;
    break;
  case '-':
    *accum -= rhs;
    break;
  case '*':
    *accum *= rhs;
    break;
  case '/':
    *accum = (fabs(rhs) < 0.0000001) ? *accum : (*accum / rhs);
    break;
  default:
    *accum = rhs;
    break;
  }
}

int main(void) {
  const StygianScopeId k_scope_chrome = 0x4101u;
  const StygianScopeId k_scope_display = 0x4102u;
  const StygianScopeId k_scope_keypad = 0x4103u;
  const StygianScopeId k_scope_history = 0x4104u;
  const StygianScopeId k_scope_perf =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4105u;

  StygianWindowConfig win_cfg = {
      .title = "Stygian Calculator Mini",
      .width = 980,
      .height = 720,
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
  char display[64] = "0";
  char history[8][64];
  int history_count = 0;
  double accumulator = 0.0;
  char pending_op = 0;
  bool clear_on_digit = true;

  const char *labels[20] = {"7", "8", "9", "/", "4", "5", "6", "*", "1", "2",
                            "3", "-", "0", ".", "C", "+", "=", "±", "%", "AC"};

  memset(history, 0, sizeof(history));
  stygian_mini_perf_init(&perf, "calculator_mini");
  perf.widget.renderer_name = STYGIAN_MINI_RENDERER_NAME;

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    bool event_mutated = false;
    bool event_requested = false;
    bool event_eval_requested = false;
    bool chrome_changed = false;
    bool display_changed = false;
    bool keypad_changed = false;
    bool history_changed = false;
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
      float panel_x = 24.0f;
      float panel_y = 60.0f;
      float panel_w = 520.0f;
      float panel_h = 620.0f;
      float btn_w = 116.0f;
      float btn_h = 64.0f;

      if (!render_frame && !eval_only_frame)
        continue;
      first_frame = false;

      stygian_window_get_size(window, &width, &height);
      stygian_begin_frame_intent(
          ctx, width, height,
          eval_only_frame ? STYGIAN_FRAME_EVAL_ONLY : STYGIAN_FRAME_RENDER);

      stygian_scope_begin(ctx, k_scope_chrome);
      stygian_rect(ctx, 0.0f, 0.0f, (float)width, (float)height, 0.08f, 0.09f,
                   0.11f, 1.0f);
      stygian_rect(ctx, 0.0f, 0.0f, (float)width, 42.0f, 0.1f, 0.12f, 0.14f,
                   1.0f);
      if (font) {
        stygian_text(ctx, font, "Stygian Calculator Mini", 14.0f, 11.0f, 15.0f,
                     0.93f, 0.94f, 0.98f, 1.0f);
      }
      if (stygian_button(ctx, font, show_perf ? "Perf: ON" : "Perf: OFF",
                         (float)width - 120.0f, 7.0f, 104.0f, 28.0f)) {
        show_perf = !show_perf;
        chrome_changed = true;
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_display);
      stygian_rect_rounded(ctx, panel_x, panel_y, panel_w, panel_h, 0.13f, 0.15f,
                           0.18f, 1.0f, 10.0f);
      stygian_rect_rounded(ctx, panel_x + 12.0f, panel_y + 12.0f, panel_w - 24.0f,
                           92.0f, 0.1f, 0.12f, 0.15f, 1.0f, 6.0f);
      if (font) {
        stygian_text(ctx, font, display, panel_x + 20.0f, panel_y + 43.0f, 34.0f,
                     0.95f, 0.95f, 0.98f, 1.0f);
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_keypad);
      for (int i = 0; i < 20; i++) {
        int row = i / 4;
        int col = i % 4;
        float bx = panel_x + 16.0f + col * (btn_w + 8.0f);
        float by = panel_y + 120.0f + row * (btn_h + 8.0f);
        const char *label = labels[i];
        if (stygian_button(ctx, font, label, bx, by, btn_w, btn_h)) {
          keypad_changed = true;
          if ((label[0] >= '0' && label[0] <= '9') || label[0] == '.') {
            if (clear_on_digit || (display[0] == '0' && display[1] == '\0')) {
              display[0] = '\0';
              clear_on_digit = false;
            }
            if (strlen(display) < sizeof(display) - 2) {
              size_t len = strlen(display);
              display[len] = label[0];
              display[len + 1] = '\0';
              display_changed = true;
            }
          } else if (strcmp(label, "C") == 0 || strcmp(label, "AC") == 0) {
            snprintf(display, sizeof(display), "0");
            accumulator = 0.0;
            pending_op = 0;
            clear_on_digit = true;
            display_changed = true;
          } else if (strcmp(label, "±") == 0) {
            double value = -calc_parse_display(display);
            snprintf(display, sizeof(display), "%.10g", value);
            display_changed = true;
          } else if (strcmp(label, "%") == 0) {
            double value = calc_parse_display(display) * 0.01;
            snprintf(display, sizeof(display), "%.10g", value);
            display_changed = true;
          } else if (strcmp(label, "=") == 0) {
            double rhs = calc_parse_display(display);
            char hist[64];
            calc_apply_op(&accumulator, pending_op, rhs);
            snprintf(display, sizeof(display), "%.10g", accumulator);
            snprintf(hist, sizeof(hist), "%c %.6g => %.6g",
                     pending_op ? pending_op : '=', rhs, accumulator);
            calc_push_history(history, &history_count, hist);
            pending_op = 0;
            clear_on_digit = true;
            display_changed = true;
            history_changed = true;
          } else {
            double rhs = calc_parse_display(display);
            if (pending_op == 0) {
              accumulator = rhs;
            } else {
              calc_apply_op(&accumulator, pending_op, rhs);
              snprintf(display, sizeof(display), "%.10g", accumulator);
            }
            pending_op = label[0];
            clear_on_digit = true;
            display_changed = true;
          }
        }
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_history);
      stygian_rect_rounded(ctx, panel_x + panel_w + 16.0f, panel_y, 400.0f,
                           panel_h, 0.13f, 0.15f, 0.18f, 1.0f, 10.0f);
      if (font) {
        stygian_text(ctx, font, "History", panel_x + panel_w + 30.0f,
                     panel_y + 20.0f, 18.0f, 0.88f, 0.9f, 0.95f, 1.0f);
        for (int i = 0; i < history_count; i++) {
          stygian_text(ctx, font, history[i], panel_x + panel_w + 30.0f,
                       panel_y + 54.0f + i * 22.0f, 14.0f, 0.75f, 0.8f, 0.88f,
                       1.0f);
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
      if (display_changed)
        stygian_scope_invalidate_next(ctx, k_scope_display);
      if (keypad_changed || event_mutated)
        stygian_scope_invalidate_next(ctx, k_scope_keypad);
      if (history_changed)
        stygian_scope_invalidate_next(ctx, k_scope_history);
      if (!show_perf)
        stygian_scope_invalidate_next(ctx, k_scope_perf);

      if (chrome_changed || display_changed || keypad_changed || history_changed ||
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
