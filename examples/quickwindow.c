#include "stygian.h"
#include "stygian_window.h"

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_QUICK_BACKEND STYGIAN_BACKEND_VULKAN
#define STYGIAN_QUICK_WINDOW_RENDER_FLAG STYGIAN_WINDOW_VULKAN
#define STYGIAN_QUICK_BACKEND_NAME "Vulkan"
#else
#define STYGIAN_QUICK_BACKEND STYGIAN_BACKEND_OPENGL
#define STYGIAN_QUICK_WINDOW_RENDER_FLAG STYGIAN_WINDOW_OPENGL
#define STYGIAN_QUICK_BACKEND_NAME "OpenGL"
#endif

int main(void) {
  StygianWindowConfig win_cfg = {
      .width = 1280,
      .height = 720,
      .title = "Stygian Quick Window",
      .flags = STYGIAN_WINDOW_RESIZABLE | STYGIAN_QUICK_WINDOW_RENDER_FLAG,
  };
  StygianWindow *window = stygian_window_create(&win_cfg);
  if (!window)
    return 1;

  StygianConfig cfg = {
      .backend = STYGIAN_QUICK_BACKEND,
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
      stygian_rect(ctx, 0.0f, 0.0f, (float)width, 40.0f, 0.08f, 0.1f, 0.13f,
                   1.0f);
      stygian_rect(ctx, 10.0f, 10.0f, 200.0f, 100.0f, 0.2f, 0.3f, 0.8f, 1.0f);
      if (font) {
        stygian_text(ctx, font, STYGIAN_QUICK_BACKEND_NAME, 16.0f, 12.0f, 14.0f,
                     0.85f, 0.9f, 0.98f, 1.0f);
        stygian_text(ctx, font, "Hello", 20.0f, 50.0f, 16.0f, 1.0f, 1.0f, 1.0f,
                     1.0f);
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
