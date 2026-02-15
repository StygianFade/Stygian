#include "../include/stygian.h"
#include "../window/stygian_window.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

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

#ifdef _WIN32
static void pump_window_events(StygianWindow *window, int loops, DWORD sleep_ms) {
  int i;
  for (i = 0; i < loops; i++) {
    StygianEvent event;
    while (stygian_window_poll_event(window, &event)) {
    }
    if (sleep_ms > 0)
      Sleep(sleep_ms);
  }
}

static void test_borderless_maximize_uses_work_area(void) {
  StygianWindowConfig win_cfg = {
      .width = 520,
      .height = 340,
      .title = "stygian_tier2_borderless_maximize",
      .flags = STYGIAN_WINDOW_OPENGL | STYGIAN_WINDOW_RESIZABLE |
               STYGIAN_WINDOW_BORDERLESS | STYGIAN_WINDOW_CENTERED,
  };
  StygianConfig cfg;
  StygianWindow *window = NULL;
  StygianContext *ctx = NULL;
  bool maximized = false;
  int i;

  memset(&cfg, 0, sizeof(cfg));
  window = stygian_window_create(&win_cfg);
  CHECK(window != NULL, "borderless maximize fixture window created");
  if (!window)
    return;

  cfg.backend = STYGIAN_BACKEND_OPENGL;
  cfg.max_elements = 128;
  cfg.max_textures = 32;
  cfg.window = window;
  ctx = stygian_create(&cfg);
  CHECK(ctx != NULL, "borderless maximize fixture context created");
  if (!ctx) {
    stygian_window_destroy(window);
    return;
  }

  stygian_window_maximize(window);
  for (i = 0; i < 240; i++) {
    StygianEvent event;
    while (stygian_window_poll_event(window, &event)) {
    }
    if (stygian_window_is_maximized(window)) {
      maximized = true;
      break;
    }
    Sleep(8);
  }
  CHECK(maximized, "borderless maximize reaches maximized state");

  if (maximized) {
    for (i = 0; i < 120; i++) {
      StygianEvent event;
      while (stygian_window_poll_event(window, &event)) {
      }
      Sleep(8);
    }

    HWND hwnd = (HWND)stygian_window_native_handle(window);
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info;
    RECT window_rect;
    RECT client_rect;
    POINT client_tl;
    POINT client_br;
    LONG_PTR ex_style;
    bool window_rect_ok = false;
    bool client_rect_ok = false;
    bool geometry_ok = false;
    bool restored = false;

    memset(&monitor_info, 0, sizeof(monitor_info));
    memset(&window_rect, 0, sizeof(window_rect));
    memset(&client_rect, 0, sizeof(client_rect));
    memset(&client_tl, 0, sizeof(client_tl));
    memset(&client_br, 0, sizeof(client_br));
    monitor_info.cbSize = sizeof(monitor_info);
    window_rect_ok = (hwnd != NULL) && (GetWindowRect(hwnd, &window_rect) != 0);
    if (hwnd != NULL && GetClientRect(hwnd, &client_rect) != 0) {
      client_tl.x = client_rect.left;
      client_tl.y = client_rect.top;
      client_br.x = client_rect.right;
      client_br.y = client_rect.bottom;
      client_rect_ok = (ClientToScreen(hwnd, &client_tl) != 0) &&
                       (ClientToScreen(hwnd, &client_br) != 0);
    }
    geometry_ok =
        (hwnd != NULL) && (monitor != NULL) &&
        (GetMonitorInfo(monitor, &monitor_info) != 0) &&
        (window_rect_ok || client_rect_ok);
    CHECK(geometry_ok, "borderless maximize monitor geometry query succeeds");
    if (geometry_ok) {
      bool window_within_work_area =
          (window_rect.left >= monitor_info.rcWork.left) &&
          (window_rect.top >= monitor_info.rcWork.top) &&
          (window_rect.right <= monitor_info.rcWork.right) &&
          (window_rect.bottom <= monitor_info.rcWork.bottom);
      bool client_within_work_area =
          (client_tl.x >= monitor_info.rcWork.left) &&
          (client_tl.y >= monitor_info.rcWork.top) &&
          (client_br.x <= monitor_info.rcWork.right) &&
          (client_br.y <= monitor_info.rcWork.bottom);
      bool matches_work_area =
          window_within_work_area || client_within_work_area;
      CHECK(matches_work_area, "borderless maximize uses monitor work area");
    }

    ex_style = (hwnd != NULL) ? GetWindowLongPtr(hwnd, GWL_EXSTYLE) : 0;
    CHECK((ex_style & WS_EX_TOPMOST) == 0,
          "borderless maximize keeps window non-topmost");

    stygian_window_restore(window);
    for (i = 0; i < 240; i++) {
      StygianEvent event;
      while (stygian_window_poll_event(window, &event)) {
      }
      if (!stygian_window_is_maximized(window)) {
        restored = true;
        break;
      }
      Sleep(8);
    }
    CHECK(restored, "borderless restore clears maximized state");
  }

  stygian_destroy(ctx);
  stygian_window_destroy(window);
}

