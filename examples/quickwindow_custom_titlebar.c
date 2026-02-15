#include "stygian.h"
#include "stygian_widgets.h"
#include "stygian_window.h"
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_TITLEBAR_BACKEND STYGIAN_BACKEND_VULKAN
#define STYGIAN_TITLEBAR_WINDOW_RENDER_FLAG STYGIAN_WINDOW_VULKAN
#define STYGIAN_TITLEBAR_BACKEND_NAME "Vulkan"
#else
#define STYGIAN_TITLEBAR_BACKEND STYGIAN_BACKEND_OPENGL
#define STYGIAN_TITLEBAR_WINDOW_RENDER_FLAG STYGIAN_WINDOW_OPENGL
#define STYGIAN_TITLEBAR_BACKEND_NAME "OpenGL"
#endif

static const char *
stygian_demo_titlebar_action_label(StygianTitlebarMenuAction action) {
  switch (action) {
  case STYGIAN_TITLEBAR_ACTION_RESTORE:
    return "Restore";
  case STYGIAN_TITLEBAR_ACTION_MAXIMIZE:
    return "Maximize";
  case STYGIAN_TITLEBAR_ACTION_ENTER_FULLSCREEN:
    return "Enter Fullscreen";
  case STYGIAN_TITLEBAR_ACTION_EXIT_FULLSCREEN:
    return "Exit Fullscreen";
  case STYGIAN_TITLEBAR_ACTION_SNAP_LEFT:
    return "Snap Left";
  case STYGIAN_TITLEBAR_ACTION_SNAP_RIGHT:
    return "Snap Right";
  case STYGIAN_TITLEBAR_ACTION_SNAP_TOP_LEFT:
    return "Snap Top Left";
  case STYGIAN_TITLEBAR_ACTION_SNAP_TOP_RIGHT:
    return "Snap Top Right";
  case STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_LEFT:
    return "Snap Bottom Left";
  case STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_RIGHT:
    return "Snap Bottom Right";
  default:
    return "Action";
  }
}

static uint64_t stygian_demo_now_ms(void) {
#ifdef _WIN32
  return (uint64_t)GetTickCount64();
#elif defined(CLOCK_MONOTONIC)
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
  }
#endif
  return (uint64_t)(clock() * 1000u / CLOCKS_PER_SEC);
}

static void stygian_demo_menu_rect(int frame_width, int frame_height,
                                   bool buttons_left, float max_x,
                                   float button_w, float title_height,
                                   const StygianContextMenu *menu,
                                   uint32_t item_count, float *out_x,
                                   float *out_y, float *out_w, float *out_h) {
  float menu_w = menu && menu->w > 60.0f ? menu->w : 180.0f;
  float item_h = menu && menu->item_h > 18.0f ? menu->item_h : 28.0f;
  float menu_h = item_h * (float)item_count + 8.0f;
  float menu_x = buttons_left ? max_x : (max_x + button_w - menu_w);
  float menu_y = title_height + 2.0f;

  if (menu_x + menu_w > (float)frame_width)
    menu_x = (float)frame_width - menu_w;
  if (menu_y + menu_h > (float)frame_height)
    menu_y = (float)frame_height - menu_h;
  if (menu_x < 0.0f)
    menu_x = 0.0f;
  if (menu_y < 0.0f)
    menu_y = 0.0f;

  if (out_x)
    *out_x = menu_x;
  if (out_y)
    *out_y = menu_y;
  if (out_w)
    *out_w = menu_w;
  if (out_h)
    *out_h = menu_h;
}

