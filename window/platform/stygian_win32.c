// stygian_win32.c - Win32 Platform Implementation
// Part of Stygian UI Library
// GRAPHICS-AGNOSTIC: Only handles window, input, events
// OpenGL/Vulkan context creation is handled by backends/
#ifdef _WIN32

#include "../stygian_window.h"
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
#ifdef STYGIAN_VULKAN
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#endif

// DWM constants
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// ============================================================================
// Window Structure (NO graphics context - that's backend's job)
// ============================================================================

struct StygianWindow {
  HWND hwnd;
  HDC hdc; // Device context for graphics backends to use

  int width, height;
  bool should_close;
  bool focused;
  bool maximized;
  bool minimized;
  bool fullscreen;
  bool borderless_manual_maximized;
  bool borderless_restore_valid;
  RECT borderless_restore_rect;
  bool fullscreen_restore_valid;
  RECT fullscreen_restore_rect;
  bool external_owned; // True if wrapped via stygian_window_from_native()

  // Event queue (ring buffer)
  StygianEvent events[256];
  int event_head;
  int event_tail;

  // Input state
  bool keys[STYGIAN_KEY_COUNT];
  bool mouse_buttons[5];
  int mouse_x, mouse_y;
  uint32_t mods;

  // Config
  uint32_t flags;
  bool gl_pixel_format_set;
  bool gl_vsync_requested;
  bool gl_swap_control_supported;
  bool gl_borderless_vsync_suspended;
  bool gl_swap_interval_resync_pending;
  uint32_t gl_borderless_present_stall_count;
  StygianTitlebarBehavior titlebar_behavior;
  bool in_size_move;
  UINT_PTR live_tick_timer_id;
  uint32_t live_tick_hz;
  bool nc_drag_active;
  int nc_drag_hit;
  POINT nc_drag_start_cursor;
  RECT nc_drag_start_rect;
};

static const UINT_PTR STYGIAN_WIN32_LIVE_TICK_TIMER_ID = 0x51A7u;
static const uint32_t STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ = 30u;
static const int STYGIAN_WIN32_MIN_TRACK_W = 320;
static const int STYGIAN_WIN32_MIN_TRACK_H = 200;
static void push_event(StygianWindow *win, StygianEvent *e);
static bool stygian_win32_get_monitor_work_rect(HWND hwnd, RECT *out_rect);
static bool stygian_win32_get_monitor_rect(HWND hwnd, RECT *out_rect);
static bool stygian_win32_get_fullscreen_state(HWND hwnd, RECT *out_rect);
static void stygian_win32_apply_window_rect(HWND hwnd, const RECT *rect);
static bool stygian_win32_present_trace_enabled(void);
static void stygian_win32_trace_transition(StygianWindow *win,
                                           const char *transition);
static void stygian_win32_trace_long_present(StygianWindow *win,
                                             double present_ms);

static void stygian_win32_start_live_ticks(StygianWindow *win, uint32_t hz) {
  UINT interval_ms;
  if (!win || !win->hwnd)
    return;
  if (hz == 0u)
    hz = STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ;
  interval_ms = (UINT)(1000u / hz);
  if (interval_ms < 1u)
    interval_ms = 1u;
  SetTimer(win->hwnd, STYGIAN_WIN32_LIVE_TICK_TIMER_ID, interval_ms, NULL);
  win->live_tick_timer_id = STYGIAN_WIN32_LIVE_TICK_TIMER_ID;
}

static void stygian_win32_stop_live_ticks(StygianWindow *win) {
  if (!win || !win->hwnd)
    return;
  if (win->live_tick_timer_id != 0u) {
    KillTimer(win->hwnd, win->live_tick_timer_id);
    win->live_tick_timer_id = 0u;
  }
}

static bool stygian_win32_is_resize_hit(int hit) {
  switch (hit) {
  case HTLEFT:
  case HTRIGHT:
  case HTTOP:
  case HTBOTTOM:
  case HTTOPLEFT:
  case HTTOPRIGHT:
  case HTBOTTOMLEFT:
  case HTBOTTOMRIGHT:
    return true;
  default:
    return false;
  }
}

static bool stygian_win32_is_move_or_resize_hit(int hit) {
  return (hit == HTCAPTION) || stygian_win32_is_resize_hit(hit);
}

static void stygian_win32_end_nc_drag(StygianWindow *win) {
  StygianEvent e = {0};
  if (!win || !win->nc_drag_active)
    return;
  win->nc_drag_active = false;
  win->nc_drag_hit = 0;
  win->in_size_move = false;
  stygian_win32_stop_live_ticks(win);
  ReleaseCapture();
  e.type = STYGIAN_EVENT_TICK;
  push_event(win, &e);
}

static void stygian_win32_apply_nc_drag(StygianWindow *win) {
  POINT cursor;
  RECT next_rect;
  LONG dx;
  LONG dy;
  LONG min_w = STYGIAN_WIN32_MIN_TRACK_W;
  LONG min_h = STYGIAN_WIN32_MIN_TRACK_H;
  if (!win || !win->nc_drag_active)
    return;
  if (!GetCursorPos(&cursor))
    return;

  dx = cursor.x - win->nc_drag_start_cursor.x;
  dy = cursor.y - win->nc_drag_start_cursor.y;
  next_rect = win->nc_drag_start_rect;

  if (win->nc_drag_hit == HTCAPTION) {
    OffsetRect(&next_rect, dx, dy);
  } else {
    switch (win->nc_drag_hit) {
    case HTLEFT:
      next_rect.left += dx;
      break;
    case HTRIGHT:
      next_rect.right += dx;
      break;
    case HTTOP:
      next_rect.top += dy;
      break;
    case HTBOTTOM:
      next_rect.bottom += dy;
      break;
    case HTTOPLEFT:
      next_rect.left += dx;
      next_rect.top += dy;
      break;
    case HTTOPRIGHT:
      next_rect.right += dx;
      next_rect.top += dy;
      break;
    case HTBOTTOMLEFT:
      next_rect.left += dx;
      next_rect.bottom += dy;
      break;
    case HTBOTTOMRIGHT:
      next_rect.right += dx;
      next_rect.bottom += dy;
      break;
    default:
      break;
    }

    if (next_rect.right - next_rect.left < min_w) {
      if (win->nc_drag_hit == HTLEFT || win->nc_drag_hit == HTTOPLEFT ||
          win->nc_drag_hit == HTBOTTOMLEFT) {
        next_rect.left = next_rect.right - min_w;
      } else {
        next_rect.right = next_rect.left + min_w;
      }
    }
    if (next_rect.bottom - next_rect.top < min_h) {
      if (win->nc_drag_hit == HTTOP || win->nc_drag_hit == HTTOPLEFT ||
          win->nc_drag_hit == HTTOPRIGHT) {
        next_rect.top = next_rect.bottom - min_h;
      } else {
        next_rect.bottom = next_rect.top + min_h;
      }
    }
  }

  SetWindowPos(win->hwnd, NULL, next_rect.left, next_rect.top,
               next_rect.right - next_rect.left,
               next_rect.bottom - next_rect.top,
               SWP_NOACTIVATE | SWP_NOZORDER);
}

