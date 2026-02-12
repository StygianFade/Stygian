#include "../include/stygian.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include "mini_perf_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_SUITE_BACKEND STYGIAN_BACKEND_VULKAN
#define STYGIAN_SUITE_WINDOW_FLAGS                                              \
  (STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_VULKAN)
#define STYGIAN_SUITE_RENDERER_NAME "vk"
#else
#define STYGIAN_SUITE_BACKEND STYGIAN_BACKEND_OPENGL
#define STYGIAN_SUITE_WINDOW_FLAGS                                              \
  (STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_OPENGL)
#define STYGIAN_SUITE_RENDERER_NAME "gl"
#endif

typedef enum PerfScenario {
  PERF_SCENARIO_IDLE = 0,
  PERF_SCENARIO_OVERLAY = 1,
  PERF_SCENARIO_SPARSE = 2,
  PERF_SCENARIO_CLIP = 3,
  PERF_SCENARIO_SCROLL = 4,
  PERF_SCENARIO_TEXT = 5,
} PerfScenario;

typedef struct PerfIntervalStats {
  uint32_t render_frames;
  uint32_t eval_frames;
  uint64_t samples;
  double sum_gpu_ms;
  double sum_build_ms;
  double sum_submit_ms;
  double sum_present_ms;
  double sum_upload_bytes;
  double sum_upload_ranges;
} PerfIntervalStats;

