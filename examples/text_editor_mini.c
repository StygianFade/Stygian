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
  const StygianScopeId k_scope_chrome = 0x4001u;
  const StygianScopeId k_scope_gutter = 0x4002u;
  const StygianScopeId k_scope_rows = 0x4003u;
  const StygianScopeId k_scope_caret =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4004u;
  const StygianScopeId k_scope_selection =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4005u;
  const StygianScopeId k_scope_perf =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4006u;

  StygianWindowConfig win_cfg = {
      .title = "Stygian Text Editor Mini",
      .width = 1280,
      .height = 800,
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
  char editor_buffer[32768];
  StygianTextArea editor_state;
  bool first_frame = true;
  bool show_perf = true;

  stygian_mini_perf_init(&perf, "text_editor_mini");
  perf.widget.renderer_name = STYGIAN_MINI_RENDERER_NAME;
  memset(editor_buffer, 0, sizeof(editor_buffer));
  snprintf(editor_buffer, sizeof(editor_buffer),
           "// Stygian mini editor\n"
           "int main(void) {\n"
           "  return 0;\n"
           "}\n");
  memset(&editor_state, 0, sizeof(editor_state));
  editor_state.buffer = editor_buffer;
  editor_state.buffer_size = (int)sizeof(editor_buffer);
  editor_state.cursor_idx = (int)strlen(editor_buffer);
  editor_state.selection_start = editor_state.cursor_idx;
  editor_state.selection_end = editor_state.cursor_idx;

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    bool event_mutated = false;
    bool event_requested = false;
    bool event_eval_requested = false;
    bool chrome_changed = false;
    bool rows_changed = false;
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
      float content_x;
      float content_y;
      float content_w;
      float content_h;

      if (!render_frame && !eval_only_frame)
        continue;
      first_frame = false;

      stygian_window_get_size(window, &width, &height);
      stygian_begin_frame_intent(
          ctx, width, height,
          eval_only_frame ? STYGIAN_FRAME_EVAL_ONLY : STYGIAN_FRAME_RENDER);

      stygian_scope_begin(ctx, k_scope_chrome);
      stygian_rect(ctx, 0.0f, 0.0f, (float)width, 40.0f, 0.08f, 0.1f, 0.13f,
                   1.0f);
      if (font) {
        stygian_text(ctx, font, "Stygian Text Editor Mini", 14.0f, 10.0f, 16.0f,
                     0.95f, 0.95f, 0.98f, 1.0f);
      }
      if (stygian_button(ctx, font, show_perf ? "Perf: ON" : "Perf: OFF",
                         (float)width - 120.0f, 6.0f, 104.0f, 28.0f)) {
        show_perf = !show_perf;
        chrome_changed = true;
      }
      stygian_scope_end(ctx);

      content_x = 0.0f;
      content_y = 40.0f;
      content_w = (float)width;
      content_h = (float)height - 40.0f;

      stygian_scope_begin(ctx, k_scope_gutter);
      stygian_rect(ctx, content_x, content_y, 64.0f, content_h, 0.09f, 0.11f,
                   0.14f, 1.0f);
      if (font) {
        for (int line = 0; line < 40; line++) {
          char label[16];
          snprintf(label, sizeof(label), "%d", line + 1);
          stygian_text(ctx, font, label, 10.0f, content_y + 8.0f + line * 18.0f,
                       13.0f, 0.55f, 0.62f, 0.72f, 1.0f);
        }
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_rows);
      editor_state.x = 64.0f;
      editor_state.y = content_y + 6.0f;
      editor_state.w = content_w - 72.0f;
      editor_state.h = content_h - 12.0f;
      if (stygian_text_area(ctx, font, &editor_state)) {
        rows_changed = true;
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_selection);
      if (editor_state.focused && editor_state.selection_start !=
                                     editor_state.selection_end) {
        stygian_rect(ctx, editor_state.x + 6.0f, editor_state.y + 6.0f,
                     editor_state.w - 12.0f, 2.0f, 0.35f, 0.52f, 0.78f, 0.45f);
      }
      stygian_scope_end(ctx);

      stygian_scope_begin(ctx, k_scope_caret);
      if (editor_state.focused) {
        stygian_rect(ctx, editor_state.x + editor_state.w - 4.0f,
                     editor_state.y + editor_state.h - 20.0f, 2.0f, 14.0f,
                     0.95f, 0.95f, 0.95f, 1.0f);
      }
      stygian_scope_end(ctx);

      if (show_perf) {
        stygian_scope_begin(ctx, k_scope_perf);
        stygian_mini_perf_draw(ctx, font, &perf, width, height);
        stygian_scope_end(ctx);
      }

      if (chrome_changed) {
        stygian_scope_invalidate_next(ctx, k_scope_chrome);
      }
      if (rows_changed || event_mutated) {
        stygian_scope_invalidate_next(ctx, k_scope_rows);
      }
      if (!show_perf) {
        stygian_scope_invalidate_next(ctx, k_scope_perf);
      }

      if (chrome_changed || rows_changed || event_mutated) {
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