static bool stygian_win32_use_dwm_flush(void) {
  static int initialized = 0;
  static bool enabled = false;
  if (!initialized) {
    const char *env = getenv("STYGIAN_GL_DWM_FLUSH");
    enabled = (env && env[0] && env[0] != '0');
    initialized = 1;
  }
  return enabled;
}

static bool stygian_win32_present_trace_enabled(void) {
  static int initialized = 0;
  static bool enabled = false;
  if (!initialized) {
    const char *env = getenv("STYGIAN_GL_PRESENT_TRACE");
    enabled = (env && env[0] && env[0] != '0');
    initialized = 1;
  }
  return enabled;
}

static void stygian_win32_trace_transition(StygianWindow *win,
                                           const char *transition) {
  if (!win || !win->hwnd || !transition || !stygian_win32_present_trace_enabled())
    return;
  printf("[Stygian Win32] %s manual=%d zoomed=%d\n", transition,
         win->borderless_manual_maximized ? 1 : 0, IsZoomed(win->hwnd) ? 1 : 0);
}

static void stygian_win32_trace_long_present(StygianWindow *win,
                                             double present_ms) {
  if (!win || !win->hwnd || !stygian_win32_present_trace_enabled() ||
      present_ms <= 50.0) {
    return;
  }
  printf("[Stygian Win32] present %.2fms manual=%d zoomed=%d\n", present_ms,
         win->borderless_manual_maximized ? 1 : 0, IsZoomed(win->hwnd) ? 1 : 0);
}

static bool stygian_win32_get_borderless_work_area(HWND hwnd,
                                                   POINT *out_max_pos,
                                                   POINT *out_max_size) {
  RECT work_rect;
  HMONITOR monitor;
  MONITORINFO monitor_info;
  if (!hwnd || !out_max_pos || !out_max_size)
    return false;
  if (!stygian_win32_get_monitor_work_rect(hwnd, &work_rect))
    return false;

  monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!monitor)
    return false;
  memset(&monitor_info, 0, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfo(monitor, &monitor_info))
    return false;

  out_max_pos->x = work_rect.left - monitor_info.rcMonitor.left;
  out_max_pos->y = work_rect.top - monitor_info.rcMonitor.top;
  out_max_size->x = work_rect.right - work_rect.left;
  out_max_size->y = work_rect.bottom - work_rect.top;
  return true;
}

static bool stygian_win32_get_monitor_work_rect(HWND hwnd, RECT *out_rect) {
  HMONITOR monitor;
  MONITORINFO monitor_info;
  if (!hwnd || !out_rect)
    return false;

  monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!monitor)
    return false;

  memset(&monitor_info, 0, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfo(monitor, &monitor_info))
    return false;

  *out_rect = monitor_info.rcWork;
  return true;
}

static bool stygian_win32_get_monitor_rect(HWND hwnd, RECT *out_rect) {
  HMONITOR monitor;
  MONITORINFO monitor_info;
  if (!hwnd || !out_rect)
    return false;

  monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!monitor)
    return false;

  memset(&monitor_info, 0, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfo(monitor, &monitor_info))
    return false;

  *out_rect = monitor_info.rcMonitor;
  return true;
}

static bool stygian_win32_get_fullscreen_state(HWND hwnd, RECT *out_rect) {
  if (!hwnd || !out_rect)
    return false;
  return stygian_win32_get_monitor_rect(hwnd, out_rect);
}

