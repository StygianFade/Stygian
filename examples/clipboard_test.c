// clipboard_test.c - Universal Clipboard Verification
#include "../backends/stygian_ap.h"
#include "../include/stygian_clipboard.h"

// ... (Rest of includes)

// ...
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_window.h" // For clipboard API
#include <stdio.h>
#include <stdlib.h> // for free
#include <string.h>
#include <windows.h> // For GetModuleFileName

// Simple buffer for text area
static char g_text_buffer[4096] = "Write something here and press Ctrl+C, or "
                                  "copy text from outside and press Ctrl+V...";
static StygianTextArea g_text_state = {.x = 50,
                                       .y = 50,
                                       .w = 500,
                                       .h = 300,
                                       .buffer = g_text_buffer,
                                       .buffer_size = 4096,
                                       .scroll_y = 0};

static char g_status[256] = "Status: Ready";

int main(int argc, char **argv) {
  StygianWindowConfig wcfg = {.width = 800,
                              .height = 600,
                              .title = "Universal Clipboard Test",
                              .flags = 0};
  StygianWindow *win = stygian_window_create(&wcfg);

  StygianConfig cfg = {
      .backend = STYGIAN_BACKEND_OPENGL, .max_elements = 1024, .window = win};
  StygianContext *ctx = stygian_create(&cfg);

  // Load font
  char exe_dir[1024];
  GetModuleFileName(NULL, exe_dir, 1024);
  strrchr(exe_dir, '\\')[1] = 0;
  char png[1024], json[1024];
  sprintf(png, "%s..\\assets\\atlas.png", exe_dir);
  sprintf(json, "%s..\\assets\\atlas.json", exe_dir);
  StygianFont font = stygian_font_load(ctx, png, json);

  while (!stygian_window_should_close(win)) {
    stygian_window_process_events(win);

    stygian_widgets_begin_frame(ctx); // Clear old events, prepare frame

    // Pass events to widgets
    StygianEvent e;
    while (stygian_window_poll_event(win, &e)) {
      stygian_widgets_process_event(ctx, &e);
    }

    stygian_begin_frame(ctx, 800, 600);

    stygian_rect_rounded(ctx, 10, 10, 780, 580, 0.2f, 0.2f, 0.2f, 1.0f, 10.0f);
    stygian_text(ctx, font, "Universal Clipboard Test Room", 20, 20, 24.0f, 1,
                 1, 1, 1);
    stygian_text(ctx, font, g_status, 20, 550, 16.0f, 0.8f, 1.0f, 0.8f, 1.0f);

    // Text Area
    bool changed = stygian_text_area(ctx, font, &g_text_state);

    // ========================================================================
    // Clipboard Shelf (History Visualization)
    // ========================================================================
    float shelf_x = 560;
    float shelf_y = 50;
    float shelf_w = 220;

    stygian_text(ctx, font, "History Shelf", shelf_x, shelf_y - 25, 20.0f, 0.9f,
                 0.9f, 0.9f, 1.0f);

    int cnt = stygian_clipboard_history_count(ctx);
    for (int i = 0; i < cnt && i < 10; i++) {
      const char *txt = stygian_clipboard_history_get(ctx, i);
      if (!txt)
        continue;

      char label[32];
      strncpy(label, txt, 20);
      label[20] = 0;
      if (strlen(txt) > 20)
        strcat(label, "...");

      float by = shelf_y + (i * 45);
      if (stygian_button(ctx, font, label, shelf_x, by, shelf_w, 40)) {
        // Promote to active clipboard
        stygian_clipboard_push(ctx, txt, NULL);
        sprintf(g_status, "Status: Promoted history item %d", i);
      }
    }

    float btn_y = shelf_y + (10 * 45) + 20;

    // Manual Clipboard Buttons (To test API directly)
    if (stygian_button(ctx, font, "Force Copy (API)", shelf_x, btn_y, shelf_w,
                       40)) {
      stygian_clipboard_push(ctx, g_text_buffer, NULL);
      sprintf(g_status, "Status: Forced API Copy of %zd bytes",
              strlen(g_text_buffer));
    }

    if (stygian_button(ctx, font, "Force Paste (API)", shelf_x, btn_y + 50,
                       shelf_w, 40)) {
      char *txt = stygian_clipboard_pop(ctx);
      if (txt) {
        // Determine length to append
        int curr_len = strlen(g_text_buffer);
        int paste_len = strlen(txt);
        if (curr_len + paste_len < 4096) {
          strcat(g_text_buffer, txt);
          sprintf(g_status, "Status: Appended %d chars from clipboard",
                  paste_len);
        } else {
          sprintf(g_status, "Status: Clipboard too large to append!");
        }
        free(txt);
      } else {
        sprintf(g_status, "Status: Clipboard Empty or Failed");
      }
    }

    stygian_end_frame(ctx);
    stygian_window_swap_buffers(win);
  }

  stygian_destroy(ctx);
  // stygian_window_destroy(win); // missing from API? check headers
  return 0;
}