int main(void) {
  const float default_title_height = 36.0f;
  const float default_button_y = 6.0f;
  const float default_button_w = 28.0f;
  const float default_button_h = 24.0f;
  const float default_button_gap = 6.0f;
  StygianContextMenu maximize_menu = {.open = false, .w = 220.0f, .item_h = 28.0f};
  StygianTitlebarBehavior titlebar_behavior = {
      .double_click_mode = STYGIAN_TITLEBAR_DBLCLICK_MAXIMIZE_RESTORE,
      .hover_menu_enabled = true,
  };
  const uint32_t menu_hover_open_delay_ms = 140u;
  const uint32_t menu_hover_close_grace_ms = 220u;
  bool menu_hover_armed = false;
  uint64_t menu_hover_arm_since_ms = 0u;
  uint64_t menu_last_inside_ms = 0u;
  bool first_frame = true;
  int frame_width = 1200;
  int frame_height = 760;

  StygianWindowConfig win_cfg = {
      .width = frame_width,
      .height = frame_height,
      .title = "Stygian Custom Titlebar",
      .flags = STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_BORDERLESS |
               STYGIAN_TITLEBAR_WINDOW_RENDER_FLAG,
  };
  StygianWindow *window = stygian_window_create(&win_cfg);
  if (!window)
    return 1;
  stygian_window_set_titlebar_behavior(window, &titlebar_behavior);
  stygian_window_get_titlebar_behavior(window, &titlebar_behavior);

  StygianConfig cfg = {
      .backend = STYGIAN_TITLEBAR_BACKEND,
      .window = window,
  };
  StygianContext *ctx = stygian_create(&cfg);
  if (!ctx) {
    stygian_window_destroy(window);
    return 1;
  }

  StygianFont font =
      stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");

  while (!stygian_window_should_close(window)) {
    StygianEvent event;
    StygianTitlebarHints titlebar_hints;
    StygianTitlebarMenuAction menu_actions[16];
    uint32_t menu_action_count = 0u;
    bool event_mutated = false;
    bool event_requested_repaint = false;
    bool event_requested_eval = false;
    bool buttons_left;
    float title_height;
    float button_w;
    float button_h;
    float button_gap;
    float button_y;
    float close_x;
    float max_x;
    float min_x;
    float controls_start;
    float controls_end;
    float title_text_x;
    uint32_t wait_ms = stygian_next_repaint_wait_ms(ctx, 250u);

    stygian_window_get_titlebar_hints(window, &titlebar_hints);
    stygian_window_get_titlebar_behavior(window, &titlebar_behavior);
    title_height = titlebar_hints.recommended_titlebar_height > 0.0f
                       ? titlebar_hints.recommended_titlebar_height
                       : default_title_height;
    button_w = titlebar_hints.recommended_button_width > 0.0f
                   ? titlebar_hints.recommended_button_width
                   : default_button_w;
    button_h = titlebar_hints.recommended_button_height > 0.0f
                   ? titlebar_hints.recommended_button_height
                   : default_button_h;
    button_gap = titlebar_hints.recommended_button_gap > 0.0f
                     ? titlebar_hints.recommended_button_gap
                     : default_button_gap;
    button_y = (title_height - button_h) * 0.5f;
    if (button_y < 2.0f)
      button_y = default_button_y;
    buttons_left =
        titlebar_hints.button_order == STYGIAN_TITLEBAR_BUTTONS_LEFT;
    if (buttons_left) {
      close_x = button_gap;
      min_x = close_x + button_w + button_gap;
      max_x = min_x + button_w + button_gap;
      controls_start = 0.0f;
      controls_end = max_x + button_w + button_gap;
      title_text_x = controls_end + 10.0f;
    } else {
      close_x = (float)frame_width - button_gap - button_w;
      max_x = close_x - button_gap - button_w;
      min_x = max_x - button_gap - button_w;
      controls_start = min_x - button_gap;
      if (controls_start < 0.0f)
        controls_start = 0.0f;
      controls_end = (float)frame_width;
      title_text_x = 14.0f;
    }

    stygian_widgets_begin_frame(ctx);

    while (stygian_window_poll_event(window, &event)) {
      StygianWidgetEventImpact impact =
          stygian_widgets_process_event_ex(ctx, &event);
      if (impact & STYGIAN_IMPACT_MUTATED_STATE)
        event_mutated = true;
      if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
        event_requested_repaint = true;
      if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
        event_requested_eval = true;

      if (event.type == STYGIAN_EVENT_CLOSE) {
        stygian_window_request_close(window);
      } else if (event.type == STYGIAN_EVENT_RESIZE) {
        frame_width = event.resize.width;
        frame_height = event.resize.height;
        event_mutated = true;
      } else if (event.type == STYGIAN_EVENT_MOUSE_DOWN &&
                 event.mouse_button.button == STYGIAN_MOUSE_LEFT) {
        bool in_title_bar =
            event.mouse_button.y >= 0 && event.mouse_button.y < (int)title_height;
        bool in_controls = event.mouse_button.y >= (int)button_y &&
                           event.mouse_button.y < (int)(button_y + button_h) &&
                           event.mouse_button.x >= (int)controls_start &&
                           event.mouse_button.x < (int)controls_end;
        if (in_title_bar && !in_controls) {
          if (event.mouse_button.clicks >= 2) {
            stygian_window_titlebar_double_click(window);
          } else {
            stygian_window_begin_system_move(window);
          }
          event_mutated = true;
          event_requested_repaint = true;
        }
      }
    }

    if (!first_frame && !event_mutated && !event_requested_repaint &&
        !event_requested_eval) {
      if (stygian_window_wait_event_timeout(window, &event, wait_ms)) {
        StygianWidgetEventImpact impact =
            stygian_widgets_process_event_ex(ctx, &event);
        if (impact & STYGIAN_IMPACT_MUTATED_STATE)
          event_mutated = true;
        if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
          event_requested_repaint = true;
        if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
          event_requested_eval = true;

        if (event.type == STYGIAN_EVENT_CLOSE) {
          stygian_window_request_close(window);
        } else if (event.type == STYGIAN_EVENT_RESIZE) {
          frame_width = event.resize.width;
          frame_height = event.resize.height;
          event_mutated = true;
        } else if (event.type == STYGIAN_EVENT_MOUSE_DOWN &&
                   event.mouse_button.button == STYGIAN_MOUSE_LEFT) {
          bool in_title_bar =
              event.mouse_button.y >= 0 && event.mouse_button.y < (int)title_height;
          bool in_controls = event.mouse_button.y >= (int)button_y &&
                             event.mouse_button.y < (int)(button_y + button_h) &&
                             event.mouse_button.x >= (int)controls_start &&
                             event.mouse_button.x < (int)controls_end;
          if (in_title_bar && !in_controls) {
            if (event.mouse_button.clicks >= 2) {
              stygian_window_titlebar_double_click(window);
            } else {
              stygian_window_begin_system_move(window);
            }
            event_mutated = true;
            event_requested_repaint = true;
          }
        }
      }
    }

    {
      bool repaint_pending = stygian_has_pending_repaint(ctx);
      bool widget_repaint_pending = stygian_widgets_wants_repaint();
      bool render_frame = first_frame || event_mutated ||
                          event_requested_repaint || repaint_pending ||
                          widget_repaint_pending;
      bool eval_only_frame = (!render_frame && event_requested_eval);

      if (!render_frame && !eval_only_frame)
        continue;

      first_frame = false;
      stygian_window_get_size(window, &frame_width, &frame_height);
      stygian_begin_frame_intent(
          ctx, frame_width, frame_height,
          eval_only_frame ? STYGIAN_FRAME_EVAL_ONLY : STYGIAN_FRAME_RENDER);
    }

    stygian_rect(ctx, 0.0f, 0.0f, (float)frame_width, title_height, 0.08f, 0.1f,
                 0.13f, 1.0f);
    stygian_rect(ctx, 0.0f, title_height, (float)frame_width,
                 (float)frame_height - title_height, 0.12f, 0.14f, 0.18f, 1.0f);

    if (font) {
      int mouse_x = 0;
      int mouse_y = 0;
      uint64_t now_ms = stygian_demo_now_ms();
      bool hover_max;
      bool pointer_in_menu = false;
      float menu_w;
      float menu_h;
      float menu_x;
      float menu_y;
      bool pointer_button_down;
      const char *max_label = stygian_window_is_maximized(window) ? "R" : "[]";

      stygian_mouse_pos(window, &mouse_x, &mouse_y);
      menu_action_count = stygian_window_get_titlebar_menu_actions(
          window, menu_actions, (uint32_t)(sizeof(menu_actions) / sizeof(menu_actions[0])));
      pointer_button_down = stygian_mouse_down(window, STYGIAN_MOUSE_LEFT) ||
                            stygian_mouse_down(window, STYGIAN_MOUSE_RIGHT);
      hover_max = ((float)mouse_x >= max_x && (float)mouse_x < (max_x + button_w) &&
                   (float)mouse_y >= button_y &&
                   (float)mouse_y < (button_y + button_h));
      if (hover_max && titlebar_behavior.hover_menu_enabled &&
          titlebar_hints.supports_hover_menu && menu_action_count > 0u) {
        if (!menu_hover_armed) {
          menu_hover_armed = true;
          menu_hover_arm_since_ms = now_ms;
        }
      } else {
        menu_hover_armed = false;
      }

      if (!maximize_menu.open && menu_hover_armed &&
          (now_ms - menu_hover_arm_since_ms) >= menu_hover_open_delay_ms) {
        maximize_menu.open = true;
        menu_last_inside_ms = now_ms;
      }

      if (maximize_menu.open && menu_action_count == 0u) {
        maximize_menu.open = false;
      }
      if (maximize_menu.open && menu_action_count > 0u) {
        stygian_demo_menu_rect(frame_width, frame_height, buttons_left, max_x,
                               button_w, title_height, &maximize_menu,
                               menu_action_count, &menu_x, &menu_y, &menu_w,
                               &menu_h);
        pointer_in_menu = ((float)mouse_x >= menu_x && (float)mouse_x <= menu_x + menu_w &&
                           (float)mouse_y >= menu_y && (float)mouse_y <= menu_y + menu_h);
        maximize_menu.x = menu_x;
        maximize_menu.y = menu_y;
      }

      if (hover_max || pointer_in_menu || pointer_button_down) {
        menu_last_inside_ms = now_ms;
      }
      if (maximize_menu.open && !hover_max && !pointer_in_menu &&
          !pointer_button_down) {
        if ((now_ms - menu_last_inside_ms) >= menu_hover_close_grace_ms) {
          maximize_menu.open = false;
        }
      }

      stygian_text(ctx, font, "Custom Titlebar", title_text_x, 10.0f, 14.0f, 0.96f,
                   0.96f, 0.98f, 1.0f);
      stygian_text(ctx, font, STYGIAN_TITLEBAR_BACKEND_NAME, title_text_x + 140.0f, 10.0f,
                   14.0f, 0.78f, 0.84f, 0.95f, 1.0f);

      if (stygian_button(ctx, font, "-", min_x, button_y, button_w, button_h)) {
        stygian_window_minimize(window);
        maximize_menu.open = false;
      }
      if (stygian_button(ctx, font, max_label, max_x, button_y, button_w,
                         button_h)) {
        if (stygian_window_is_maximized(window))
          stygian_window_restore(window);
        else
          stygian_window_maximize(window);
        maximize_menu.open = false;
      }
      if (stygian_button(ctx, font, "X", close_x, button_y, button_w, button_h)) {
        stygian_window_request_close(window);
        maximize_menu.open = false;
      }

      if (maximize_menu.open && menu_action_count > 0u &&
          stygian_context_menu_begin(ctx, font, &maximize_menu,
                                     (int)menu_action_count)) {
        uint32_t action_index;
        for (action_index = 0u; action_index < menu_action_count; ++action_index) {
          const char *label =
              stygian_demo_titlebar_action_label(menu_actions[action_index]);
          if (stygian_context_menu_item(ctx, font, &maximize_menu, label,
                                        (int)action_index)) {
            stygian_window_apply_titlebar_menu_action(window,
                                                      menu_actions[action_index]);
            event_mutated = true;
            event_requested_repaint = true;
          }
        }
        stygian_context_menu_end(ctx, &maximize_menu);
      }

      stygian_text(ctx, font, "Drag empty title area to move window", 16.0f,
                   title_height + 14.0f, 15.0f, 0.9f, 0.92f, 0.95f, 1.0f);
    }

    stygian_end_frame(ctx);
    stygian_widgets_commit_regions();
  }

  if (font)
    stygian_font_destroy(ctx, font);
  stygian_destroy(ctx);
  stygian_window_destroy(window);
  return 0;
}