static void stygian_win32_apply_window_rect(HWND hwnd, const RECT *rect) {
  if (!hwnd || !rect)
    return;
  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  SetWindowPos(hwnd, NULL, rect->left, rect->top, rect->right - rect->left,
               rect->bottom - rect->top,
               SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

// ============================================================================
// Key Translation
// ============================================================================

static StygianKey translate_key(WPARAM vk) {
  if (vk >= 'A' && vk <= 'Z')
    return STYGIAN_KEY_A + (vk - 'A');
  if (vk >= '0' && vk <= '9')
    return STYGIAN_KEY_0 + (vk - '0');
  if (vk >= VK_F1 && vk <= VK_F12)
    return STYGIAN_KEY_F1 + (vk - VK_F1);

  switch (vk) {
  case VK_SHIFT:
    return STYGIAN_KEY_SHIFT;
  case VK_CONTROL:
    return STYGIAN_KEY_CTRL;
  case VK_MENU:
    return STYGIAN_KEY_ALT;
  case VK_LWIN:
  case VK_RWIN:
    return STYGIAN_KEY_SUPER;
  case VK_UP:
    return STYGIAN_KEY_UP;
  case VK_DOWN:
    return STYGIAN_KEY_DOWN;
  case VK_LEFT:
    return STYGIAN_KEY_LEFT;
  case VK_RIGHT:
    return STYGIAN_KEY_RIGHT;
  case VK_HOME:
    return STYGIAN_KEY_HOME;
  case VK_END:
    return STYGIAN_KEY_END;
  case VK_PRIOR:
    return STYGIAN_KEY_PAGE_UP;
  case VK_NEXT:
    return STYGIAN_KEY_PAGE_DOWN;
  case VK_INSERT:
    return STYGIAN_KEY_INSERT;
  case VK_DELETE:
    return STYGIAN_KEY_DELETE;
  case VK_ESCAPE:
    return STYGIAN_KEY_ESCAPE;
  case VK_RETURN:
    return STYGIAN_KEY_ENTER;
  case VK_TAB:
    return STYGIAN_KEY_TAB;
  case VK_BACK:
    return STYGIAN_KEY_BACKSPACE;
  case VK_SPACE:
    return STYGIAN_KEY_SPACE;
  default:
    return STYGIAN_KEY_UNKNOWN;
  }
}

static uint32_t get_mods(void) {
  uint32_t mods = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000)
    mods |= STYGIAN_MOD_SHIFT;
  if (GetKeyState(VK_CONTROL) & 0x8000)
    mods |= STYGIAN_MOD_CTRL;
  if (GetKeyState(VK_MENU) & 0x8000)
    mods |= STYGIAN_MOD_ALT;
  if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
    mods |= STYGIAN_MOD_SUPER;
  return mods;
}

// ============================================================================
// Event Queue
// ============================================================================

static void push_event(StygianWindow *win, StygianEvent *e) {
  int prev;
  int next = (win->event_head + 1) % 256;

  // Coalesce high-rate move/resize events so the app sees the latest state
  // without processing every intermediate OS message.
  if (win->event_head != win->event_tail) {
    prev = (win->event_head - 1 + 256) % 256;
    if ((e->type == STYGIAN_EVENT_MOUSE_MOVE &&
         win->events[prev].type == STYGIAN_EVENT_MOUSE_MOVE) ||
        (e->type == STYGIAN_EVENT_RESIZE &&
         win->events[prev].type == STYGIAN_EVENT_RESIZE) ||
        (e->type == STYGIAN_EVENT_SCROLL &&
         win->events[prev].type == STYGIAN_EVENT_SCROLL) ||
        (e->type == STYGIAN_EVENT_TICK &&
         win->events[prev].type == STYGIAN_EVENT_TICK)) {
      if (e->type == STYGIAN_EVENT_SCROLL &&
          win->events[prev].type == STYGIAN_EVENT_SCROLL) {
        // Merge wheel bursts into one event to avoid queue tail-lag.
        win->events[prev].scroll.dx += e->scroll.dx;
        win->events[prev].scroll.dy += e->scroll.dy;
        win->events[prev].scroll.x = e->scroll.x;
        win->events[prev].scroll.y = e->scroll.y;
      } else {
        win->events[prev] = *e;
      }
      return;
    }
  }

  if (next != win->event_tail) {
    win->events[win->event_head] = *e;
    win->event_head = next;
  }
}

// ============================================================================
// Window Procedure
// ============================================================================

static LRESULT CALLBACK win32_wndproc(HWND hwnd, UINT msg, WPARAM wp,
                                      LPARAM lp) {
  StygianWindow *win = (StygianWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (!win) {
    if (msg == WM_NCCREATE) {
      const CREATESTRUCT *create_struct = (const CREATESTRUCT *)lp;
      StygianWindow *create_win =
          create_struct ? (StygianWindow *)create_struct->lpCreateParams : NULL;
      if (create_win) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)create_win);
        create_win->hwnd = hwnd;
        return TRUE;
      }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
  }

  StygianEvent e = {0};

  switch (msg) {
  case WM_CLOSE:
    e.type = STYGIAN_EVENT_CLOSE;
    push_event(win, &e);
    win->should_close = true;
    return 0;

  case WM_SIZE:
    win->width = LOWORD(lp);
    win->height = HIWORD(lp);
    win->maximized =
        win->borderless_manual_maximized || (wp == SIZE_MAXIMIZED);
    win->minimized = (wp == SIZE_MINIMIZED);
    e.type = STYGIAN_EVENT_RESIZE;
    e.resize.width = win->width;
    e.resize.height = win->height;
    push_event(win, &e);
    return 0;

  case WM_ENTERSIZEMOVE:
    return 0;

  case WM_EXITSIZEMOVE:
    return 0;

  case WM_MOVING:
  case WM_SIZING:
    return 0;

  case WM_SETFOCUS:
    win->focused = true;
    e.type = STYGIAN_EVENT_FOCUS;
    push_event(win, &e);
    return 0;

  case WM_KILLFOCUS:
    win->focused = false;
    e.type = STYGIAN_EVENT_BLUR;
    push_event(win, &e);
    return 0;

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    e.type = STYGIAN_EVENT_KEY_DOWN;
    e.key.key = translate_key(wp);
    e.key.mods = get_mods();
    e.key.repeat = (lp & 0x40000000) != 0;
    win->keys[e.key.key] = true;
    win->mods = e.key.mods;
    push_event(win, &e);
    return 0;

  case WM_KEYUP:
  case WM_SYSKEYUP:
    e.type = STYGIAN_EVENT_KEY_UP;
    e.key.key = translate_key(wp);
    e.key.mods = get_mods();
    e.key.repeat = false;
    win->keys[e.key.key] = false;
    win->mods = e.key.mods;
    push_event(win, &e);
    return 0;

  case WM_CHAR:
    if (wp >= 32) {
      e.type = STYGIAN_EVENT_CHAR;
      e.chr.codepoint = (uint32_t)wp;
      push_event(win, &e);
    }
    return 0;

  case WM_MOUSEMOVE:
    e.type = STYGIAN_EVENT_MOUSE_MOVE;
    e.mouse_move.x = GET_X_LPARAM(lp);
    e.mouse_move.y = GET_Y_LPARAM(lp);
    e.mouse_move.dx = e.mouse_move.x - win->mouse_x;
    e.mouse_move.dy = e.mouse_move.y - win->mouse_y;
    win->mouse_x = e.mouse_move.x;
    win->mouse_y = e.mouse_move.y;
    push_event(win, &e);
    return 0;

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
    e.type = STYGIAN_EVENT_MOUSE_DOWN;
    e.mouse_button.x = GET_X_LPARAM(lp);
    e.mouse_button.y = GET_Y_LPARAM(lp);
    e.mouse_button.button = (msg == WM_LBUTTONDOWN)   ? STYGIAN_MOUSE_LEFT
                            : (msg == WM_RBUTTONDOWN) ? STYGIAN_MOUSE_RIGHT
                                                      : STYGIAN_MOUSE_MIDDLE;
    e.mouse_button.mods = get_mods();
    e.mouse_button.clicks = 1;
    win->mouse_buttons[e.mouse_button.button] = true;
    push_event(win, &e);
    SetCapture(hwnd);
    return 0;

  case WM_LBUTTONDBLCLK:
    e.type = STYGIAN_EVENT_MOUSE_DOWN;
    e.mouse_button.x = GET_X_LPARAM(lp);
    e.mouse_button.y = GET_Y_LPARAM(lp);
    e.mouse_button.button = STYGIAN_MOUSE_LEFT;
    e.mouse_button.mods = get_mods();
    e.mouse_button.clicks = 2;
    win->mouse_buttons[e.mouse_button.button] = true;
    push_event(win, &e);
    SetCapture(hwnd);
    return 0;

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
    e.type = STYGIAN_EVENT_MOUSE_UP;
    e.mouse_button.x = GET_X_LPARAM(lp);
    e.mouse_button.y = GET_Y_LPARAM(lp);
    e.mouse_button.button = (msg == WM_LBUTTONUP)   ? STYGIAN_MOUSE_LEFT
                            : (msg == WM_RBUTTONUP) ? STYGIAN_MOUSE_RIGHT
                                                    : STYGIAN_MOUSE_MIDDLE;
    e.mouse_button.mods = get_mods();
    win->mouse_buttons[e.mouse_button.button] = false;
    push_event(win, &e);
    if (win->nc_drag_active) {
      stygian_win32_end_nc_drag(win);
    } else {
      ReleaseCapture();
    }
    return 0;

  case WM_NCLBUTTONDOWN: {
    int hit = (int)wp;
    if (stygian_win32_is_move_or_resize_hit(hit)) {
      win->nc_drag_active = true;
      win->nc_drag_hit = hit;
      win->in_size_move = true;
      stygian_win32_start_live_ticks(win, win->live_tick_hz);
      GetCursorPos(&win->nc_drag_start_cursor);
      GetWindowRect(hwnd, &win->nc_drag_start_rect);
      SetCapture(hwnd);
      e.type = STYGIAN_EVENT_TICK;
      push_event(win, &e);
      return 0;
    }
  } break;

  case WM_NCLBUTTONUP:
    stygian_win32_end_nc_drag(win);
    return 0;

  case WM_CAPTURECHANGED:
    stygian_win32_end_nc_drag(win);
    return 0;

  case WM_MOUSEWHEEL:
    e.type = STYGIAN_EVENT_SCROLL;
    e.scroll.x = GET_X_LPARAM(lp);
    e.scroll.y = GET_Y_LPARAM(lp);
    e.scroll.dx = 0;
    e.scroll.dy = (float)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
    push_event(win, &e);
    return 0;

  case WM_TIMER:
    if (wp == STYGIAN_WIN32_LIVE_TICK_TIMER_ID &&
        (win->in_size_move || win->nc_drag_active)) {
      if (win->nc_drag_active) {
        stygian_win32_apply_nc_drag(win);
      }
      e.type = STYGIAN_EVENT_TICK;
      push_event(win, &e);
      return 0;
    }
    break;

  case WM_GETMINMAXINFO:
    if ((win->flags & STYGIAN_WINDOW_BORDERLESS) && lp) {
      MINMAXINFO *minmax_info = (MINMAXINFO *)lp;
      POINT max_pos = {0};
      POINT max_size = {0};
      if (stygian_win32_get_borderless_work_area(hwnd, &max_pos, &max_size)) {
        minmax_info->ptMaxPosition = max_pos;
        minmax_info->ptMaxSize = max_size;
        return 0;
      }
    }
    break;
  }

  return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
// Window Creation (NO graphics context - that's backend's job)
// ============================================================================

static const char *WIN_CLASS = "StygianWindowClass";
static bool class_registered = false;

StygianWindow *stygian_window_create(const StygianWindowConfig *config) {
  // Register window class
  if (!class_registered) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = win32_wndproc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WIN_CLASS;
    RegisterClassEx(&wc);
    class_registered = true;
  }

  // Allocate window
  StygianWindow *win = (StygianWindow *)calloc(1, sizeof(StygianWindow));
  if (!win)
    return NULL;

  win->width = config->width;
  win->height = config->height;
  win->flags = config->flags;
  win->focused = true;
  win->live_tick_hz = STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ;
  win->titlebar_behavior.double_click_mode =
      STYGIAN_TITLEBAR_DBLCLICK_MAXIMIZE_RESTORE;
  win->titlebar_behavior.hover_menu_enabled = true;

  // Window style
  DWORD style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = WS_EX_APPWINDOW;

  // Apply Role Styles
  switch (config->role) {
  case STYGIAN_ROLE_MAIN:
    ex_style = WS_EX_APPWINDOW;
    break;
  case STYGIAN_ROLE_TOOL:
    ex_style = WS_EX_TOOLWINDOW;
    style = WS_OVERLAPPEDWINDOW; // Or WS_POPUP | WS_CAPTION?
    break;
  case STYGIAN_ROLE_POPUP:
    ex_style = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    style = WS_POPUP | WS_BORDER;
    break;
  case STYGIAN_ROLE_TOOLTIP:
    ex_style =
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    style = WS_POPUP;
    break;
  }

  if (config->flags & STYGIAN_WINDOW_BORDERLESS) {
    style = WS_POPUP;
  }
  if (!(config->flags & STYGIAN_WINDOW_RESIZABLE)) {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  if (config->flags & STYGIAN_WINDOW_ALWAYS_ON_TOP) {
    ex_style |= WS_EX_TOPMOST;
  }

  // Adjust window size for borders
  RECT rc = {0, 0, config->width, config->height};
  AdjustWindowRectEx(&rc, style, FALSE, ex_style);
  int adj_w = rc.right - rc.left;
  int adj_h = rc.bottom - rc.top;

  // Position
  int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
  if (config->flags & STYGIAN_WINDOW_CENTERED) {
    x = (GetSystemMetrics(SM_CXSCREEN) - adj_w) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - adj_h) / 2;
  }

  // Create window
  win->hwnd =
      CreateWindowEx(ex_style, WIN_CLASS, config->title, style, x, y, adj_w,
                     adj_h, NULL, NULL, GetModuleHandle(NULL), win);
  if (!win->hwnd) {
    free(win);
    return NULL;
  }

  SetWindowLongPtr(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);

  // Dark mode
  BOOL dark = TRUE;
  DwmSetWindowAttribute(win->hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                        sizeof(dark));

  // Get device context for graphics backends
  win->hdc = GetDC(win->hwnd);

  // If OpenGL window, set pixel format early
  if (win->flags & STYGIAN_WINDOW_OPENGL) {
    if (!stygian_window_gl_set_pixel_format(win)) {
      printf("[Stygian Window] Failed to set OpenGL pixel format\n");
      DestroyWindow(win->hwnd);
      free(win);
      return NULL;
    }
  }

  // Show window
  ShowWindow(win->hwnd, (config->flags & STYGIAN_WINDOW_MAXIMIZED)
                            ? SW_SHOWMAXIMIZED
                            : SW_SHOW);
  UpdateWindow(win->hwnd);

  return win;
}

