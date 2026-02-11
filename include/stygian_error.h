// stygian_error.h - Error Handling System for Stygian
// Part of Phase 5.5 - Advanced Features

#ifndef STYGIAN_ERROR_H
#define STYGIAN_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Error Codes
// ============================================================================

typedef enum StygianError {
  STYGIAN_OK = 0,

  // Initialization errors
  STYGIAN_ERROR_INIT_FAILED,
  STYGIAN_ERROR_BACKEND_NOT_SUPPORTED,
  STYGIAN_ERROR_WINDOW_CREATION_FAILED,

  // Resource errors
  STYGIAN_ERROR_OUT_OF_MEMORY,
  STYGIAN_ERROR_RESOURCE_NOT_FOUND,
  STYGIAN_ERROR_RESOURCE_LOAD_FAILED,

  // Rendering errors
  STYGIAN_ERROR_SHADER_COMPILATION_FAILED,
  STYGIAN_ERROR_PIPELINE_CREATION_FAILED,
  STYGIAN_ERROR_COMMAND_BUFFER_FULL,

  // State errors
  STYGIAN_ERROR_INVALID_STATE,
  STYGIAN_ERROR_INVALID_PARAMETER,
  STYGIAN_ERROR_CONTEXT_NOT_CURRENT,

  // Platform errors
  STYGIAN_ERROR_PLATFORM_SPECIFIC,

  STYGIAN_ERROR_COUNT
} StygianError;

// ============================================================================
// Error Callback System
// ============================================================================

typedef void (*StygianErrorCallback)(StygianError error, const char *message,
                                     void *user_data);

// Set global error callback
void stygian_set_error_callback(StygianErrorCallback callback, void *user_data);

// Get last error (thread-local)
StygianError stygian_get_last_error(void);

// Get error message
const char *stygian_error_string(StygianError error);

// Internal: Set error (used by Stygian implementation)
void stygian_set_error(StygianError error, const char *message);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_ERROR_H