static void test_borderless_style_routing(void) {
  StygianWindowConfig gl_cfg = {
      .width = 500,
      .height = 320,
      .title = "stygian_tier2_borderless_gl_style",
      .flags = STYGIAN_WINDOW_OPENGL | STYGIAN_WINDOW_RESIZABLE |
               STYGIAN_WINDOW_BORDERLESS | STYGIAN_WINDOW_CENTERED,
      .role = STYGIAN_ROLE_MAIN,
  };
  StygianWindowConfig vk_cfg = {
      .width = 500,
      .height = 320,
      .title = "stygian_tier2_borderless_vk_style",
      .flags = STYGIAN_WINDOW_VULKAN | STYGIAN_WINDOW_RESIZABLE |
               STYGIAN_WINDOW_BORDERLESS | STYGIAN_WINDOW_CENTERED,
      .role = STYGIAN_ROLE_MAIN,
  };
  StygianWindow *gl_window = stygian_window_create(&gl_cfg);
  StygianWindow *vk_window = NULL;

  CHECK(gl_window != NULL, "opengl borderless style fixture window created");
  if (gl_window) {
    HWND hwnd = (HWND)stygian_window_native_handle(gl_window);
    LONG_PTR style = (hwnd != NULL) ? GetWindowLongPtr(hwnd, GWL_STYLE) : 0;
    CHECK((style & WS_POPUP) != 0, "opengl borderless main keeps popup style");
    stygian_window_destroy(gl_window);
  }

  vk_window = stygian_window_create(&vk_cfg);
  CHECK(vk_window != NULL, "vulkan borderless style fixture window created");
  if (vk_window) {
    HWND hwnd = (HWND)stygian_window_native_handle(vk_window);
    LONG_PTR style = (hwnd != NULL) ? GetWindowLongPtr(hwnd, GWL_STYLE) : 0;
    CHECK((style & WS_POPUP) != 0, "vulkan borderless main keeps popup style");
    stygian_window_destroy(vk_window);
  }
}