StygianWindow *stygian_window_create_simple(int w, int h, const char *title) {
  StygianWindowConfig cfg = {.width = w,
                             .height = h,
                             .title = title,
                             .flags = STYGIAN_WINDOW_RESIZABLE |
                                      STYGIAN_WINDOW_CENTERED,
                             .gl_major = 4,
                             .gl_minor = 3};
  return stygian_window_create(&cfg);
}

StygianWindow *stygian_window_from_native(void *native_handle) {
  if (!native_handle)
    return NULL;

  HWND hwnd = (HWND)native_handle;

  StygianWindow *win = (StygianWindow *)calloc(1, sizeof(StygianWindow));
  if (!win)
    return NULL;

  win->hwnd = hwnd;
  win->hdc = GetDC(hwnd);
  win->external_owned = true; // Don't destroy window on cleanup
  win->focused = true;

  // Get current size
  RECT rc;
  if (GetClientRect(hwnd, &rc)) {
    win->width = rc.right;
    win->height = rc.bottom;
  }

  return win;
}

void stygian_window_destroy(StygianWindow *win) {
  if (!win)
    return;

  stygian_win32_stop_live_ticks(win);

  // NOTE: Graphics context cleanup is backend's responsibility
  // We only handle window resources here

  if (win->hdc) {
    ReleaseDC(win->hwnd, win->hdc);
  }

  // Only destroy window if we own it
  if (!win->external_owned && win->hwnd) {
    DestroyWindow(win->hwnd);
  }

  free(win);
}

// ============================================================================
// Window State
// ============================================================================

bool stygian_window_should_close(StygianWindow *win) {
  return win ? win->should_close : true;
}

void stygian_window_request_close(StygianWindow *win) {
  if (win)
    win->should_close = true;
}

void stygian_window_get_size(StygianWindow *win, int *w, int *h) {
  if (win) {
    if (w)
      *w = win->width;
    if (h)
      *h = win->height;
  }
}

