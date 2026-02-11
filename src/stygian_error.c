// stygian_error.c - Error Handling Implementation
// Part of Phase 5.5 - Advanced Features

#include "../include/stygian_error.h"
#include "stygian_internal.h" // stygian_cpystr
#include <string.h>

// ============================================================================
// Thread-Local Error Storage
// ============================================================================

#ifdef _WIN32
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif

static THREAD_LOCAL StygianError g_last_error = STYGIAN_OK;
static THREAD_LOCAL char g_error_message[256] = {0};

// Global error callback
static StygianErrorCallback g_error_callback = NULL;
static void *g_error_callback_user_data = NULL;

// ============================================================================
// Error Strings
// ============================================================================

static const char *g_error_strings[STYGIAN_ERROR_COUNT] = {
    "No error",
    "Initialization failed",
    "Backend not supported",
    "Window creation failed",
    "Out of memory",
    "Resource not found",
    "Resource load failed",
    "Shader compilation failed",
    "Pipeline creation failed",
    "Command buffer full",
    "Invalid state",
    "Invalid parameter",
    "Context not current",
    "Platform-specific error"};

// ============================================================================
// Public API
// ============================================================================

void stygian_set_error_callback(StygianErrorCallback callback,
                                void *user_data) {
  g_error_callback = callback;
  g_error_callback_user_data = user_data;
}

StygianError stygian_get_last_error(void) { return g_last_error; }

const char *stygian_error_string(StygianError error) {
  if (error < 0 || error >= STYGIAN_ERROR_COUNT) {
    return "Unknown error";
  }
  return g_error_strings[error];
}

void stygian_set_error(StygianError error, const char *message) {
  g_last_error = error;

  if (message) {
    stygian_cpystr(g_error_message, sizeof(g_error_message), message);
  } else {
    g_error_message[0] = '\0';
  }

  // Invoke callback if set
  if (g_error_callback) {
    const char *msg = message ? message : stygian_error_string(error);
    g_error_callback(error, msg, g_error_callback_user_data);
  }
}
