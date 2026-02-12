#include "mini_perf_harness.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static double stygian_now_seconds(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void stygian_mini_perf_init(StygianMiniPerfHarness *perf, const char *name) {
  if (!perf)
    return;
  memset(perf, 0, sizeof(*perf));
  perf->name = name;
  perf->last_log_seconds = stygian_now_seconds();
  perf->widget.enabled = true;
  perf->widget.show_graph = true;
  perf->widget.show_input = true;
  perf->widget.auto_scale_graph = false;
  perf->widget.history_window = 120u;
  perf->widget.idle_hz = 30u;
  perf->widget.active_hz = 30u;
  perf->widget.text_hz = 5u;
  perf->widget.graph_max_segments = 64u;
  perf->widget.max_stress_hz = 120u;
  perf->widget.stress_mode = false;
  perf->widget.compact_mode = false;
  perf->widget.show_memory = true;
  perf->widget.show_glyphs = true;
  perf->widget.show_triad = true;
}

void stygian_mini_perf_accumulate(StygianMiniPerfHarness *perf, bool eval_only) {
  if (!perf)
    return;
  if (eval_only)
    perf->eval_frames++;
  else
    perf->render_frames++;
}

void stygian_mini_perf_draw(StygianContext *ctx, StygianFont font,
                            StygianMiniPerfHarness *perf, int width,
                            int height) {
  if (!ctx || !perf)
    return;
  if (!perf->pos_initialized) {
    perf->widget.w = 360.0f;
    perf->widget.h = 230.0f;
    perf->widget.x = (float)width - perf->widget.w - 12.0f;
    perf->widget.y = 44.0f;
    perf->pos_initialized = true;
  }
  if (perf->widget.x < 8.0f)
    perf->widget.x = 8.0f;
  if (perf->widget.y < 8.0f)
    perf->widget.y = 8.0f;
  if (perf->widget.x + perf->widget.w > (float)width - 8.0f)
    perf->widget.x = (float)width - perf->widget.w - 8.0f;
  if (perf->widget.y + perf->widget.h > (float)height - 8.0f)
    perf->widget.y = (float)height - perf->widget.h - 8.0f;
  stygian_perf_widget(ctx, font, &perf->widget);
}

void stygian_mini_perf_log(StygianContext *ctx, StygianMiniPerfHarness *perf) {
  double now;
  double dt;
  if (!ctx || !perf)
    return;
  now = stygian_now_seconds();
  dt = now - perf->last_log_seconds;
  if (dt < 10.0)
    return;
  printf("[%s] render=%u eval=%u draw=%u elems=%u upload=%uB/%ur "
         "cpu(build=%.2f submit=%.2f present=%.2f) gpu=%.3fms reason=0x%x\n",
         perf->name ? perf->name : "mini", perf->render_frames,
         perf->eval_frames, stygian_get_last_frame_draw_calls(ctx),
         stygian_get_last_frame_element_count(ctx),
         stygian_get_last_frame_upload_bytes(ctx),
         stygian_get_last_frame_upload_ranges(ctx),
         stygian_get_last_frame_build_ms(ctx),
         stygian_get_last_frame_submit_ms(ctx),
         stygian_get_last_frame_present_ms(ctx), stygian_get_last_frame_gpu_ms(ctx),
         stygian_get_last_frame_reason_flags(ctx));
  perf->render_frames = 0u;
  perf->eval_frames = 0u;
  perf->last_log_seconds = now;
}
