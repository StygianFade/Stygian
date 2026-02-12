#ifndef STYGIAN_MINI_PERF_HARNESS_H
#define STYGIAN_MINI_PERF_HARNESS_H

#include "../include/stygian.h"
#include "../widgets/stygian_widgets.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct StygianMiniPerfHarness {
  const char *name;
  double last_log_seconds;
  uint32_t render_frames;
  uint32_t eval_frames;
  bool pos_initialized;
  StygianPerfWidget widget;
} StygianMiniPerfHarness;

void stygian_mini_perf_init(StygianMiniPerfHarness *perf, const char *name);
void stygian_mini_perf_accumulate(StygianMiniPerfHarness *perf, bool eval_only);
void stygian_mini_perf_draw(StygianContext *ctx, StygianFont font,
                            StygianMiniPerfHarness *perf, int width,
                            int height);
void stygian_mini_perf_log(StygianContext *ctx, StygianMiniPerfHarness *perf);

#endif // STYGIAN_MINI_PERF_HARNESS_H
