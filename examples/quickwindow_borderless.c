#include "stygian.h"
#include "stygian_window.h"

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_BORDERLESS_BACKEND STYGIAN_BACKEND_VULKAN
#define STYGIAN_BORDERLESS_WINDOW_RENDER_FLAG STYGIAN_WINDOW_VULKAN
#define STYGIAN_BORDERLESS_BACKEND_NAME "Vulkan"
#else
#define STYGIAN_BORDERLESS_BACKEND STYGIAN_BACKEND_OPENGL
#define STYGIAN_BORDERLESS_WINDOW_RENDER_FLAG STYGIAN_WINDOW_OPENGL
#define STYGIAN_BORDERLESS_BACKEND_NAME "OpenGL"
#endif

int main(void) {
  StygianWindowConfig win_cfg = {
      .width = 1100,
      .height = 680,
      .title = "Stygian Borderless Quick Window",
      .flags = STYGIAN_WINDOW_RESIZABLE | STYGIAN_WINDOW_BORDERLESS |
               STYGIAN_BORDERLESS_WINDOW_RENDER_FLAG,
  };
  StygianWindow *window = stygian_window_create(&win_cfg);
  if (!window)
    return 1;

  StygianConfig cfg = {
      .backend = STYGIAN_BORDERLESS_BACKEND,
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
    while (stygian_window_poll_event(window, &event)) {
      if (event.type == STYGIAN_EVENT_CLOSE)
        stygian_window_request_close(window);
    }

    {
      int width;
      int height;
      stygian_window_get_size(window, &width, &height);

      stygian_begin_frame(ctx, width, height);
      stygian_rect(ctx, 0.0f, 0.0f, (float)width, 38.0f, 0.07f, 0.09f, 0.12f,
                   1.0f);
      stygian_rect(ctx, 0.0f, 38.0f, (float)width, (float)height - 38.0f, 0.12f,
                   0.14f, 0.18f, 1.0f);
      if (font) {
        stygian_text(ctx, font, "Borderless Window", 14.0f, 10.0f, 14.0f, 0.96f,
                     0.96f, 0.98f, 1.0f);
        stygian_text(ctx, font, STYGIAN_BORDERLESS_BACKEND_NAME, 180.0f, 10.0f,
                     14.0f, 0.78f, 0.84f, 0.95f, 1.0f);
        stygian_text(ctx, font, "Close with Alt+F4 or window close event",
                     14.0f, 52.0f, 15.0f, 0.9f, 0.92f, 0.95f, 1.0f);
      }
      stygian_end_frame(ctx);
    }
  }

  if (font)
    stygian_font_destroy(ctx, font);
  stygian_destroy(ctx);
  stygian_window_destroy(window);
  return 0;
}