static void test_titlebar_behavior_and_actions(void) {
  StygianWindowConfig cfg = {
      .width = 640,
      .height = 420,
      .title = "stygian_tier2_titlebar_behavior",
      .flags = STYGIAN_WINDOW_OPENGL | STYGIAN_WINDOW_RESIZABLE |
               STYGIAN_WINDOW_BORDERLESS | STYGIAN_WINDOW_CENTERED,
      .role = STYGIAN_ROLE_MAIN,
  };
  StygianWindow *window = stygian_window_create(&cfg);
  StygianTitlebarBehavior behavior = {0};
  StygianTitlebarHints hints = {0};
  StygianTitlebarMenuAction actions[16];
  uint32_t action_count = 0u;
  bool has_maximize = false;
  bool has_fullscreen = false;
  bool has_snap_right = false;
  bool fullscreen_toggled = false;
  bool fullscreen_restored = false;
  bool begin_move_ok = false;
  bool snap_applied = false;

  CHECK(window != NULL, "titlebar behavior fixture window created");
  if (!window)
    return;

  stygian_window_get_titlebar_hints(window, &hints);
  CHECK(hints.button_order == STYGIAN_TITLEBAR_BUTTONS_RIGHT,
        "win32 titlebar hints default to right button order");
  CHECK(hints.supports_hover_menu, "win32 titlebar hints expose hover menu");
  CHECK(hints.supports_snap_actions, "win32 titlebar hints expose snap actions");

  stygian_window_get_titlebar_behavior(window, &behavior);
  CHECK(behavior.double_click_mode == STYGIAN_TITLEBAR_DBLCLICK_MAXIMIZE_RESTORE,
        "titlebar double-click defaults to maximize/restore");

  stygian_window_titlebar_double_click(window);
  pump_window_events(window, 80, 8);
  CHECK(stygian_window_is_maximized(window),
        "titlebar double-click toggles to maximized");
  stygian_window_titlebar_double_click(window);
  pump_window_events(window, 80, 8);
  CHECK(!stygian_window_is_maximized(window),
        "titlebar double-click toggles restore");

  behavior.double_click_mode = STYGIAN_TITLEBAR_DBLCLICK_FULLSCREEN_TOGGLE;
  behavior.hover_menu_enabled = true;
  stygian_window_set_titlebar_behavior(window, &behavior);
  stygian_window_titlebar_double_click(window);
  pump_window_events(window, 60, 8);
  fullscreen_toggled = stygian_window_is_fullscreen(window);
  CHECK(fullscreen_toggled, "fullscreen policy toggles on double-click");
  stygian_window_titlebar_double_click(window);
  pump_window_events(window, 60, 8);
  fullscreen_restored = !stygian_window_is_fullscreen(window);
  CHECK(fullscreen_restored, "fullscreen policy toggles back on double-click");

  action_count = stygian_window_get_titlebar_menu_actions(
      window, actions, (uint32_t)(sizeof(actions) / sizeof(actions[0])));
  CHECK(action_count >= 8u, "titlebar menu exposes native preset action set");
  if (action_count > 0u) {
    uint32_t i;
    for (i = 0u; i < action_count; i++) {
      if (actions[i] == STYGIAN_TITLEBAR_ACTION_MAXIMIZE ||
          actions[i] == STYGIAN_TITLEBAR_ACTION_RESTORE) {
        has_maximize = true;
      }
      if (actions[i] == STYGIAN_TITLEBAR_ACTION_ENTER_FULLSCREEN ||
          actions[i] == STYGIAN_TITLEBAR_ACTION_EXIT_FULLSCREEN) {
        has_fullscreen = true;
      }
      if (actions[i] == STYGIAN_TITLEBAR_ACTION_SNAP_RIGHT) {
        has_snap_right = true;
      }
    }
  }
  CHECK(has_maximize, "titlebar menu includes maximize/restore action");
  CHECK(has_fullscreen, "titlebar menu includes fullscreen action");
  CHECK(has_snap_right, "titlebar menu includes snap action");

  snap_applied = stygian_window_apply_titlebar_menu_action(
      window, STYGIAN_TITLEBAR_ACTION_SNAP_RIGHT);
  CHECK(snap_applied, "snap-right titlebar action applies");
  if (snap_applied) {
    HWND hwnd = (HWND)stygian_window_native_handle(window);
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info;
    RECT rect;
    bool geometry_ok;
    bool right_half_ok = false;
    LONG expected_left;

    memset(&monitor_info, 0, sizeof(monitor_info));
    monitor_info.cbSize = sizeof(monitor_info);
    memset(&rect, 0, sizeof(rect));
    pump_window_events(window, 60, 8);
    geometry_ok =
        (hwnd != NULL) && (monitor != NULL) &&
        (GetMonitorInfo(monitor, &monitor_info) != 0) &&
        (GetWindowRect(hwnd, &rect) != 0);
    CHECK(geometry_ok, "snap-right geometry query succeeds");
    if (geometry_ok) {
      LONG half_width = (monitor_info.rcWork.right - monitor_info.rcWork.left) / 2;
      expected_left = monitor_info.rcWork.right - half_width;
      right_half_ok = (rect.right <= monitor_info.rcWork.right + 1) &&
                      (rect.right >= monitor_info.rcWork.right - 1) &&
                      (rect.left >= expected_left - 1) &&
                      (rect.left <= expected_left + 1);
      CHECK(right_half_ok, "snap-right aligns to monitor work-area right half");
    }
  }

  begin_move_ok = stygian_window_begin_system_move(window);
  CHECK(begin_move_ok, "begin_system_move returns success on win32");
  pump_window_events(window, 30, 8);

  stygian_window_destroy(window);
}
#else
static void test_borderless_maximize_uses_work_area(void) {
  CHECK(true, "borderless maximize work-area check skipped on non-Windows");
}

static void test_borderless_style_routing(void) {
  CHECK(true, "borderless style routing check skipped on non-Windows");
}

static void test_titlebar_behavior_and_actions(void) {
  CHECK(true, "titlebar behavior checks skipped on non-Windows");
}
#endif

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
  test_borderless_maximize_uses_work_area();
  test_borderless_style_routing();
  test_titlebar_behavior_and_actions();

  test_env_destroy(&env);

  if (g_failures == 0) {
    printf("[PASS] tier2 runtime suite complete\n");
    return 0;
  }
  fprintf(stderr, "[FAIL] tier2 runtime suite failures=%d\n", g_failures);
  return 1;
}
