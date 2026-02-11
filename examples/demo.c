// stygian_demo.c - Proper Stygian Library Demo
// Demonstrates GPU SDF UI with text, animations, and metaball menu bar
#include "../backends/stygian_ap.h"
#include "../include/stygian.h"
#include "../window/stygian_window.h"
#include <dwmapi.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

#ifdef STYGIAN_DEMO_VULKAN
#define STYGIAN_DEMO_BACKEND STYGIAN_BACKEND_VULKAN
#else
#define STYGIAN_DEMO_BACKEND STYGIAN_BACKEND_OPENGL
#endif


// DWM constants for disabling Windows 11 styling
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWA_NCRENDERING_POLICY
#define DWMWA_NCRENDERING_POLICY 2
#endif
#ifndef DWMNCRP_DISABLED
#define DWMNCRP_DISABLED 1
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Layout constants
#define TITLEBAR_HEIGHT 46
#define BTN_SIZE 25
#define BTN_SPACING 45
#define BTN_RADIUS 4.0f
#define CORNER_RADIUS 10.0f
#define TARGET_FRAME_TIME 16.667f // ~60 FPS in ms

// Global state for WndProc access during resize
static StygianContext *g_ctx = NULL;
static StygianWindow *g_win = NULL; // Window wrapper
static StygianFont g_font = 0;
static int g_hovered_button = 0;
static float g_time = 0.0f;
static bool g_in_resize = false; // Track if we're in a resize operation

// Menu animation state (like stygian_core.c)
static float g_menu_anim = 0.0f;         // 0=retracted, 1=expanded
static int g_mouse_x = 0, g_mouse_y = 0; // Screen mouse position
static ULONGLONG g_menu_interact_time = 0;

// Forward declaration
static void render_frame(void);

// Window procedure for custom hit testing and live resize
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  // Extend client area into entire window frame
  case WM_NCCALCSIZE:
    if (wp == TRUE) {
      NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *)lp;
      // Adjust client rect to cover entire window - no frame
      DefWindowProc(hwnd, msg, wp, lp);
      // Reset to full window size (eliminate non-client area)
      params->rgrc[0].left = params->rgrc[0].left;
      params->rgrc[0].top = params->rgrc[0].top;
      params->rgrc[0].right = params->rgrc[0].right;
      params->rgrc[0].bottom = params->rgrc[0].bottom;
      return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);

  case WM_NCHITTEST: {
    LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
    if (hit == HTCLIENT) {
      POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      ScreenToClient(hwnd, &pt);
      RECT rc;
      GetClientRect(hwnd, &rc);
      int w = rc.right;
      int h = rc.bottom;

      // Corner resize (must check before edges)
      int border = 8;
      if (pt.x < border && pt.y < border)
        return HTTOPLEFT;
      if (pt.x > w - border && pt.y < border)
        return HTTOPRIGHT;
      if (pt.x < border && pt.y > h - border)
        return HTBOTTOMLEFT;
      if (pt.x > w - border && pt.y > h - border)
        return HTBOTTOMRIGHT;

      // Edge resize
      if (pt.x < border)
        return HTLEFT;
      if (pt.x > w - border)
        return HTRIGHT;
      if (pt.y < border)
        return HTTOP;
      if (pt.y > h - border)
        return HTBOTTOM;

      // Titlebar drag
      if (pt.y < TITLEBAR_HEIGHT) {
        if (pt.x >= w - 160)
          return HTCLIENT; // Buttons
        if (pt.x <= 450)
          return HTCLIENT; // Menu
        return HTCAPTION;
      }
    }
    return hit;
  }

  // Prevent Windows from painting non-client area (eliminates white border)
  case WM_NCPAINT:
    return 0;

  case WM_ENTERSIZEMOVE:
    g_in_resize = true;
    return 0;

  case WM_EXITSIZEMOVE:
    g_in_resize = false;
    return 0;

  case WM_SIZE: {
    // Set window region to rounded rectangle for clipped corners
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int radius = CORNER_RADIUS * 2; // Region uses diameter
    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, radius, radius);
    SetWindowRgn(hwnd, rgn, TRUE);

    // Render during resize
    if (g_ctx && g_in_resize) {
      render_frame();
    }
    return 0;
  }

  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    // Don't render here - let main loop handle it
    return 0;
  }

  case WM_MOUSEMOVE: {
    int mx = GET_X_LPARAM(lp);
    int my = GET_Y_LPARAM(lp);
    g_mouse_x = mx;
    g_mouse_y = my;

    // Menu proximity detection (trigger when near top)
    if (my < 40) {
      g_menu_interact_time = GetTickCount64();
    }

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right;

    int old = g_hovered_button;
    g_hovered_button = 0;

    int btn_y = 10;
    int base_x = w - 18;
    int close_x = base_x - BTN_SIZE;
    int max_x = close_x - BTN_SPACING;
    int min_x = max_x - BTN_SPACING;

    if (my >= btn_y && my < btn_y + BTN_SIZE) {
      if (mx >= close_x && mx < close_x + BTN_SIZE)
        g_hovered_button = 3;
      else if (mx >= max_x && mx < max_x + BTN_SIZE)
        g_hovered_button = 2;
      else if (mx >= min_x && mx < min_x + BTN_SIZE)
        g_hovered_button = 1;
    }

    (void)old; // Hover state change handled in render
    break;
  }

  case WM_LBUTTONDOWN: {
    int mx = GET_X_LPARAM(lp);
    int my = GET_Y_LPARAM(lp);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right;

    int btn_y = 10;
    int base_x = w - 18;
    int close_x = base_x - BTN_SIZE;
    int max_x = close_x - BTN_SPACING;
    int min_x = max_x - BTN_SPACING;

    if (my >= btn_y && my < btn_y + BTN_SIZE) {
      if (mx >= close_x && mx < close_x + BTN_SIZE)
        PostQuitMessage(0);
      else if (mx >= max_x && mx < max_x + BTN_SIZE)
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
      else if (mx >= min_x && mx < min_x + BTN_SIZE)
        ShowWindow(hwnd, SW_MINIMIZE);
    }
    break;
  }

  case WM_CLOSE:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProc(hwnd, msg, wp, lp);
}

