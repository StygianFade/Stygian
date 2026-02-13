#include "../include/stygian.h"
#include "../window/stygian_window.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct TestEnv {
  StygianWindow *window;
  StygianContext *ctx;
} TestEnv;

static int g_failures = 0;

#define CHECK(cond, name)                                                      \
  do {                                                                         \
    if (cond) {                                                                \
      printf("[PASS] %s\n", name);                                             \
    } else {                                                                   \
      fprintf(stderr, "[FAIL] %s\n", name);                                    \
      g_failures++;                                                            \
    }                                                                          \
  } while (0)

static int test_env_init(TestEnv *env) {
  StygianWindowConfig win_cfg = {
      .width = 360,
      .height = 260,
      .title = "stygian_tier2_runtime",
      .flags = STYGIAN_WINDOW_OPENGL,
  };
  StygianConfig cfg;
  if (!env)
    return 0;
  memset(env, 0, sizeof(*env));

  env->window = stygian_window_create(&win_cfg);
  if (!env->window)
    return 0;

  memset(&cfg, 0, sizeof(cfg));
  cfg.backend = STYGIAN_BACKEND_OPENGL;
  cfg.max_elements = 256;
  cfg.max_textures = 64;
  cfg.window = env->window;
  env->ctx = stygian_create(&cfg);
  if (!env->ctx) {
    stygian_window_destroy(env->window);
    env->window = NULL;
    return 0;
  }
  return 1;
}

static void test_env_destroy(TestEnv *env) {
  if (!env)
    return;
  if (env->ctx) {
    stygian_destroy(env->ctx);
    env->ctx = NULL;
  }
  if (env->window) {
    stygian_window_destroy(env->window);
    env->window = NULL;
  }
}

static void begin_render_frame(TestEnv *env) {
  stygian_request_repaint_after_ms(env->ctx, 0u);
  stygian_begin_frame(env->ctx, 360, 260);
}

static void build_scope_rects(TestEnv *env, StygianScopeId id, int count) {
  int i;
  stygian_scope_begin(env->ctx, id);
  for (i = 0; i < count; i++) {
    stygian_rect(env->ctx, 10.0f + (float)(i * 8), 12.0f, 18.0f, 14.0f, 1.0f,
                 0.6f, 0.2f, 1.0f);
  }
  stygian_scope_end(env->ctx);
}

static void test_scope_replay_and_invalidation(TestEnv *env) {
  const StygianScopeId id = 0x90010001u;
  CHECK(stygian_scope_is_dirty(env->ctx, id), "unknown scope reports dirty");

  begin_render_frame(env);
  build_scope_rects(env, id, 1);
  stygian_end_frame(env->ctx);
  CHECK(!stygian_scope_is_dirty(env->ctx, id), "scope clean after first build");

  begin_render_frame(env);
  build_scope_rects(env, id, 1);
  stygian_end_frame(env->ctx);
  CHECK(stygian_get_last_frame_scope_replay_hits(env->ctx) >= 1u,
        "scope replay hit recorded");

  begin_render_frame(env);
  stygian_scope_begin(env->ctx, id);
  stygian_scope_end(env->ctx);
  stygian_end_frame(env->ctx);
  CHECK(stygian_get_last_frame_scope_forced_rebuilds(env->ctx) >= 1u,
        "scope replay mismatch forces rebuild");
  CHECK(stygian_scope_is_dirty(env->ctx, id), "scope dirty after forced rebuild");

  stygian_scope_invalidate_now(env->ctx, id);
  CHECK(stygian_scope_is_dirty(env->ctx, id), "invalidate_now marks scope dirty");
  begin_render_frame(env);
  build_scope_rects(env, id, 1);
  stygian_end_frame(env->ctx);
  CHECK(!stygian_scope_is_dirty(env->ctx, id),
        "dirty scope rebuilds and becomes clean");

  stygian_scope_invalidate_next(env->ctx, id);
  CHECK(stygian_scope_is_dirty(env->ctx, id),
        "invalidate_next marks scope pending dirty");
  begin_render_frame(env);
  build_scope_rects(env, id, 1);
  stygian_end_frame(env->ctx);
  CHECK(!stygian_scope_is_dirty(env->ctx, id),
        "scope clean again after rebuild from invalidate_next");
}

