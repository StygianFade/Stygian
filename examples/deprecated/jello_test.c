// jello_test.c - Dual Window Metaball Melting Test
#include "../backends/stygian_ap.h"
#include "../include/stygian.h"
#include "../window/stygian_window.h"
#include <dwmapi.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

// Disable DWM styling props
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWA_NCRENDERING_POLICY
#define DWMWA_NCRENDERING_POLICY 2
#define DWMNCRP_DISABLED 1
#endif

// Constants
#define GHOST_SIZE 600
#define BODY_W 800
#define BODY_H 500

// State
static StygianContext *ctx_main = NULL;
static StygianContext *ctx_ghost = NULL;
static StygianWindow *win_main = NULL;
static StygianWindow *win_ghost = NULL;
static HWND hwnd_main = NULL;
static HWND hwnd_ghost = NULL;
static StygianFont g_font = 0;

static bool g_dragging = false;
static POINT g_drag_start; // Client pos

// Physics
typedef struct {
  float x, y, vx, vy, tx, ty;
} Spring;
static Spring s_tab = {0};

void update_spring(Spring *s, float dt) {
  float k = 150.0f, d = 12.0f;
  float ax = (k * (s->tx - s->x)) - (d * s->vx);
  float ay = (k * (s->ty - s->y)) - (d * s->vy);
  s->vx += ax * dt;
  s->vy += ay * dt;
  s->x += s->vx * dt;
  s->y += s->vy * dt;
}

LRESULT CALLBACK WndProcMain(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_NCCALCSIZE:
    if (wp)
      return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  case WM_NCHITTEST: {
    // Allow dragging the body
    LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
    if (hit == HTCLIENT)
      return HTCAPTION;
    return hit;
  }
  case WM_LBUTTONDOWN: {
    // Check for tab click (approximated region)
    POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    // If clicking top-left area...
    // Visual Tab is at body relative (50, 50-20) -> (50, 30).
    // Actually, we render Tab at s_tab.x.
    // In main window, s_tab.x might be anything.
    StygianWindow *win = stygian_window_from_native(hwnd);
    int cx = pt.x;
    int cy = pt.y;
    // Simple box check: 0-200, 0-60
    if (cx < 200 && cy < 60) {
      g_dragging = true;
      GetCursorPos(&g_drag_start);
      ScreenToClient(hwnd, &g_drag_start);

      // Show Ghost
      ShowWindow(hwnd_ghost, SW_SHOWNA);
      SetCapture(hwnd);
      return 0;
    }
    break;
  }
  case WM_LBUTTONUP:
    if (g_dragging) {
      g_dragging = false;
      ReleaseCapture();
      ShowWindow(hwnd_ghost, SW_HIDE);
      // Snap tab back spring handled in update
    }
    break;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

// Ghost is pass-through
LRESULT CALLBACK WndProcGhost(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  return DefWindowProc(hwnd, msg, wp, lp);
}

void SetupWindow(HWND hwnd) {
  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
  DWORD policy = DWMNCRP_DISABLED;
  DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy,
                        sizeof(policy));
  DWORD corner = DWMWCP_DONOTROUND;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                        sizeof(corner));
  MARGINS m = {0, 0, 0, 0};
  DwmExtendFrameIntoClientArea(hwnd, &m);
}