void stygian_window_set_size(StygianWindow *win, int w, int h) {
  if (win && win->hwnd) {
    if (win->fullscreen) {
      win->fullscreen = false;
      win->fullscreen_restore_valid = false;
    }
    if (win->borderless_manual_maximized) {
      win->borderless_manual_maximized = false;
      win->maximized = false;
      win->gl_borderless_vsync_suspended = false;
      win->gl_borderless_present_stall_count = 0u;
      if ((win->flags & STYGIAN_WINDOW_OPENGL) && win->gl_vsync_requested &&
          win->gl_swap_control_supported) {
        win->gl_swap_interval_resync_pending = true;
      }
    }
    SetWindowPos(win->hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
  }
}

void stygian_window_get_position(StygianWindow *win, int *x, int *y) {
  if (win && win->hwnd) {
    RECT rc;
    GetWindowRect(win->hwnd, &rc);
    if (x)
      *x = rc.left;
    if (y)
      *y = rc.top;
  }
}

void stygian_window_set_position(StygianWindow *win, int x, int y) {
  if (win && win->hwnd) {
    if (win->fullscreen) {
      win->fullscreen = false;
      win->fullscreen_restore_valid = false;
    }
    if (win->borderless_manual_maximized) {
      win->borderless_manual_maximized = false;
      win->maximized = false;
      win->gl_borderless_vsync_suspended = false;
      win->gl_borderless_present_stall_count = 0u;
      if ((win->flags & STYGIAN_WINDOW_OPENGL) && win->gl_vsync_requested &&
          win->gl_swap_control_supported) {
        win->gl_swap_interval_resync_pending = true;
      }
    }
    SetWindowPos(win->hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }
}

void stygian_window_set_title(StygianWindow *win, const char *title) {
  if (win && win->hwnd) {
    SetWindowText(win->hwnd, title);
  }
}

void stygian_window_minimize(StygianWindow *win) {
  if (win && win->hwnd) {
    if (win->fullscreen) {
      stygian_window_set_fullscreen(win, false);
    }
    ShowWindow(win->hwnd, SW_MINIMIZE);
  }
}

void stygian_window_maximize(StygianWindow *win) {
  if (!win || !win->hwnd)
    return;
  if (win->fullscreen) {
    stygian_window_set_fullscreen(win, false);
  }
  stygian_win32_trace_transition(win, "maximize-request");
  if (win->flags & STYGIAN_WINDOW_BORDERLESS) {
    RECT work_rect;
    RECT monitor_rect;
    if (!win->borderless_manual_maximized &&
        GetWindowRect(win->hwnd, &win->borderless_restore_rect)) {
      win->borderless_restore_valid = true;
    }
    if (stygian_win32_get_monitor_work_rect(win->hwnd, &work_rect)) {
      if ((win->flags & STYGIAN_WINDOW_OPENGL) &&
          stygian_win32_get_monitor_rect(win->hwnd, &monitor_rect)) {
        bool full_monitor =
            (work_rect.left == monitor_rect.left) &&
            (work_rect.top == monitor_rect.top) &&
            (work_rect.right == monitor_rect.right) &&
            (work_rect.bottom == monitor_rect.bottom);
        if (full_monitor && (work_rect.bottom - work_rect.top) > 1) {
          work_rect.bottom -= 1;
        }
      }
      stygian_win32_apply_window_rect(win->hwnd, &work_rect);
      win->borderless_manual_maximized = true;
      win->maximized = true;
      win->minimized = false;
      win->gl_borderless_present_stall_count = 0u;
      if ((win->flags & STYGIAN_WINDOW_OPENGL) && win->gl_vsync_requested &&
          win->gl_swap_control_supported) {
        win->gl_borderless_vsync_suspended = false;
        win->gl_swap_interval_resync_pending = true;
      }
      return;
    }
  }
  ShowWindow(win->hwnd, SW_MAXIMIZE);
}

void stygian_window_restore(StygianWindow *win) {
  if (!win || !win->hwnd)
    return;
  if (win->fullscreen) {
    stygian_window_set_fullscreen(win, false);
    return;
  }
  stygian_win32_trace_transition(win, "restore-request");
  if (win->borderless_manual_maximized) {
    if (win->borderless_restore_valid) {
      RECT restore_rect = win->borderless_restore_rect;
      stygian_win32_apply_window_rect(win->hwnd, &restore_rect);
    } else {
      ShowWindow(win->hwnd, SW_RESTORE);
    }
    win->borderless_manual_maximized = false;
    win->maximized = false;
    win->minimized = false;
    win->gl_borderless_vsync_suspended = false;
    win->gl_borderless_present_stall_count = 0u;
    if ((win->flags & STYGIAN_WINDOW_OPENGL) && win->gl_vsync_requested &&
        win->gl_swap_control_supported) {
      win->gl_swap_interval_resync_pending = true;
    }
    return;
  }
  ShowWindow(win->hwnd, SW_RESTORE);
}

bool stygian_window_is_maximized(StygianWindow *win) {
  return win ? win->maximized : false;
}

bool stygian_window_is_minimized(StygianWindow *win) {
  return win ? win->minimized : false;
}

void stygian_window_set_fullscreen(StygianWindow *win, bool enabled) {
  RECT target_rect;
  if (!win || !win->hwnd)
    return;
  if (enabled == win->fullscreen)
    return;

  if (enabled) {
    if (!win->fullscreen_restore_valid &&
        GetWindowRect(win->hwnd, &win->fullscreen_restore_rect)) {
      win->fullscreen_restore_valid = true;
    }
    if (!stygian_win32_get_fullscreen_state(win->hwnd, &target_rect))
      return;
    stygian_win32_apply_window_rect(win->hwnd, &target_rect);
    win->fullscreen = true;
    win->maximized = false;
    win->minimized = false;
    win->borderless_manual_maximized = false;
    win->gl_borderless_vsync_suspended = false;
    win->gl_borderless_present_stall_count = 0u;
    if ((win->flags & STYGIAN_WINDOW_OPENGL) && win->gl_vsync_requested &&
        win->gl_swap_control_supported) {
      win->gl_swap_interval_resync_pending = true;
    }
    return;
  }

  if (win->fullscreen_restore_valid) {
    target_rect = win->fullscreen_restore_rect;
    stygian_win32_apply_window_rect(win->hwnd, &target_rect);
  }
  win->fullscreen = false;
  win->maximized = false;
  win->minimized = false;
  win->gl_borderless_vsync_suspended = false;
  win->gl_borderless_present_stall_count = 0u;
  if ((win->flags & STYGIAN_WINDOW_OPENGL) && win->gl_vsync_requested &&
      win->gl_swap_control_supported) {
    win->gl_swap_interval_resync_pending = true;
  }
}

bool stygian_window_is_fullscreen(StygianWindow *win) {
  return win ? win->fullscreen : false;
}

void stygian_window_get_titlebar_hints(StygianWindow *win,
                                       StygianTitlebarHints *out_hints) {
  if (!out_hints)
    return;
  out_hints->button_order = STYGIAN_TITLEBAR_BUTTONS_RIGHT;
  out_hints->supports_hover_menu = true;
  out_hints->supports_snap_actions = true;
  out_hints->recommended_titlebar_height = 36.0f;
  out_hints->recommended_button_width = 28.0f;
  out_hints->recommended_button_height = 24.0f;
  out_hints->recommended_button_gap = 6.0f;
  if (win && !win->titlebar_behavior.hover_menu_enabled) {
    out_hints->supports_hover_menu = false;
  }
}

void stygian_window_set_titlebar_behavior(
    StygianWindow *win, const StygianTitlebarBehavior *behavior) {
  if (!win || !behavior)
    return;
  win->titlebar_behavior = *behavior;
  if (win->titlebar_behavior.double_click_mode !=
          STYGIAN_TITLEBAR_DBLCLICK_MAXIMIZE_RESTORE &&
      win->titlebar_behavior.double_click_mode !=
          STYGIAN_TITLEBAR_DBLCLICK_FULLSCREEN_TOGGLE) {
    win->titlebar_behavior.double_click_mode =
        STYGIAN_TITLEBAR_DBLCLICK_MAXIMIZE_RESTORE;
  }
}

void stygian_window_get_titlebar_behavior(StygianWindow *win,
                                          StygianTitlebarBehavior *out_behavior) {
  if (!out_behavior)
    return;
  out_behavior->double_click_mode = STYGIAN_TITLEBAR_DBLCLICK_MAXIMIZE_RESTORE;
  out_behavior->hover_menu_enabled = true;
  if (!win)
    return;
  *out_behavior = win->titlebar_behavior;
}

bool stygian_window_begin_system_move(StygianWindow *win) {
  if (!win || !win->hwnd || win->fullscreen)
    return false;
  ReleaseCapture();
  return PostMessage(win->hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0) != 0;
}

void stygian_window_titlebar_double_click(StygianWindow *win) {
  if (!win)
    return;
  if (win->titlebar_behavior.double_click_mode ==
      STYGIAN_TITLEBAR_DBLCLICK_FULLSCREEN_TOGGLE) {
    stygian_window_set_fullscreen(win, !stygian_window_is_fullscreen(win));
    return;
  }
  if (stygian_window_is_fullscreen(win)) {
    stygian_window_set_fullscreen(win, false);
    return;
  }
  if (stygian_window_is_maximized(win)) {
    stygian_window_restore(win);
  } else {
    stygian_window_maximize(win);
  }
}

uint32_t stygian_window_get_titlebar_menu_actions(
    StygianWindow *win, StygianTitlebarMenuAction *out_actions,
    uint32_t max_actions) {
  uint32_t count = 0;
#define STYGIAN_PUSH_TITLEBAR_ACTION(action_value)                              \
  do {                                                                          \
    if (out_actions && count < max_actions)                                     \
      out_actions[count] = (action_value);                                      \
    count++;                                                                    \
  } while (0)
  if (!win)
    return 0;

  if (stygian_window_is_maximized(win)) {
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_RESTORE);
  } else {
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_MAXIMIZE);
  }
  if (stygian_window_is_fullscreen(win)) {
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_EXIT_FULLSCREEN);
  } else {
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_ENTER_FULLSCREEN);
  }
  if (win->flags & STYGIAN_WINDOW_RESIZABLE) {
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_SNAP_LEFT);
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_SNAP_RIGHT);
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_SNAP_TOP_LEFT);
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_SNAP_TOP_RIGHT);
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_LEFT);
    STYGIAN_PUSH_TITLEBAR_ACTION(STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_RIGHT);
  }