// Render a single frame
static void render_frame(void) {
  if (!g_ctx || !g_win)
    return;

  HWND hwnd = (HWND)stygian_window_native_handle(g_win);
  RECT rc;
  GetClientRect(hwnd, &rc);
  int width = rc.right;
  int height = rc.bottom;

  if (width <= 0 || height <= 0)
    return;

  // Begin frame
  stygian_begin_frame(g_ctx, width, height);

  // PASS 1: Window body with gradient border
  StygianElement window_body = stygian_element_transient(g_ctx);
  stygian_set_bounds(g_ctx, window_body, 0, 0, (float)width, (float)height);
  stygian_set_type(g_ctx, window_body, STYGIAN_WINDOW_BODY);
  stygian_set_color(g_ctx, window_body, 0.5f, 0.5f, 0.5f, 1.0f);
  stygian_set_border(g_ctx, window_body, 0.235f, 0.259f, 0.294f, 1.0f);
  stygian_set_radius(g_ctx, window_body, CORNER_RADIUS, CORNER_RADIUS,
                     CORNER_RADIUS, CORNER_RADIUS);

  // PASS 2: Main panel (dark content area)
  int panel_margin = 10;
  int panel_top = TITLEBAR_HEIGHT + 10;
  int panel_bottom = TITLEBAR_HEIGHT + 10;
  stygian_rect_rounded(g_ctx, (float)panel_margin, (float)panel_top,
                       (float)(width - panel_margin * 2),
                       (float)(height - panel_top - panel_bottom), 0.10f, 0.10f,
                       0.11f, 1.0f, 8.0f);

  // PASS 3: Control buttons (top right)
  int btn_y = 10;
  int base_x = width - 18;
  int close_x = base_x - BTN_SIZE;
  int max_x = close_x - BTN_SPACING;
  int min_x = max_x - BTN_SPACING;

  // Close button (red on hover) - Background
  float cr = (g_hovered_button == 3) ? 0.95f : 0.35f;
  float cg = (g_hovered_button == 3) ? 0.3f : 0.38f;
  float cb = (g_hovered_button == 3) ? 0.3f : 0.42f;
  stygian_rect_rounded(g_ctx, (float)close_x, (float)btn_y, (float)BTN_SIZE,
                       (float)BTN_SIZE, cr, cg, cb, 1.0f, BTN_RADIUS);
  // Close icon: X (SDF type 7)
  {
    float ic = (g_hovered_button == 3) ? 1.0f : 0.9f;
    StygianElement icon = stygian_element_transient(g_ctx);
    stygian_set_bounds(g_ctx, icon, (float)close_x, (float)btn_y,
                       (float)BTN_SIZE, (float)BTN_SIZE);
    stygian_set_type(g_ctx, icon, STYGIAN_ICON_CLOSE);
    stygian_set_color(g_ctx, icon, ic, ic, ic, 1.0f);
  }

  // Maximize button (green on hover) - Background
  float mr = (g_hovered_button == 2) ? 0.3f : 0.45f;
  float mg = (g_hovered_button == 2) ? 0.85f : 0.48f;
  float mb = (g_hovered_button == 2) ? 0.4f : 0.52f;
  stygian_rect_rounded(g_ctx, (float)max_x, (float)btn_y, (float)BTN_SIZE,
                       (float)BTN_SIZE, mr, mg, mb, 1.0f, BTN_RADIUS);
  // Maximize icon: square outline (SDF type 8)
  {
    float ic = (g_hovered_button == 2) ? 1.0f : 0.9f;
    StygianElement icon = stygian_element_transient(g_ctx);
    stygian_set_bounds(g_ctx, icon, (float)max_x, (float)btn_y, (float)BTN_SIZE,
                       (float)BTN_SIZE);
    stygian_set_type(g_ctx, icon, STYGIAN_ICON_MAXIMIZE);
    stygian_set_color(g_ctx, icon, ic, ic, ic, 1.0f);
  }

  // Minimize button (yellow on hover) - Background
  float nr = (g_hovered_button == 1) ? 0.95f : 0.35f;
  float ng = (g_hovered_button == 1) ? 0.8f : 0.38f;
  float nb = (g_hovered_button == 1) ? 0.2f : 0.42f;
  stygian_rect_rounded(g_ctx, (float)min_x, (float)btn_y, (float)BTN_SIZE,
                       (float)BTN_SIZE, nr, ng, nb, 1.0f, BTN_RADIUS);
  // Minimize icon: horizontal line (SDF type 9)
  {
    float ic = (g_hovered_button == 1) ? 1.0f : 0.9f;
    StygianElement icon = stygian_element_transient(g_ctx);
    stygian_set_bounds(g_ctx, icon, (float)min_x, (float)btn_y, (float)BTN_SIZE,
                       (float)BTN_SIZE);
    stygian_set_type(g_ctx, icon, STYGIAN_ICON_MINIMIZE);
    stygian_set_color(g_ctx, icon, ic, ic, ic, 1.0f);
  }

  // PASS 4: Metaball menu bar - Proximity-based animation (like stygian_core.c)
  {
    // Animation constants from stygian_core.c
    float menu_stay_open_ms = 500.0f;
    float anim_speed = 0.15f;

    // Check proximity and timeout
    ULONGLONG current_time = GetTickCount64();
    bool near_menu = (g_mouse_y < 40 && g_mouse_x >= 80 && g_mouse_x < 430);
    bool within_timeout =
        (current_time - g_menu_interact_time) < (ULONGLONG)menu_stay_open_ms;

    // Update animation target
    float target = (near_menu || within_timeout) ? 1.0f : 0.0f;
    g_menu_anim += (target - g_menu_anim) * anim_speed;
    if (g_menu_anim < 0.01f)
      g_menu_anim = 0.0f;
    if (g_menu_anim > 0.99f)
      g_menu_anim = 1.0f;

    // Calculate animated height (2px retracted, 16px expanded)
    float menu_height_retracted = 2.0f;
    float menu_height_expanded = 16.0f;
    float menu_height =
        menu_height_retracted +
        (menu_height_expanded - menu_height_retracted) * g_menu_anim;

    // Menu bar element (Type 9 metaball with blend)
    StygianElement menu = stygian_element_transient(g_ctx);
    stygian_set_bounds(g_ctx, menu, 80, 0, 350, 40);
    stygian_set_type(g_ctx, menu, STYGIAN_METABALL_LEFT);
    stygian_set_color(g_ctx, menu, 0.235f, 0.259f, 0.294f, 1.0f);
    stygian_set_radius(g_ctx, menu, 0, 6, 0, 6); // Round bottom only
    stygian_set_blend(g_ctx, menu, 40.0f); // blend_k = 40 for smooth metaball
    // Store height in extra data if needed
    (void)menu_height; // Used indirectly by shader via blend
  }

  // PASS 5: Text rendering (fades with menu animation)
  if (g_font) {
    float text_alpha =
        0.3f + g_menu_anim * 0.7f; // 0.3 when retracted, 1.0 when expanded
    stygian_text(g_ctx, g_font, "FILE  EDIT  AGENT  VIEW  RUN", 95, 12, 14,
                 0.9f, 0.9f, 0.9f, text_alpha);

    stygian_text(g_ctx, g_font, "Stygian Demo - GPU SDF UI with Animations", 25,
                 (float)(TITLEBAR_HEIGHT + 25), 12, 0.7f, 0.7f, 0.7f, 1.0f);

    char time_str[64];
    sprintf(time_str, "Time: %.2fs | Menu anim: %.2f", g_time, g_menu_anim);
    stygian_text(g_ctx, g_font, time_str, 25, (float)(TITLEBAR_HEIGHT + 45), 12,
                 0.6f, 0.6f, 0.6f, 1.0f);
  }

  // End frame (single GPU draw call)
  stygian_end_frame(g_ctx);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  // Register window class
  WNDCLASSEX wc = {
      .cbSize = sizeof(wc),
      .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
      .lpfnWndProc = window_proc,
      .hInstance = hInstance,
      .hCursor = LoadCursor(NULL, IDC_ARROW),
      .lpszClassName = "StygianDemo",
  };
  RegisterClassEx(&wc);

  // Create window - no WS_THICKFRAME (WM_NCHITTEST handles resize)
  HWND hwnd = CreateWindowEx(
      WS_EX_APPWINDOW | WS_EX_LAYERED, "StygianDemo", "Stygian Demo",
      WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, CW_USEDEFAULT,
      CW_USEDEFAULT, 1024, 640, NULL, NULL, hInstance, NULL);

  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

  // Disable Windows DWM styling - we render our own corners and borders
  DWORD ncPolicy = DWMNCRP_DISABLED;
  DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncPolicy,
                        sizeof(ncPolicy));

  DWORD cornerPref = DWMWCP_DONOTROUND;
  DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref,
                        sizeof(cornerPref));

  // Use dark mode to avoid light titlebar flash
  BOOL darkMode = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode,
                        sizeof(darkMode));

  // Extend frame with zero margins (no blur effect)
  MARGINS margins = {0, 0, 0, 0};
  DwmExtendFrameIntoClientArea(hwnd, &margins);

  // Wrap the HWND for Stygian
  g_win = stygian_window_from_native(hwnd);
  if (!g_win) {
    MessageBoxA(NULL, "Failed to wrap window", "Error", MB_OK);
    return 1;
  }

  // Create Stygian context
  StygianConfig config = {
      .backend = STYGIAN_DEMO_BACKEND,
      .max_elements = 2048,
      .max_textures = 64,
      .window = g_win,
  };

  g_ctx = stygian_create(&config);
  if (!g_ctx) {
    MessageBoxA(NULL, "Failed to create Stygian context", "Error", MB_OK);
    stygian_window_destroy(g_win);
    return 1;
  }

  // Enable VSync
  stygian_set_vsync(g_ctx, true);

  // Load font - use path relative to exe directory
  char exe_dir[MAX_PATH];
  GetModuleFileName(NULL, exe_dir, MAX_PATH);
  char *last_slash = strrchr(exe_dir, '\\');
  if (last_slash)
    *(last_slash + 1) = 0;

  char png_path[MAX_PATH], json_path[MAX_PATH];
  snprintf(png_path, MAX_PATH, "%s..\\assets\\atlas.png", exe_dir);
  snprintf(json_path, MAX_PATH, "%s..\\assets\\atlas.json", exe_dir);

  g_font = stygian_font_load(g_ctx, png_path, json_path);
  if (!g_font) {
    printf("[Warning] Failed to load font atlas\n");
  }

  // High-precision timing for frame limiting
  LARGE_INTEGER freq, lastTime, now;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&lastTime);
  double ticksPerMs = (double)freq.QuadPart / 1000.0;

  // Main loop
  MSG msg;
  bool running = true;

  while (running) {
    bool had_msg = false;
    bool near_menu;
    bool within_timeout;
    bool animating;

    // Process all pending messages
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      had_msg = true;
      if (msg.message == WM_QUIT)
        running = false;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (!running)
      break;

    near_menu = (g_mouse_y < 40 && g_mouse_x >= 80 && g_mouse_x < 430);
    within_timeout = (GetTickCount64() - g_menu_interact_time) < 500ULL;
    animating = g_in_resize || near_menu || within_timeout || (g_menu_anim > 0.01f);

    // Idle path: block on OS events instead of redrawing continuously.
    if (!had_msg && !animating) {
      WaitMessage();
      continue;
    }

    // Calculate delta time
    QueryPerformanceCounter(&now);
    double elapsedMs = (double)(now.QuadPart - lastTime.QuadPart) / ticksPerMs;

    // Only render if enough time has passed (frame limiting)
    if (elapsedMs >= TARGET_FRAME_TIME) {
      lastTime = now;
      g_time += (float)(elapsedMs / 1000.0);

      // Check for shader file changes (hot-reload)
      if (g_ctx && stygian_ap_shaders_need_reload(stygian_get_ap(g_ctx))) {
        stygian_ap_reload_shaders(stygian_get_ap(g_ctx));
      }

      // Render frame (only from main loop, not during resize)
      if (!g_in_resize) {
        render_frame();
      }
    }

    // Yield CPU during animation paths.
    if (animating)
      Sleep(1);
  }

  if (g_font)
    stygian_font_destroy(g_ctx, g_font);
  stygian_destroy(g_ctx);
  g_ctx = NULL;
  return 0;
}