static double now_seconds(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static const char *scenario_name(PerfScenario scenario) {
  switch (scenario) {
  case PERF_SCENARIO_IDLE:
    return "idle";
  case PERF_SCENARIO_OVERLAY:
    return "overlay";
  case PERF_SCENARIO_SPARSE:
    return "sparse";
  case PERF_SCENARIO_CLIP:
    return "clip";
  case PERF_SCENARIO_SCROLL:
    return "scroll";
  case PERF_SCENARIO_TEXT:
    return "text";
  default:
    return "idle";
  }
}

static PerfScenario parse_scenario(const char *name) {
  if (!name)
    return PERF_SCENARIO_IDLE;
  if (strcmp(name, "overlay") == 0)
    return PERF_SCENARIO_OVERLAY;
  if (strcmp(name, "sparse") == 0)
    return PERF_SCENARIO_SPARSE;
  if (strcmp(name, "clip") == 0)
    return PERF_SCENARIO_CLIP;
  if (strcmp(name, "scroll") == 0)
    return PERF_SCENARIO_SCROLL;
  if (strcmp(name, "text") == 0)
    return PERF_SCENARIO_TEXT;
  return PERF_SCENARIO_IDLE;
}

static void interval_add_sample(StygianContext *ctx, PerfIntervalStats *stats,
                                bool eval_only) {
  if (!ctx || !stats)
    return;
  if (eval_only)
    stats->eval_frames++;
  else
    stats->render_frames++;
  stats->samples++;
  stats->sum_gpu_ms += (double)stygian_get_last_frame_gpu_ms(ctx);
  stats->sum_build_ms += (double)stygian_get_last_frame_build_ms(ctx);
  stats->sum_submit_ms += (double)stygian_get_last_frame_submit_ms(ctx);
  stats->sum_present_ms += (double)stygian_get_last_frame_present_ms(ctx);
  stats->sum_upload_bytes +=
      (double)stygian_get_last_frame_upload_bytes(ctx);
  stats->sum_upload_ranges +=
      (double)stygian_get_last_frame_upload_ranges(ctx);
}

static void interval_log(const PerfIntervalStats *stats,
                         const char *scenario_label, uint32_t second_index,
                         StygianContext *ctx) {
  double n = (stats && stats->samples > 0u) ? (double)stats->samples : 1.0;
  printf("PERFCASE scenario=%s backend=%s second=%u render=%u eval=%u "
         "gpu_ms=%.4f build_ms=%.4f submit_ms=%.4f present_ms=%.4f "
         "upload_bytes=%.0f upload_ranges=%.2f cmd_applied=%u cmd_drops=%u\n",
         scenario_label, STYGIAN_SUITE_RENDERER_NAME, second_index,
         stats ? stats->render_frames : 0u, stats ? stats->eval_frames : 0u,
         stats ? stats->sum_gpu_ms / n : 0.0, stats ? stats->sum_build_ms / n : 0.0,
         stats ? stats->sum_submit_ms / n : 0.0,
         stats ? stats->sum_present_ms / n : 0.0,
         stats ? stats->sum_upload_bytes / n : 0.0,
         stats ? stats->sum_upload_ranges / n : 0.0,
         stygian_get_last_commit_applied(ctx), stygian_get_total_command_drops(ctx));
}

static void render_sparse_static_scene(StygianContext *ctx) {
  const uint32_t cols = 100u;
  const uint32_t rows = 100u;
  const float base_x = 12.0f;
  const float base_y = 72.0f;
  const float step_x = 7.0f;
  const float step_y = 5.6f;
  for (uint32_t y = 0u; y < rows; y++) {
    for (uint32_t x = 0u; x < cols; x++) {
      uint32_t index = y * cols + x;
      float phase = ((float)(index % 251u)) / 250.0f;
      float r = 0.11f + 0.04f * phase;
      float g = 0.13f + 0.03f * phase;
      float b = 0.17f + 0.04f * phase;
      stygian_rect(ctx, base_x + (float)x * step_x, base_y + (float)y * step_y,
                   5.4f, 4.0f, r, g, b, 1.0f);
    }
  }
}

static void render_sparse_dynamic_scene(StygianContext *ctx, uint32_t tick_count) {
  const uint32_t cols = 100u;
  const float base_x = 12.0f;
  const float base_y = 72.0f;
  const float step_x = 7.0f;
  const float step_y = 5.6f;
  const uint32_t hot_points = 256u;
  for (uint32_t i = 0u; i < hot_points; i++) {
    uint32_t idx = (i * 97u + tick_count * 131u) % 10000u;
    uint32_t x = idx % cols;
    uint32_t y = idx / cols;
    float phase = ((float)((idx + tick_count * 13u) % 211u)) / 210.0f;
    stygian_rect(ctx, base_x + (float)x * step_x, base_y + (float)y * step_y,
                 5.4f, 4.0f, 0.35f + 0.6f * phase, 0.86f - 0.4f * phase,
                 0.2f + 0.4f * phase, 1.0f);
  }
}

static void render_clip_scene(StygianContext *ctx, uint32_t tick_count, int width,
                              int height) {
  float x = 80.0f;
  float y = 100.0f;
  float w = (float)width - 220.0f;
  float h = (float)height - 180.0f;
  float drift = (float)((tick_count % 120u)) * 0.5f;
  for (uint32_t depth = 0u; depth < 20u; depth++) {
    float inset = (float)depth * 10.0f;
    if (w - inset * 2.0f <= 4.0f || h - inset * 2.0f <= 4.0f)
      break;
    stygian_clip_push(ctx, x + inset, y + inset, w - inset * 2.0f,
                      h - inset * 2.0f);
    stygian_rect(ctx, x + inset + drift, y + inset + 2.0f + (float)depth * 0.5f,
                 w - inset * 2.0f - drift, 3.0f, 0.14f + 0.01f * (float)depth,
                 0.25f, 0.35f, 1.0f);
  }
  for (uint32_t depth = 0u; depth < 20u; depth++) {
    stygian_clip_pop(ctx);
  }
}

static void render_scroll_shell(StygianContext *ctx, int width, int height) {
  float vx = 40.0f;
  float vy = 82.0f;
  float vw = (float)width - 80.0f;
  float vh = (float)height - 130.0f;
  stygian_rect_rounded(ctx, vx, vy, vw, vh, 0.12f, 0.13f, 0.16f, 1.0f, 8.0f);
  stygian_widgets_register_region(vx, vy, vw, vh, STYGIAN_WIDGET_REGION_SCROLL);
}

static bool render_scroll_rows(StygianContext *ctx, StygianFont font,
                               float *scroll_y, int width, int height) {
  float vx = 40.0f;
  float vy = 82.0f;
  float vw = (float)width - 80.0f;
  float vh = (float)height - 130.0f;
  float row_h = 32.0f;
  int total_rows = 180;
  float content_h = (float)total_rows * row_h;
  int first_row;
  int visible_rows;
  int end_row;
  bool changed = false;
  if (!scroll_y)
    return false;
  first_row = (int)(*scroll_y / row_h) - 2;
  if (first_row < 0)
    first_row = 0;
  visible_rows = (int)(vh / row_h) + 6;
  end_row = first_row + visible_rows;
  if (end_row > total_rows)
    end_row = total_rows;

  stygian_clip_push(ctx, vx + 8.0f, vy + 8.0f, vw - 16.0f, vh - 16.0f);
  for (int i = first_row; i < end_row; i++) {
    char line[80];
    float ly = vy + 10.0f + (float)i * row_h - *scroll_y;
    snprintf(line, sizeof(line), "Scrollable row %03d  mutation target", i + 1);
    stygian_text(ctx, font, line, vx + 14.0f, ly, 15.0f, 0.78f, 0.83f, 0.9f,
                 1.0f);
  }
  stygian_clip_pop(ctx);
  if (stygian_scrollbar_v(ctx, vx + vw - 10.0f, vy + 6.0f, 6.0f, vh - 12.0f,
                          content_h, scroll_y)) {
    changed = true;
  }
  return changed;
}

static void render_text_scene(StygianContext *ctx, StygianFont font,
                              StygianTextArea *editor) {
  stygian_rect_rounded(ctx, editor->x - 6.0f, editor->y - 6.0f,
                       editor->w + 12.0f, editor->h + 12.0f, 0.12f, 0.13f, 0.16f,
                       1.0f, 8.0f);
  stygian_text_area(ctx, font, editor);
}

int main(int argc, char **argv) {
  PerfScenario scenario = PERF_SCENARIO_IDLE;
  int duration_seconds = 12;
  int width = 1280;
  int height = 820;
  bool first_frame = true;
  bool show_perf = true;
  double start_time;
  double last_tick_time;
  double next_interval_time;
  uint32_t second_index = 0u;
  uint32_t tick_count = 0u;
  float auto_scroll_y = 0.0f;
  float auto_scroll_dir = 1.0f;
  char editor_buffer[32768];
  StygianTextArea editor_state;
  StygianMiniPerfHarness perf;
  PerfIntervalStats interval_stats;
  const StygianScopeId k_scope_chrome = 0x4301u;
  const StygianScopeId k_scope_scene_static = 0x4302u;
  const StygianScopeId k_scope_scene_dynamic = 0x4305u;
  const StygianScopeId k_scope_overlay =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4303u;
  const StygianScopeId k_scope_perf =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x4304u;
  const char *scenario_label;
  StygianWindowConfig win_cfg;
  StygianWindow *window;
  StygianConfig cfg;
  StygianContext *ctx;
  StygianFont font;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--scenario") == 0 && (i + 1) < argc) {
      scenario = parse_scenario(argv[++i]);
    } else if (strcmp(argv[i], "--seconds") == 0 && (i + 1) < argc) {
      duration_seconds = atoi(argv[++i]);
      if (duration_seconds < 2)
        duration_seconds = 2;
    } else if (strcmp(argv[i], "--no-perf") == 0) {
      show_perf = false;
    }
  }

  scenario_label = scenario_name(scenario);

  win_cfg.title = "Stygian Pathological Perf Suite";
  win_cfg.width = width;
  win_cfg.height = height;
  win_cfg.flags = STYGIAN_SUITE_WINDOW_FLAGS;
  win_cfg.role = STYGIAN_ROLE_MAIN;
  win_cfg.gl_major = 4;
  win_cfg.gl_minor = 3;

  window = stygian_window_create(&win_cfg);
  if (!window)
    return 1;

  cfg.backend = STYGIAN_SUITE_BACKEND;
  cfg.max_elements = 0u;
  cfg.max_textures = 0u;
  cfg.glyph_feature_flags = 0u;
  cfg.window = window;
  cfg.shader_dir = NULL;
  cfg.persistent_allocator = NULL;

  ctx = stygian_create(&cfg);
  if (!ctx)
    return 1;
  font = stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");

  memset(&interval_stats, 0, sizeof(interval_stats));
  memset(editor_buffer, 0, sizeof(editor_buffer));
  snprintf(editor_buffer, sizeof(editor_buffer),
           "// pathological text churn\n"
           "let perf = stygian::perf();\n"
           "fn mutate() { /* append */ }\n");
  memset(&editor_state, 0, sizeof(editor_state));
  editor_state.buffer = editor_buffer;
  editor_state.buffer_size = (int)sizeof(editor_buffer);
  editor_state.cursor_idx = (int)strlen(editor_buffer);
  editor_state.selection_start = editor_state.cursor_idx;
  editor_state.selection_end = editor_state.cursor_idx;

  stygian_mini_perf_init(&perf, "perf_pathological_suite");
  perf.widget.renderer_name = STYGIAN_SUITE_RENDERER_NAME;
  perf.widget.enabled = show_perf;
  perf.widget.show_graph = true;
  perf.widget.idle_hz = 30u;
  perf.widget.active_hz = 30u;
  perf.widget.max_stress_hz = 120u;
  perf.widget.graph_max_segments = 64u;

  start_time = now_seconds();
  last_tick_time = start_time;
  next_interval_time = start_time + 1.0;

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    bool event_mutated = false;
    bool event_requested = false;
    bool event_eval_requested = false;
    bool scene_static_changed = false;
    bool scene_dynamic_changed = false;
    bool overlay_changed = false;
    bool chrome_changed = false;
    bool repaint_pending;
    bool render_frame;
    bool eval_only_frame;
    double current_time = now_seconds();
    double dt;
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

    current_time = now_seconds();
    dt = current_time - last_tick_time;
    if (dt > (1.0 / 30.0)) {
      tick_count++;
      last_tick_time = current_time;
      if (scenario == PERF_SCENARIO_OVERLAY) {
        overlay_changed = true;
      } else if (scenario == PERF_SCENARIO_SPARSE) {
        scene_dynamic_changed = true;
      } else if (scenario == PERF_SCENARIO_CLIP) {
        scene_dynamic_changed = true;
      } else if (scenario == PERF_SCENARIO_SCROLL) {
        auto_scroll_y += auto_scroll_dir * 22.0f;
        if (auto_scroll_y > 5400.0f) {
          auto_scroll_y = 5400.0f;
          auto_scroll_dir = -1.0f;
        } else if (auto_scroll_y < 0.0f) {
          auto_scroll_y = 0.0f;
          auto_scroll_dir = 1.0f;
        }
        scene_dynamic_changed = true;
      } else if (scenario == PERF_SCENARIO_TEXT) {
        size_t len = strlen(editor_buffer);
        if (len + 3 < sizeof(editor_buffer)) {
          editor_buffer[len] = (char)('a' + (tick_count % 26u));
          editor_buffer[len + 1] = '\n';
          editor_buffer[len + 2] = '\0';
          editor_state.cursor_idx = (int)(len + 2u);
          editor_state.selection_start = editor_state.cursor_idx;
          editor_state.selection_end = editor_state.cursor_idx;
          scene_dynamic_changed = true;
        }
      }
    }

    if (scene_static_changed || scene_dynamic_changed || overlay_changed ||
        chrome_changed || event_mutated) {
      stygian_set_repaint_source(ctx, "mutation");
      stygian_request_repaint_after_ms(ctx, 0u);
    }

    repaint_pending = stygian_has_pending_repaint(ctx);
    render_frame = first_frame || event_mutated || scene_static_changed ||
                   scene_dynamic_changed ||
                   overlay_changed || repaint_pending;
    eval_only_frame =
        (!render_frame && (event_eval_requested || event_requested));
    if (!render_frame && !eval_only_frame) {
      if ((now_seconds() - start_time) >= (double)duration_seconds) {
        break;
      }
      continue;
    }
    first_frame = false;

    stygian_window_get_size(window, &width, &height);
    stygian_begin_frame_intent(
        ctx, width, height,
        eval_only_frame ? STYGIAN_FRAME_EVAL_ONLY : STYGIAN_FRAME_RENDER);

    stygian_scope_begin(ctx, k_scope_chrome);
    stygian_rect(ctx, 0.0f, 0.0f, (float)width, 44.0f, 0.09f, 0.11f, 0.14f,
                 1.0f);
    if (font) {
      char title[128];
      snprintf(title, sizeof(title), "Pathological suite: %s (%s)",
               scenario_label, STYGIAN_SUITE_RENDERER_NAME);
      stygian_text(ctx, font, title, 14.0f, 13.0f, 15.0f, 0.95f, 0.96f, 0.99f,
                   1.0f);
    }
    stygian_scope_end(ctx);

    stygian_scope_begin(ctx, k_scope_scene_static);
    if (scenario == PERF_SCENARIO_IDLE) {
      stygian_rect_rounded(ctx, 24.0f, 80.0f, (float)width - 48.0f,
                           (float)height - 120.0f, 0.12f, 0.13f, 0.16f, 1.0f,
                           8.0f);
      if (font) {
        stygian_text(ctx, font, "Idle scenario: no active mutation path.",
                     40.0f, 112.0f, 18.0f, 0.8f, 0.85f, 0.9f, 1.0f);
      }
    } else if (scenario == PERF_SCENARIO_SPARSE) {
      render_sparse_static_scene(ctx);
    } else if (scenario == PERF_SCENARIO_SCROLL) {
      render_scroll_shell(ctx, width, height);
    } else if (scenario == PERF_SCENARIO_OVERLAY) {
      stygian_rect_rounded(ctx, 24.0f, 80.0f, (float)width - 48.0f,
                           (float)height - 120.0f, 0.11f, 0.12f, 0.15f, 1.0f,
                           8.0f);
      if (font) {
        stygian_text(ctx, font, "Overlay scenario: base scope should stay clean.",
                     38.0f, 116.0f, 17.0f, 0.8f, 0.85f, 0.9f, 1.0f);
      }
    } else {
      stygian_rect_rounded(ctx, 24.0f, 80.0f, (float)width - 48.0f,
                           (float)height - 120.0f, 0.12f, 0.13f, 0.16f, 1.0f,
                           8.0f);
    }
    stygian_scope_end(ctx);

    stygian_scope_begin(ctx, k_scope_scene_dynamic);
    if (scenario == PERF_SCENARIO_SPARSE) {
      render_sparse_dynamic_scene(ctx, tick_count);
    } else if (scenario == PERF_SCENARIO_CLIP) {
      render_clip_scene(ctx, tick_count, width, height);
    } else if (scenario == PERF_SCENARIO_SCROLL) {
      if (render_scroll_rows(ctx, font, &auto_scroll_y, width, height)) {
        scene_dynamic_changed = true;
      }
    } else if (scenario == PERF_SCENARIO_TEXT) {
      editor_state.x = 30.0f;
      editor_state.y = 74.0f;
      editor_state.w = (float)width - 60.0f;
      editor_state.h = (float)height - 120.0f;
      render_text_scene(ctx, font, &editor_state);
    }
    stygian_scope_end(ctx);

    stygian_scope_begin(ctx, k_scope_overlay);
    if (scenario == PERF_SCENARIO_OVERLAY) {
      stygian_request_overlay_hz(ctx, 30u);
      stygian_line(ctx, 36.0f, (float)height - 70.0f,
                   36.0f + (float)((tick_count * 7u) % 600u),
                   (float)height - 70.0f, 1.8f, 0.2f, 0.85f, 0.42f, 1.0f);
    }
    stygian_scope_end(ctx);

    if (show_perf) {
      stygian_scope_begin(ctx, k_scope_perf);
      stygian_mini_perf_draw(ctx, font, &perf, width, height);
      stygian_scope_end(ctx);
    }

    if (first_frame || event_mutated) {
      stygian_scope_invalidate_next(ctx, k_scope_scene_static);
      stygian_scope_invalidate_next(ctx, k_scope_scene_dynamic);
    } else {
      if (scene_static_changed) {
        stygian_scope_invalidate_next(ctx, k_scope_scene_static);
      }
      if (scene_dynamic_changed) {
        stygian_scope_invalidate_next(ctx, k_scope_scene_dynamic);
      }
    }
    if (overlay_changed) {
      stygian_scope_invalidate_next(ctx, k_scope_overlay);
    }
    if (!show_perf) {
      stygian_scope_invalidate_next(ctx, k_scope_perf);
    }

    stygian_widgets_commit_regions();
    stygian_end_frame(ctx);
    stygian_mini_perf_accumulate(&perf, eval_only_frame);
    interval_add_sample(ctx, &interval_stats, eval_only_frame);

    current_time = now_seconds();
    if (current_time >= next_interval_time) {
      second_index++;
      interval_log(&interval_stats, scenario_label, second_index, ctx);
      memset(&interval_stats, 0, sizeof(interval_stats));
      next_interval_time += 1.0;
    }

    if ((current_time - start_time) >= (double)duration_seconds) {
      break;
    }
  }

  if (interval_stats.samples > 0u) {
    second_index++;
    interval_log(&interval_stats, scenario_label, second_index, ctx);
  }

  if (font)
    stygian_font_destroy(ctx, font);
  stygian_destroy(ctx);
  stygian_window_destroy(window);
  return 0;
}