#undef STYGIAN_PUSH_TITLEBAR_ACTION
  return count;
}

bool stygian_window_apply_titlebar_menu_action(StygianWindow *win,
                                               StygianTitlebarMenuAction action) {
  RECT work_rect;
  RECT target_rect;
  LONG half_w;
  LONG half_h;
  if (!win || !win->hwnd)
    return false;

  switch (action) {
  case STYGIAN_TITLEBAR_ACTION_RESTORE:
    if (stygian_window_is_fullscreen(win)) {
      stygian_window_set_fullscreen(win, false);
    } else {
      stygian_window_restore(win);
    }
    return true;
  case STYGIAN_TITLEBAR_ACTION_MAXIMIZE:
    if (stygian_window_is_fullscreen(win)) {
      stygian_window_set_fullscreen(win, false);
    }
    stygian_window_maximize(win);
    return true;
  case STYGIAN_TITLEBAR_ACTION_ENTER_FULLSCREEN:
    stygian_window_set_fullscreen(win, true);
    return true;
  case STYGIAN_TITLEBAR_ACTION_EXIT_FULLSCREEN:
    stygian_window_set_fullscreen(win, false);
    return true;
  case STYGIAN_TITLEBAR_ACTION_SNAP_LEFT:
  case STYGIAN_TITLEBAR_ACTION_SNAP_RIGHT:
  case STYGIAN_TITLEBAR_ACTION_SNAP_TOP_LEFT:
  case STYGIAN_TITLEBAR_ACTION_SNAP_TOP_RIGHT:
  case STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_LEFT:
  case STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_RIGHT:
    break;
  default:
    return false;
  }

  if (!(win->flags & STYGIAN_WINDOW_RESIZABLE))
    return false;
  if (!stygian_win32_get_monitor_work_rect(win->hwnd, &work_rect))
    return false;

  if (stygian_window_is_fullscreen(win)) {
    stygian_window_set_fullscreen(win, false);
  }

  half_w = (work_rect.right - work_rect.left) / 2;
  half_h = (work_rect.bottom - work_rect.top) / 2;
  target_rect = work_rect;

  switch (action) {
  case STYGIAN_TITLEBAR_ACTION_SNAP_LEFT:
    target_rect.right = target_rect.left + half_w;
    break;
  case STYGIAN_TITLEBAR_ACTION_SNAP_RIGHT:
    target_rect.left = target_rect.right - half_w;
    break;
  case STYGIAN_TITLEBAR_ACTION_SNAP_TOP_LEFT:
    target_rect.right = target_rect.left + half_w;
    target_rect.bottom = target_rect.top + half_h;
    break;
  case STYGIAN_TITLEBAR_ACTION_SNAP_TOP_RIGHT:
    target_rect.left = target_rect.right - half_w;
    target_rect.bottom = target_rect.top + half_h;
    break;
  case STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_LEFT:
    target_rect.right = target_rect.left + half_w;
    target_rect.top = target_rect.bottom - half_h;
    break;
  case STYGIAN_TITLEBAR_ACTION_SNAP_BOTTOM_RIGHT:
    target_rect.left = target_rect.right - half_w;
    target_rect.top = target_rect.bottom - half_h;
    break;
  default:
    return false;
  }

  stygian_win32_apply_window_rect(win->hwnd, &target_rect);
  win->fullscreen = false;
  win->borderless_manual_maximized = false;
  win->maximized = false;
  win->minimized = false;
  win->gl_borderless_vsync_suspended = false;
  win->gl_borderless_present_stall_count = 0u;
  if ((win->flags & STYGIAN_WINDOW_OPENGL) && win->gl_vsync_requested &&
      win->gl_swap_control_supported) {
    win->gl_swap_interval_resync_pending = true;
  }
  return true;
}

