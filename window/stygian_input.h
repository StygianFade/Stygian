// stygian_input.h - Platform-Agnostic Input Events
// Part of Stygian UI Library
#ifndef STYGIAN_INPUT_H
#define STYGIAN_INPUT_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Event Types
// ============================================================================

typedef enum StygianEventType {
  STYGIAN_EVENT_NONE = 0,
  STYGIAN_EVENT_KEY_DOWN,
  STYGIAN_EVENT_KEY_UP,
  STYGIAN_EVENT_CHAR, // Text input (Unicode)
  STYGIAN_EVENT_MOUSE_MOVE,
  STYGIAN_EVENT_MOUSE_DOWN,
  STYGIAN_EVENT_MOUSE_UP,
  STYGIAN_EVENT_SCROLL,
  STYGIAN_EVENT_RESIZE,
  STYGIAN_EVENT_TICK, // Timer-driven evaluation tick (no direct input)
  STYGIAN_EVENT_FOCUS,
  STYGIAN_EVENT_BLUR,
  STYGIAN_EVENT_CLOSE
} StygianEventType;

// ============================================================================
// Mouse Buttons
// ============================================================================

typedef enum StygianMouseButton {
  STYGIAN_MOUSE_LEFT = 0,
  STYGIAN_MOUSE_RIGHT = 1,
  STYGIAN_MOUSE_MIDDLE = 2,
  STYGIAN_MOUSE_X1 = 3,
  STYGIAN_MOUSE_X2 = 4
} StygianMouseButton;

// ============================================================================
// Key Codes (Platform-Agnostic)
// ============================================================================

typedef enum StygianKey {
  STYGIAN_KEY_UNKNOWN = 0,

  // Letters
  STYGIAN_KEY_A,
  STYGIAN_KEY_B,
  STYGIAN_KEY_C,
  STYGIAN_KEY_D,
  STYGIAN_KEY_E,
  STYGIAN_KEY_F,
  STYGIAN_KEY_G,
  STYGIAN_KEY_H,
  STYGIAN_KEY_I,
  STYGIAN_KEY_J,
  STYGIAN_KEY_K,
  STYGIAN_KEY_L,
  STYGIAN_KEY_M,
  STYGIAN_KEY_N,
  STYGIAN_KEY_O,
  STYGIAN_KEY_P,
  STYGIAN_KEY_Q,
  STYGIAN_KEY_R,
  STYGIAN_KEY_S,
  STYGIAN_KEY_T,
  STYGIAN_KEY_U,
  STYGIAN_KEY_V,
  STYGIAN_KEY_W,
  STYGIAN_KEY_X,
  STYGIAN_KEY_Y,
  STYGIAN_KEY_Z,

  // Numbers
  STYGIAN_KEY_0,
  STYGIAN_KEY_1,
  STYGIAN_KEY_2,
  STYGIAN_KEY_3,
  STYGIAN_KEY_4,
  STYGIAN_KEY_5,
  STYGIAN_KEY_6,
  STYGIAN_KEY_7,
  STYGIAN_KEY_8,
  STYGIAN_KEY_9,

  // Function keys
  STYGIAN_KEY_F1,
  STYGIAN_KEY_F2,
  STYGIAN_KEY_F3,
  STYGIAN_KEY_F4,
  STYGIAN_KEY_F5,
  STYGIAN_KEY_F6,
  STYGIAN_KEY_F7,
  STYGIAN_KEY_F8,
  STYGIAN_KEY_F9,
  STYGIAN_KEY_F10,
  STYGIAN_KEY_F11,
  STYGIAN_KEY_F12,

  // Modifiers
  STYGIAN_KEY_SHIFT,
  STYGIAN_KEY_CTRL,
  STYGIAN_KEY_ALT,
  STYGIAN_KEY_SUPER,

  // Navigation
  STYGIAN_KEY_UP,
  STYGIAN_KEY_DOWN,
  STYGIAN_KEY_LEFT,
  STYGIAN_KEY_RIGHT,
  STYGIAN_KEY_HOME,
  STYGIAN_KEY_END,
  STYGIAN_KEY_PAGE_UP,
  STYGIAN_KEY_PAGE_DOWN,
  STYGIAN_KEY_INSERT,
  STYGIAN_KEY_DELETE,

  // Control
  STYGIAN_KEY_ESCAPE,
  STYGIAN_KEY_ENTER,
  STYGIAN_KEY_TAB,
  STYGIAN_KEY_BACKSPACE,
  STYGIAN_KEY_SPACE,

  // Punctuation
  STYGIAN_KEY_MINUS,
  STYGIAN_KEY_EQUALS,
  STYGIAN_KEY_LBRACKET,
  STYGIAN_KEY_RBRACKET,
  STYGIAN_KEY_BACKSLASH,
  STYGIAN_KEY_SEMICOLON,
  STYGIAN_KEY_APOSTROPHE,
  STYGIAN_KEY_COMMA,
  STYGIAN_KEY_PERIOD,
  STYGIAN_KEY_SLASH,
  STYGIAN_KEY_GRAVE,

  STYGIAN_KEY_COUNT
} StygianKey;

// ============================================================================
// Modifier Flags
// ============================================================================

typedef enum StygianMod {
  STYGIAN_MOD_NONE = 0,
  STYGIAN_MOD_SHIFT = (1 << 0),
  STYGIAN_MOD_CTRL = (1 << 1),
  STYGIAN_MOD_ALT = (1 << 2),
  STYGIAN_MOD_SUPER = (1 << 3) // Windows key / Cmd
} StygianMod;

// ============================================================================
// Event Structure
// ============================================================================

typedef struct StygianEvent {
  StygianEventType type;

  union {
    // Key events
    struct {
      StygianKey key;
      uint32_t mods; // StygianMod flags
      bool repeat;
    } key;

    // Char event (text input)
    struct {
      uint32_t codepoint; // Unicode codepoint
    } chr;

    // Mouse move
    struct {
      int x, y;   // Window-relative position
      int dx, dy; // Delta from last position
    } mouse_move;

    // Mouse button
    struct {
      int x, y;
      StygianMouseButton button;
      uint32_t mods;
      int clicks; // 1=single, 2=double
    } mouse_button;

    // Scroll
    struct {
      int x, y;
      float dx, dy; // Scroll delta (dy positive = up)
    } scroll;

    // Resize
    struct {
      int width, height;
    } resize;
  };
} StygianEvent;

// ============================================================================
// Input State Query
// ============================================================================

// These are implemented by the window backend
struct StygianWindow;

bool stygian_key_down(struct StygianWindow *win, StygianKey key);
bool stygian_mouse_down(struct StygianWindow *win, StygianMouseButton button);
void stygian_mouse_pos(struct StygianWindow *win, int *x, int *y);
uint32_t stygian_get_mods(struct StygianWindow *win);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_INPUT_H
