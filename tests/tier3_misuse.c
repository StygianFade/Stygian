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
static uint32_t g_rng = 0xC0FFEE11u;

#define CHECK(cond, name)                                                      \
  do {                                                                         \
    if (cond) {                                                                \
      printf("[PASS] %s\n", name);                                             \
    } else {                                                                   \
      fprintf(stderr, "[FAIL] %s\n", name);                                    \
      g_failures++;                                                            \
    }                                                                          \
  } while (0)

static uint32_t next_u32(void) {
  g_rng = g_rng * 1664525u + 1013904223u;
  return g_rng;
}

static float next_f01(void) {
  return (float)(next_u32() & 0xFFFFu) / 65535.0f;
}

static uint32_t pick_index(uint32_t n) {
  if (n == 0u)
    return 0u;
  return next_u32() % n;
}

static int test_env_init(TestEnv *env) {
  StygianWindowConfig win_cfg = {
      .width = 400,
      .height = 300,
      .title = "stygian_tier3_misuse",
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
  cfg.max_elements = 384;
  cfg.max_textures = 96;
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

static StygianElement random_element_handle(StygianElement *elems,
                                            uint32_t elem_n) {
  if ((next_u32() & 3u) == 0u) {
    return (StygianElement)(0x10000000u | (next_u32() & 0x0FFFFFFFu));
  }
  return elems[pick_index(elem_n)];
}

static StygianTexture random_texture_handle(StygianTexture *textures,
                                            uint32_t tex_n) {
  if ((next_u32() & 3u) == 0u) {
    return (StygianTexture)(0x20000000u | (next_u32() & 0x0FFFFFFFu));
  }
  return textures[pick_index(tex_n)];
}

static StygianFont random_font_handle(StygianFont *fonts, uint32_t font_n) {
  if ((next_u32() & 3u) == 0u) {
    return (StygianFont)(0x30000000u | (next_u32() & 0x0FFFFFFFu));
  }
  return fonts[pick_index(font_n)];
}

static void test_misuse_sequences(TestEnv *env) {
  StygianElement elems[96];
  StygianTexture textures[48];
  StygianFont fonts[16];
  uint8_t rgba[4 * 8 * 8];
  uint32_t frame;
  uint32_t i;

  memset(elems, 0, sizeof(elems));
  memset(textures, 0, sizeof(textures));
  memset(fonts, 0, sizeof(fonts));
  memset(rgba, 0xAA, sizeof(rgba));
  fonts[0] = stygian_font_load(env->ctx, "assets/atlas.png", "assets/atlas.json");

  for (frame = 0u; frame < 36u; frame++) {
    uint32_t step;
    stygian_request_repaint_after_ms(env->ctx, 0u);
    stygian_begin_frame(env->ctx, 400, 300);

    for (step = 0u; step < 64u; step++) {
      uint32_t op = next_u32() % 10u;
      uint32_t idx;
      switch (op) {
      case 0: {
        idx = pick_index((uint32_t)(sizeof(elems) / sizeof(elems[0])));
        if (elems[idx] && stygian_element_is_valid(env->ctx, elems[idx])) {
          stygian_element_free(env->ctx, elems[idx]);
          elems[idx] = 0u;
        } else {
          elems[idx] = stygian_element(env->ctx);
        }
        break;
      }
      case 1: {
        StygianElement e = random_element_handle(
            elems, (uint32_t)(sizeof(elems) / sizeof(elems[0])));
        stygian_set_bounds(env->ctx, e, next_f01() * 200.0f, next_f01() * 160.0f,
                           10.0f + next_f01() * 50.0f,
                           8.0f + next_f01() * 40.0f);
        stygian_set_color(env->ctx, e, next_f01(), next_f01(), next_f01(), 1.0f);
        break;
      }
      case 2: {
        idx = pick_index((uint32_t)(sizeof(textures) / sizeof(textures[0])));
        if (textures[idx] &&
            stygian_texture_is_valid(env->ctx, textures[idx])) {
          if ((next_u32() & 1u) == 0u) {
            stygian_texture_update(env->ctx, textures[idx], 0, 0, 8, 8, rgba);
          } else {
            stygian_texture_destroy(env->ctx, textures[idx]);
            textures[idx] = 0u;
          }
        } else if ((next_u32() & 1u) == 0u) {
          textures[idx] = stygian_texture_create(env->ctx, 8, 8, rgba);
        }
        break;
      }
      case 3: {
        StygianElement e = random_element_handle(
            elems, (uint32_t)(sizeof(elems) / sizeof(elems[0])));
        StygianTexture t = random_texture_handle(
            textures, (uint32_t)(sizeof(textures) / sizeof(textures[0])));
        stygian_set_texture(env->ctx, e, t, 0.0f, 0.0f, 1.0f, 1.0f);
        break;
      }
      case 4: {
        idx = pick_index((uint32_t)(sizeof(fonts) / sizeof(fonts[0])));
        if (fonts[idx] && stygian_font_is_valid(env->ctx, fonts[idx])) {
          if ((next_u32() & 1u) == 0u) {
            stygian_font_destroy(env->ctx, fonts[idx]);
            fonts[idx] = 0u;
          } else {
            stygian_text_width(env->ctx, fonts[idx], "abc xyz", 12.0f);
          }
        }
        break;
      }
      case 5: {
        StygianFont f = random_font_handle(
            fonts, (uint32_t)(sizeof(fonts) / sizeof(fonts[0])));
        stygian_text(env->ctx, f, "misuse", 12.0f + next_f01() * 240.0f,
                     16.0f + next_f01() * 160.0f, 12.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        break;
      }
      case 6: {
        StygianCmdBuffer *cmd = stygian_cmd_begin(env->ctx, 0x5500u + step);
        if (cmd) {
          StygianElement e = random_element_handle(
              elems, (uint32_t)(sizeof(elems) / sizeof(elems[0])));
          stygian_cmd_set_color(cmd, e, next_f01(), next_f01(), next_f01(), 1.0f);
          if ((next_u32() & 1u) == 0u)
            stygian_cmd_submit(env->ctx, cmd);
          else
            stygian_cmd_discard(cmd);
        }
        break;
      }
      case 7: {
        StygianScopeId sid = 0xA0000000u | (next_u32() & 0xFFu);
        stygian_scope_begin(env->ctx, sid);
        stygian_rect(env->ctx, 5.0f + next_f01() * 320.0f, 5.0f, 14.0f, 10.0f,
                     0.2f, 0.8f, 0.4f, 1.0f);
        stygian_scope_end(env->ctx);
        break;
      }
      case 8: {
        uint8_t clip = stygian_clip_push(env->ctx, next_f01() * 120.0f,
                                         next_f01() * 120.0f, 50.0f, 50.0f);
        StygianElement e = random_element_handle(
            elems, (uint32_t)(sizeof(elems) / sizeof(elems[0])));
        stygian_set_clip(env->ctx, e, clip);
        stygian_clip_pop(env->ctx);
        break;
      }
      default:
        stygian_rect(env->ctx, next_f01() * 320.0f, next_f01() * 220.0f, 12.0f,
                     12.0f, next_f01(), next_f01(), next_f01(), 1.0f);
        break;
      }
    }

    stygian_end_frame(env->ctx);

    if (fonts[0] == 0u || !stygian_font_is_valid(env->ctx, fonts[0])) {
      fonts[0] =
          stygian_font_load(env->ctx, "assets/atlas.png", "assets/atlas.json");
    }

    CHECK(stygian_get_free_element_count(env->ctx) <=
              stygian_get_element_capacity(env->ctx),
          "free element count bounded by capacity");
    CHECK(stygian_get_total_command_drops(env->ctx) == 0u,
          "no command drops in fuzz sequence");
  }

  for (i = 0u; i < (uint32_t)(sizeof(fonts) / sizeof(fonts[0])); i++) {
    if (fonts[i] && stygian_font_is_valid(env->ctx, fonts[i])) {
      stygian_font_destroy(env->ctx, fonts[i]);
      fonts[i] = 0u;
    }
  }
  for (i = 0u; i < (uint32_t)(sizeof(textures) / sizeof(textures[0])); i++) {
    if (textures[i] && stygian_texture_is_valid(env->ctx, textures[i])) {
      stygian_texture_destroy(env->ctx, textures[i]);
      textures[i] = 0u;
    }
  }
  for (i = 0u; i < (uint32_t)(sizeof(elems) / sizeof(elems[0])); i++) {
    if (elems[i] && stygian_element_is_valid(env->ctx, elems[i])) {
      stygian_element_free(env->ctx, elems[i]);
      elems[i] = 0u;
    }
  }
}

int main(void) {
  TestEnv env;
  if (!test_env_init(&env)) {
    fprintf(stderr, "[ERROR] failed to initialize tier3 misuse test env\n");
    return 2;
  }

  test_misuse_sequences(&env);
  test_env_destroy(&env);

  if (g_failures == 0) {
    printf("[PASS] tier3 misuse suite complete\n");
    return 0;
  }
  fprintf(stderr, "[FAIL] tier3 misuse suite failures=%d\n", g_failures);
  return 1;
}