void DrawScene(StygianContext *ctx, float local_w, float local_h, float off_x,
               float off_y, float body_screen_x, float body_screen_y,
               bool is_ghost) {
  stygian_begin_frame(ctx, (int)local_w, (int)local_h);
  StygianElement grp = stygian_begin_metaball_group(ctx);

  // Draw Main Body (Mapped from Screen Space to Local Space)
  float main_x = body_screen_x - off_x;
  float main_y = body_screen_y - off_y;

  // Body
  StygianElement b =
      stygian_rect(ctx, main_x, main_y, BODY_W - 100, BODY_H - 100, 0, 0, 0, 0);
  stygian_set_color(ctx, b, 0.2f, 0.2f, 0.22f, 1.0f);
  // Fix: Use generic RECT for metaball children to match shader expectation
  stygian_set_type(ctx, b, STYGIAN_RECT);
  stygian_set_radius(ctx, b, 10, 10, 10, 10);

  // Draw Tab (Relative to this window)
  float tab_local_x = s_tab.x - off_x;
  float tab_local_y = s_tab.y - off_y;

  StygianElement t =
      stygian_rect(ctx, tab_local_x, tab_local_y, 120, 40, 0, 0, 0, 0);
  float tc = g_dragging ? 0.3f : 0.25f;
  stygian_set_color(ctx, t, tc, tc, tc + 0.05f, 1.0f);
  stygian_set_radius(ctx, t, 10, 10, 0, 0);

  stygian_end_metaball_group(ctx, grp);

  // Text overlays (no melt)
  if (g_font) {
    if (!is_ghost) {
      stygian_text(ctx, g_font, "Visual Melt", main_x + 20, main_y + 20, 16.0f,
                   1, 1, 1, 1);
    }
    stygian_text(ctx, g_font, "Drag Me ->", tab_local_x + 20, tab_local_y + 12,
                 14.0f, 1, 1, 1, 1);
  }

  stygian_end_frame(ctx);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  HINSTANCE hInst = GetModuleHandle(NULL);

  // 1. Classes
  WNDCLASSEX wc = {sizeof(WNDCLASSEX),
                   CS_HREDRAW | CS_VREDRAW,
                   WndProcMain,
                   0,
                   0,
                   hInst,
                   NULL,
                   LoadCursor(NULL, IDC_ARROW),
                   NULL,
                   NULL,
                   "MainClass",
                   NULL};
  RegisterClassEx(&wc);
  wc.lpfnWndProc = WndProcGhost;
  wc.lpszClassName = "GhostClass";
  RegisterClassEx(&wc);

  // 2. Windows
  // Main
  hwnd_main = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_LAYERED, "MainClass",
                             "Jello Main", WS_POPUP | WS_VISIBLE, 200, 200,
                             BODY_W, BODY_H, NULL, NULL, hInst, NULL);
  SetupWindow(hwnd_main);
  win_main = stygian_window_from_native(hwnd_main);

  // Ghost (Hidden initially)
  // WS_EX_TRANSPARENT allows click-through to under windows if needed, but we
  // might want to drag it. Actually if we capture mouse on Main, we drive Ghost
  // position from Main.
  hwnd_ghost = CreateWindowEx(
      WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT, "GhostClass",
      "Ghost", WS_POPUP, 0, 0, GHOST_SIZE, GHOST_SIZE, NULL, NULL, hInst, NULL);
  SetupWindow(hwnd_ghost);
  win_ghost = stygian_window_from_native(hwnd_ghost);

  // 3. Contexts
  StygianConfig cfg = {.backend = STYGIAN_BACKEND_OPENGL,
                       .max_elements = 1024,
                       .window = win_main};
  ctx_main = stygian_create(&cfg);
  stygian_set_vsync(ctx_main, true); // Sync on main

  cfg.window = win_ghost;
  ctx_ghost = stygian_create(&cfg);

  // Load font
  char exe_dir[MAX_PATH];
  GetModuleFileName(NULL, exe_dir, MAX_PATH);
  strrchr(exe_dir, '\\')[1] = 0;
  char png[MAX_PATH], json[MAX_PATH];
  sprintf(png, "%s..\\assets\\atlas.png", exe_dir);
  sprintf(json, "%s..\\assets\\atlas.json", exe_dir);
  g_font = stygian_font_load(ctx_main, png, json);

  // Share font texture? No mechanism yet.
  // We must load it again for ghost context or just skip text on ghost.
  // Let's load it again for now (wasteful but easy)
  stygian_font_load(ctx_ghost, png, json);

  bool running = true;
  MSG msg;

  // Init physics
  RECT rc;
  GetWindowRect(hwnd_main, &rc);
  s_tab.x = (float)rc.left + 50 + 20;
  s_tab.y = (float)rc.top + 50 - 20; // Inside body initially
  s_tab.tx = s_tab.x;
  s_tab.ty = s_tab.y;

  while (running) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT)
        running = false;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Update Logic
    POINT cursor;
    GetCursorPos(&cursor);
    RECT rc_main;
    GetWindowRect(hwnd_main, &rc_main);

    if (g_dragging) {
      // Target is mouse pos centered on tab
      s_tab.tx = (float)cursor.x - 60; // Center offset
      s_tab.ty = (float)cursor.y - 20;

      // Move Ghost Window to center on tab
      // Ghost is GHOST_SIZE x GHOST_SIZE.
      // We want Ghost Center to be Tab Center.
      int gx = (int)s_tab.x - GHOST_SIZE / 2 + 60; // +60 is half tab width
      int gy = (int)s_tab.y - GHOST_SIZE / 2 + 20;
      SetWindowPos(hwnd_ghost, HWND_TOPMOST, gx, gy, GHOST_SIZE, GHOST_SIZE,
                   SWP_NOACTIVATE);
    } else {
      // Snap to Main Body
      s_tab.tx = (float)rc_main.left + 50 + 20; // Indented
      s_tab.ty = (float)rc_main.top + 50 - 20;  // Sticking out slightly
    }
    update_spring(&s_tab, 0.016f);

    // Check errors
    // stygian_window_process_events(win_main); // TODO needed?

    // Render Main
    stygian_window_make_current(win_main);
    RECT rc_m;
    GetWindowRect(hwnd_main, &rc_m);
    float body_sx = (float)rc_m.left + 50.0f;
    float body_sy = (float)rc_m.top + 50.0f;
    DrawScene(ctx_main, BODY_W, BODY_H, (float)rc_m.left, (float)rc_m.top,
              body_sx, body_sy, false);
    stygian_window_swap_buffers(win_main);

    // Render Ghost (only if dragging/visible)
    if (g_dragging) {
      stygian_window_make_current(win_ghost);
      RECT rc_ghost;
      GetWindowRect(hwnd_ghost, &rc_ghost);
      DrawScene(ctx_ghost, GHOST_SIZE, GHOST_SIZE, (float)rc_ghost.left,
                (float)rc_ghost.top, body_sx, body_sy, true);
      stygian_window_swap_buffers(win_ghost);
    }

    Sleep(16);
  }

  stygian_destroy(ctx_main);
  stygian_destroy(ctx_ghost);
  return 0;
}