static void test_overlay_invalidation_isolated(TestEnv *env) {
  const StygianScopeId base_scope = 0x90020001u;
  const StygianScopeId overlay_scope =
      STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x33u;

  begin_render_frame(env);
  build_scope_rects(env, base_scope, 1);
  build_scope_rects(env, overlay_scope, 1);
  stygian_end_frame(env->ctx);
  CHECK(!stygian_scope_is_dirty(env->ctx, base_scope), "base scope starts clean");
  CHECK(!stygian_scope_is_dirty(env->ctx, overlay_scope),
        "overlay scope starts clean");

  stygian_invalidate_overlay_scopes(env->ctx);
  CHECK(!stygian_scope_is_dirty(env->ctx, base_scope),
        "overlay invalidation leaves base clean");
  CHECK(stygian_scope_is_dirty(env->ctx, overlay_scope),
        "overlay invalidation marks overlay dirty");
}

static void test_clip_runtime_behavior(TestEnv *env) {
  uint8_t clip_id;
  StygianElement e;
  int pushes = 0;
  int i;

  begin_render_frame(env);
  clip_id = stygian_clip_push(env->ctx, 4.0f, 4.0f, 100.0f, 100.0f);
  CHECK(clip_id != 0u, "clip_push returns valid id");
  e = stygian_element(env->ctx);
  CHECK(e != 0u, "element alloc for clip test");
  stygian_set_clip(env->ctx, e, clip_id);
  stygian_set_clip(env->ctx, e, 250u);
  stygian_clip_pop(env->ctx);
  stygian_end_frame(env->ctx);
  CHECK(stygian_get_last_frame_clip_count(env->ctx) == 2u,
        "frame clip count includes pushed clip");
  stygian_element_free(env->ctx, e);

  begin_render_frame(env);
  for (i = 0; i < 300; i++) {
    uint8_t id = stygian_clip_push(env->ctx, (float)i, 0.0f, 2.0f, 2.0f);
    if (id == 0u)
      break;
    pushes++;
  }
  stygian_end_frame(env->ctx);
  CHECK(pushes == 255, "clip ids are bounded to 255 user clips");
}

static void test_transient_cleanup_determinism(TestEnv *env) {
  StygianElement transient;
  uint32_t cap = stygian_get_element_capacity(env->ctx);

  begin_render_frame(env);
  transient = stygian_element_transient(env->ctx);
  CHECK(transient != 0u, "transient element allocated");
  CHECK(stygian_element_is_valid(env->ctx, transient), "transient handle valid");
  stygian_end_frame(env->ctx);

  begin_render_frame(env);
  CHECK(!stygian_element_is_valid(env->ctx, transient),
        "transient handle invalid after next frame reset");
  CHECK(stygian_get_free_element_count(env->ctx) == cap,
        "element free count resets to full capacity");
  stygian_end_frame(env->ctx);
}

int main(void) {
  TestEnv env;
  if (!test_env_init(&env)) {
    fprintf(stderr, "[ERROR] failed to initialize tier2 runtime test env\n");
    return 2;
  }

  test_scope_replay_and_invalidation(&env);
  test_overlay_invalidation_isolated(&env);
  test_clip_runtime_behavior(&env);
  test_transient_cleanup_determinism(&env);

  test_env_destroy(&env);

  if (g_failures == 0) {
    printf("[PASS] tier2 runtime suite complete\n");
    return 0;
  }
  fprintf(stderr, "[FAIL] tier2 runtime suite failures=%d\n", g_failures);
  return 1;
}