void stygian_window_focus(StygianWindow *win) {
  if (win && win->hwnd)
    SetForegroundWindow(win->hwnd);
}

bool stygian_window_is_focused(StygianWindow *win) {
  return win ? win->focused : false;
}

// ============================================================================
// Event Processing
// ============================================================================

bool stygian_window_poll_event(StygianWindow *win, StygianEvent *event) {
  if (!win)
    return false;

  // Process Windows messages first
  MSG msg;
  while (PeekMessage(&msg, win->hwnd, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Return queued event
  if (win->event_head != win->event_tail) {
    *event = win->events[win->event_tail];
    win->event_tail = (win->event_tail + 1) % 256;
    return true;
  }

  event->type = STYGIAN_EVENT_NONE;
  return false;
}

void stygian_window_wait_event(StygianWindow *win, StygianEvent *event) {
  if (!win || !win->hwnd)
    return;

  // If internal queue is empty, wait for OS message
  if (win->event_head == win->event_tail) {
    WaitMessage();
  }

  // Then process normally (poll will pick it up)
  stygian_window_poll_event(win, event);
}

bool stygian_window_wait_event_timeout(StygianWindow *win, StygianEvent *event,
                                       uint32_t timeout_ms) {
  DWORD wait_res;
  if (!win || !win->hwnd || !event)
    return false;

  if (win->in_size_move) {
    uint32_t hz = win->live_tick_hz ? win->live_tick_hz
                                    : STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ;
    uint32_t tick_ms = 1000u / hz;
    if (tick_ms < 1u)
      tick_ms = 1u;
    if (timeout_ms == 0u || timeout_ms > tick_ms)
      timeout_ms = tick_ms;
  }

  // Fast path: already queued.
  if (win->event_head != win->event_tail) {
    return stygian_window_poll_event(win, event);
  }

  wait_res = MsgWaitForMultipleObjectsEx(
      0, NULL, timeout_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
  if (wait_res == WAIT_TIMEOUT) {
    event->type = STYGIAN_EVENT_NONE;
    return false;
  }

  return stygian_window_poll_event(win, event);
}

void stygian_window_process_events(StygianWindow *win) {
  if (!win)
    return;

  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      win->should_close = true;
      return;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

// ============================================================================
// OpenGL Context (Win32 implementation)
// ============================================================================

typedef BOOL(WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int interval);
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;

bool stygian_window_gl_set_pixel_format(StygianWindow *win) {
  if (!win || !win->hdc)
    return false;
  if (win->gl_pixel_format_set)
    return true;

  PIXELFORMATDESCRIPTOR pfd = {
      .nSize = sizeof(pfd),
      .nVersion = 1,
      .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      .iPixelType = PFD_TYPE_RGBA,
      .cColorBits = 32,
      .cDepthBits = 24,
      .iLayerType = PFD_MAIN_PLANE,
  };

  int format = ChoosePixelFormat(win->hdc, &pfd);
  if (!format)
    return false;
  if (SetPixelFormat(win->hdc, format, &pfd) == TRUE) {
    win->gl_pixel_format_set = true;
    return true;
  }
  return false;
}

void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx) {
  if (!win || !win->hdc)
    return NULL;

  HGLRC ctx = wglCreateContext(win->hdc);
  if (!ctx)
    return NULL;

  if (share_ctx) {
    wglShareLists((HGLRC)share_ctx, ctx);
  }

  return (void *)ctx;
}

void stygian_window_gl_destroy_context(void *ctx) {
  if (ctx) {
    wglDeleteContext((HGLRC)ctx);
  }
}

bool stygian_window_gl_make_current(StygianWindow *win, void *ctx) {
  if (!win || !win->hdc || !ctx)
    return false;
  return wglMakeCurrent(win->hdc, (HGLRC)ctx) == TRUE;
}

void stygian_window_gl_swap_buffers(StygianWindow *win) {
  ULONGLONG present_start_ms = 0u;
  ULONGLONG present_end_ms;
  double present_ms = 0.0;
  bool force_dwm_flush;
  bool borderless_gl_manual;
  bool watch_present_stall;
  bool trace_present;
  if (!win || !win->hdc)
    return;
  if (win->gl_swap_interval_resync_pending && wglSwapIntervalEXT &&
      win->gl_swap_control_supported) {
    int interval = win->gl_vsync_requested ? 1 : 0;
    wglSwapIntervalEXT(interval);
    win->gl_swap_interval_resync_pending = false;
  }
  borderless_gl_manual =
      ((win->flags & STYGIAN_WINDOW_OPENGL) != 0u) &&
      ((win->flags & STYGIAN_WINDOW_BORDERLESS) != 0u) &&
      win->borderless_manual_maximized;
  watch_present_stall = borderless_gl_manual && win->gl_vsync_requested &&
                        win->gl_swap_control_supported &&
                        !win->gl_borderless_vsync_suspended;
  trace_present = stygian_win32_present_trace_enabled() || watch_present_stall;
  if (trace_present)
    present_start_ms = GetTickCount64();
  SwapBuffers(win->hdc);
  force_dwm_flush = stygian_win32_use_dwm_flush() ||
                    (win->gl_vsync_requested && !win->gl_swap_control_supported);
  if (force_dwm_flush) {
    DwmFlush();
  }
  if (present_start_ms > 0u) {
    present_end_ms = GetTickCount64();
    present_ms = (double)(present_end_ms - present_start_ms);
    if (watch_present_stall) {
      if (present_ms >= 120.0) {
        win->gl_borderless_present_stall_count++;
      } else {
        win->gl_borderless_present_stall_count = 0u;
      }
      if (win->gl_borderless_present_stall_count >= 3u && wglSwapIntervalEXT) {
        wglSwapIntervalEXT(0);
        win->gl_borderless_vsync_suspended = true;
        win->gl_borderless_present_stall_count = 0u;
        if (stygian_win32_present_trace_enabled()) {
          printf("[Stygian Win32] present fallback: swap interval forced to 0\n");
        }
      }
    } else if (!borderless_gl_manual) {
      win->gl_borderless_present_stall_count = 0u;
    }
    stygian_win32_trace_long_present(win, present_ms);
  }
}

void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled) {
  if (!win)
    return;
  win->gl_vsync_requested = enabled;

  if (!wglSwapIntervalEXT) {
    wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
  }

  if (wglSwapIntervalEXT) {
    win->gl_swap_control_supported = true;
    wglSwapIntervalEXT(enabled ? 1 : 0);
    win->gl_borderless_vsync_suspended = false;
    win->gl_swap_interval_resync_pending = false;
    win->gl_borderless_present_stall_count = 0u;
  } else {
    HMODULE gl = GetModuleHandle("opengl32.dll");
    if (gl) {
      wglSwapIntervalEXT =
          (PFNWGLSWAPINTERVALEXTPROC)GetProcAddress(gl, "wglSwapIntervalEXT");
      if (wglSwapIntervalEXT) {
        win->gl_swap_control_supported = true;
        wglSwapIntervalEXT(enabled ? 1 : 0);
        win->gl_borderless_vsync_suspended = false;
        win->gl_swap_interval_resync_pending = false;
        win->gl_borderless_present_stall_count = 0u;
        return;
      }
    }
    win->gl_swap_control_supported = false;
  }
}

void *stygian_window_gl_get_proc_address(const char *name) {
  void *p = (void *)wglGetProcAddress(name);
  if (!p) {
    HMODULE gl = GetModuleHandle("opengl32.dll");
    if (gl)
      p = (void *)GetProcAddress(gl, name);
  }
  return p;
}

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts) {
#ifdef STYGIAN_VULKAN
  static const char *exts[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                               VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
  uint32_t count = (uint32_t)(sizeof(exts) / sizeof(exts[0]));
  if (out_exts && max_exts >= count) {
    for (uint32_t i = 0; i < count; i++) {
      out_exts[i] = exts[i];
    }
  }
  return count;
#else
  (void)out_exts;
  (void)max_exts;
  return 0;
#endif
}

bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **vk_surface) {
#ifdef STYGIAN_VULKAN
  if (!win || !vk_instance || !vk_surface)
    return false;

  VkWin32SurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = GetModuleHandle(NULL),
      .hwnd = win->hwnd,
  };

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult res = vkCreateWin32SurfaceKHR((VkInstance)vk_instance, &info, NULL,
                                         &surface);
  if (res != VK_SUCCESS)
    return false;

  *vk_surface = (void *)surface;
  return true;
#else
  (void)win;
  (void)vk_instance;
  (void)vk_surface;
  return false;
#endif
}

