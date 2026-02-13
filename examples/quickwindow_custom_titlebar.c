#include "stygian.h"
#include "stygian_widgets.h"
#include "stygian_window.h"

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_TITLEBAR_BACKEND STYGIAN_BACKEND_VULKAN
#define STYGIAN_TITLEBAR_WINDOW_RENDER_FLAG STYGIAN_WINDOW_VULKAN
#define STYGIAN_TITLEBAR_BACKEND_NAME "Vulkan"
#else
#define STYGIAN_TITLEBAR_BACKEND STYGIAN_BACKEND_OPENGL
#define STYGIAN_TITLEBAR_WINDOW_RENDER_FLAG STYGIAN_WINDOW_OPENGL
#define STYGIAN_TITLEBAR_BACKEND_NAME "OpenGL"
#endif

int main(void) {
  const float title_height = 36.0f;
  const float button_y = 6.0f;
  const float button_w = 28.0f;
  const float button_h = 24.0f;
  const float button_gap = 6.0f;
  bool dragging_title = false;
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
    stygian_widgets_begin_frame(ctx);

    while (stygian_window_poll_event(window, &event)) {
      stygian_widgets_process_event(ctx, &event);

      if (event.type == STYGIAN_EVENT_CLOSE) {
        stygian_window_request_close(window);
      } else if (event.type == STYGIAN_EVENT_RESIZE) {
        frame_width = event.resize.width;
        frame_height = event.resize.height;
      } else if (event.type == STYGIAN_EVENT_MOUSE_DOWN &&
                 event.mouse_button.button == STYGIAN_MOUSE_LEFT) {
        float controls_start =
            (float)frame_width - (button_w * 3.0f) - (button_gap * 4.0f);
        bool in_title_bar =
            event.mouse_button.y >= 0 && event.mouse_button.y < (int)title_height;
        bool in_controls =
            event.mouse_button.x >= (int)controls_start && event.mouse_button.x < frame_width;
        if (in_title_bar && !in_controls)
          dragging_title = true;
      } else if (event.type == STYGIAN_EVENT_MOUSE_UP &&
                 event.mouse_button.button == STYGIAN_MOUSE_LEFT) {
        dragging_title = false;
      } else if (event.type == STYGIAN_EVENT_MOUSE_MOVE && dragging_title) {
        int win_x;
        int win_y;
        stygian_window_get_position(window, &win_x, &win_y);
        stygian_window_set_position(window, win_x + event.mouse_move.dx,
                                    win_y + event.mouse_move.dy);
      }
    }

    if (!stygian_mouse_down(window, STYGIAN_MOUSE_LEFT))
      dragging_title = false;

    stygian_window_get_size(window, &frame_width, &frame_height);
    stygian_begin_frame(ctx, frame_width, frame_height);

    stygian_rect(ctx, 0.0f, 0.0f, (float)frame_width, title_height, 0.08f, 0.1f,
                 0.13f, 1.0f);
    stygian_rect(ctx, 0.0f, title_height, (float)frame_width,
                 (float)frame_height - title_height, 0.12f, 0.14f, 0.18f, 1.0f);

    if (font) {
      float close_x = (float)frame_width - button_gap - button_w;
      float max_x = close_x - button_gap - button_w;
      float min_x = max_x - button_gap - button_w;
      const char *max_label = stygian_window_is_maximized(window) ? "R" : "[]";

      stygian_text(ctx, font, "Custom Titlebar", 14.0f, 10.0f, 14.0f, 0.96f,
                   0.96f, 0.98f, 1.0f);
      stygian_text(ctx, font, STYGIAN_TITLEBAR_BACKEND_NAME, 150.0f, 10.0f,
                   14.0f, 0.78f, 0.84f, 0.95f, 1.0f);

      if (stygian_button(ctx, font, "_", min_x, button_y, button_w, button_h))
        stygian_window_minimize(window);
      if (stygian_button(ctx, font, max_label, max_x, button_y, button_w,
                         button_h)) {
        if (stygian_window_is_maximized(window))
          stygian_window_restore(window);
        else
          stygian_window_maximize(window);
      }
      if (stygian_button(ctx, font, "X", close_x, button_y, button_w, button_h))
        stygian_window_request_close(window);

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
