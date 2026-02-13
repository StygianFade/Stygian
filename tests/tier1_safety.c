#include "../include/stygian.h"
#include "../include/stygian_cmd.h"
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
      .width = 320,
      .height = 240,
      .title = "stygian_tier1_safety",
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
  cfg.max_textures = 128;
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

static void test_invalid_handle_basics(TestEnv *env) {
  CHECK(!stygian_element_is_valid(env->ctx, 0u), "element zero invalid");
  CHECK(!stygian_texture_is_valid(env->ctx, 0u), "texture zero invalid");
  CHECK(!stygian_font_is_valid(env->ctx, 0u), "font zero invalid");
  CHECK(!stygian_element_is_valid(env->ctx, 0xFFFFFFFFu),
        "element random invalid");
  CHECK(!stygian_texture_is_valid(env->ctx, 0xFFFFFFFFu),
        "texture random invalid");
  CHECK(!stygian_font_is_valid(env->ctx, 0xFFFFFFFFu), "font random invalid");
}

static void test_element_lifecycle(TestEnv *env) {
  StygianElement e1 = stygian_element(env->ctx);
  CHECK(e1 != 0u, "element alloc returns non-zero");
  CHECK(stygian_element_is_valid(env->ctx, e1), "element valid after alloc");

  stygian_element_free(env->ctx, e1);
  CHECK(!stygian_element_is_valid(env->ctx, e1), "element invalid after free");

  stygian_element_free(env->ctx, e1);
  CHECK(!stygian_element_is_valid(env->ctx, e1),
        "element stays invalid after double free");

  {
    StygianElement e2 = stygian_element(env->ctx);
    CHECK(e2 != 0u, "element realloc returns non-zero");
    CHECK(e2 != e1, "reused slot gets new generation handle");
    CHECK(stygian_element_is_valid(env->ctx, e2), "new element valid");
    stygian_element_free(env->ctx, e2);
  }
}

static void test_texture_lifecycle(TestEnv *env) {
  uint8_t rgba[4 * 4 * 4];
  memset(rgba, 0xCC, sizeof(rgba));

  StygianTexture tex = stygian_texture_create(env->ctx, 4, 4, rgba);
  CHECK(tex != 0u, "texture create returns non-zero");
  CHECK(stygian_texture_is_valid(env->ctx, tex), "texture valid after create");
  CHECK(stygian_texture_update(env->ctx, tex, 0, 0, 4, 4, rgba),
        "texture update valid handle succeeds");

  stygian_texture_destroy(env->ctx, tex);
  CHECK(!stygian_texture_is_valid(env->ctx, tex), "texture invalid after destroy");
  CHECK(!stygian_texture_update(env->ctx, tex, 0, 0, 4, 4, rgba),
        "stale texture update rejected");

  stygian_texture_destroy(env->ctx, tex);
  CHECK(!stygian_texture_is_valid(env->ctx, tex),
        "texture stays invalid after double destroy");
}

static void test_font_lifecycle(TestEnv *env) {
  StygianFont font =
      stygian_font_load(env->ctx, "assets/atlas.png", "assets/atlas.json");
  CHECK(font != 0u, "font load returns non-zero");
  CHECK(stygian_font_is_valid(env->ctx, font), "font valid after load");

  stygian_font_destroy(env->ctx, font);
  CHECK(!stygian_font_is_valid(env->ctx, font), "font invalid after destroy");
  CHECK(stygian_text(env->ctx, font, "x", 10.0f, 10.0f, 14.0f, 1, 1, 1, 1) ==
            0u,
        "text with stale font rejected");

  stygian_font_destroy(env->ctx, font);
  CHECK(!stygian_font_is_valid(env->ctx, font),
        "font stays invalid after double destroy");
}

static void test_stale_texture_binding_noop(TestEnv *env) {
  uint8_t rgba[4 * 4 * 4];
  StygianElement e;
  StygianTexture tex;
  memset(rgba, 0x7F, sizeof(rgba));

  e = stygian_element(env->ctx);
  tex = stygian_texture_create(env->ctx, 4, 4, rgba);
  CHECK(e != 0u && tex != 0u, "fixture handles allocated for stale bind");
  stygian_texture_destroy(env->ctx, tex);
  stygian_set_texture(env->ctx, e, tex, 0, 0, 1, 1);
  CHECK(stygian_element_is_valid(env->ctx, e),
        "stale texture set does not invalidate element");
  stygian_element_free(env->ctx, e);
}

static void test_cmd_rejects_stale_element(TestEnv *env) {
  StygianElement e = stygian_element(env->ctx);
  StygianCmdBuffer *cmd;
  CHECK(e != 0u, "command fixture element allocated");
  stygian_element_free(env->ctx, e);

  cmd = stygian_cmd_begin(env->ctx, 0xCAFEu);
  CHECK(cmd != NULL, "command buffer begin succeeds");
  if (cmd) {
    CHECK(!stygian_cmd_set_color(cmd, e, 1, 0, 0, 1),
          "command rejects stale element");
    stygian_cmd_discard(cmd);
  }
}

static void test_cmd_accepts_valid_element(TestEnv *env) {
  StygianElement e = stygian_element(env->ctx);
  StygianCmdBuffer *cmd = stygian_cmd_begin(env->ctx, 0xBEEFu);
  CHECK(e != 0u && cmd != NULL, "command valid fixture");
  if (e != 0u && cmd) {
    CHECK(stygian_cmd_set_color(cmd, e, 0.2f, 0.3f, 0.4f, 1.0f),
          "command accepts valid element");
    CHECK(stygian_cmd_submit(env->ctx, cmd), "command submit succeeds");
  } else if (cmd) {
    stygian_cmd_discard(cmd);
  }
  if (e)
    stygian_element_free(env->ctx, e);
}

static void test_frame_intent_eval_only(TestEnv *env) {
  stygian_begin_frame_intent(env->ctx, 320, 240, STYGIAN_FRAME_EVAL_ONLY);
  CHECK(stygian_is_eval_only_frame(env->ctx), "eval-only frame flag set");
  stygian_end_frame(env->ctx);

  stygian_begin_frame(env->ctx, 320, 240);
  stygian_rect(env->ctx, 0.0f, 0.0f, 40.0f, 20.0f, 1, 0, 0, 1);
  stygian_end_frame(env->ctx);
  CHECK(!stygian_is_eval_only_frame(env->ctx), "render frame flag clear");
}

int main(void) {
  TestEnv env;
  if (!test_env_init(&env)) {
    fprintf(stderr, "[ERROR] failed to initialize test context\n");
    return 2;
  }

  test_invalid_handle_basics(&env);
  test_element_lifecycle(&env);
  test_texture_lifecycle(&env);
  test_font_lifecycle(&env);
  test_stale_texture_binding_noop(&env);
  test_cmd_rejects_stale_element(&env);
  test_cmd_accepts_valid_element(&env);
  test_frame_intent_eval_only(&env);

  test_env_destroy(&env);

  if (g_failures == 0) {
    printf("[PASS] tier1 safety suite complete\n");
    return 0;
  }
  fprintf(stderr, "[FAIL] tier1 safety suite failures=%d\n", g_failures);
  return 1;
}