void stygian_window_make_current(StygianWindow *win) {
  (void)win;
  // Deprecated: use stygian_window_gl_make_current
}

void stygian_window_swap_buffers(StygianWindow *win) {
  (void)win;
  // Swap is handled by backend (stygian_ap_swap).
}

void stygian_window_set_vsync(StygianWindow *win, bool enabled) {
  stygian_window_gl_set_vsync(win, enabled);
}

// ============================================================================
// Native Handle (for backends to use)
// ============================================================================

void *stygian_window_native_handle(StygianWindow *win) {
  return win ? win->hwnd : NULL;
}

void *stygian_window_native_context(StygianWindow *win) {
  return win ? win->hdc : NULL;
}

// ============================================================================
// Cursor
// ============================================================================

void stygian_window_set_cursor(StygianWindow *win, StygianCursor cursor) {
  (void)win;
  LPCSTR cur = IDC_ARROW;
  switch (cursor) {
  case STYGIAN_CURSOR_IBEAM:
    cur = IDC_IBEAM;
    break;
  case STYGIAN_CURSOR_CROSSHAIR:
    cur = IDC_CROSS;
    break;
  case STYGIAN_CURSOR_HAND:
    cur = IDC_HAND;
    break;
  case STYGIAN_CURSOR_RESIZE_H:
    cur = IDC_SIZEWE;
    break;
  case STYGIAN_CURSOR_RESIZE_V:
    cur = IDC_SIZENS;
    break;
  case STYGIAN_CURSOR_RESIZE_NWSE:
    cur = IDC_SIZENWSE;
    break;
  case STYGIAN_CURSOR_RESIZE_NESW:
    cur = IDC_SIZENESW;
    break;
  case STYGIAN_CURSOR_RESIZE_ALL:
    cur = IDC_SIZEALL;
    break;
  case STYGIAN_CURSOR_NOT_ALLOWED:
    cur = IDC_NO;
    break;
  default:
    break;
  }
  SetCursor(LoadCursor(NULL, cur));
}

void stygian_window_hide_cursor(StygianWindow *win) {
  (void)win;
  ShowCursor(FALSE);
}

void stygian_window_show_cursor(StygianWindow *win) {
  (void)win;
  ShowCursor(TRUE);
}

// ============================================================================
// DPI
// ============================================================================

float stygian_window_get_dpi_scale(StygianWindow *win) {
  if (!win || !win->hdc)
    return 1.0f;
  return (float)GetDeviceCaps(win->hdc, LOGPIXELSX) / 96.0f;
}

void stygian_window_get_framebuffer_size(StygianWindow *win, int *w, int *h) {
  stygian_window_get_size(win, w, h);
}

float stygian_window_get_scale(StygianWindow *win) {
  return stygian_window_get_dpi_scale(win);
}

void stygian_window_screen_to_client(StygianWindow *win, int screen_x,
                                     int screen_y, int *client_x,
                                     int *client_y) {
  if (!win || !win->hwnd)
    return;
  POINT pt = {screen_x, screen_y};
  ScreenToClient(win->hwnd, &pt);
  if (client_x)
    *client_x = pt.x;
  if (client_y)
    *client_y = pt.y;
}

// ============================================================================
// Input State Query
// ============================================================================

bool stygian_key_down(StygianWindow *win, StygianKey key) {
  return (win && key < STYGIAN_KEY_COUNT) ? win->keys[key] : false;
}

bool stygian_mouse_down(StygianWindow *win, StygianMouseButton button) {
  return (win && button < 5) ? win->mouse_buttons[button] : false;
}

void stygian_mouse_pos(StygianWindow *win, int *x, int *y) {
  if (win) {
    if (x)
      *x = win->mouse_x;
    if (y)
      *y = win->mouse_y;
  }
}

uint32_t stygian_get_mods(StygianWindow *win) { return win ? win->mods : 0; }

// ============================================================================
// Clipboard Implementation
// ============================================================================

void stygian_clipboard_write(StygianWindow *win, const char *text) {
  if (!win || !win->hwnd || !text)
    return;

  if (!OpenClipboard(win->hwnd))
    return;
  EmptyClipboard();

  size_t len = strlen(text);
  HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, len + 1);
  if (!hGlob) {
    CloseClipboard();
    return;
  }

  char *p = (char *)GlobalLock(hGlob);
  memcpy(p, text, len + 1);
  GlobalUnlock(hGlob);

  SetClipboardData(CF_TEXT, hGlob);
  CloseClipboard();
}

char *stygian_clipboard_read(StygianWindow *win) {
  if (!win || !win->hwnd)
    return NULL;

  if (!OpenClipboard(win->hwnd))
    return NULL;

  HANDLE hData = GetClipboardData(CF_TEXT);
  if (!hData) {
    CloseClipboard();
    return NULL;
  }

  char *pszText = (char *)GlobalLock(hData);
  if (!pszText) {
    CloseClipboard();
    return NULL;
  }

  // Copy out
  char *result = _strdup(pszText);

  GlobalUnlock(hData);
  CloseClipboard();

  return result;
}

#endif // _WIN32
