#ifndef STYGIAN_PLATFORM_H
#define STYGIAN_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

// ============================================================================
// Platform Abstraction Helpers
// ============================================================================

// Check if a path (file or directory) exists
static inline bool stygian_path_exists(const char *path) {
  if (!path || !path[0])
    return false;
#ifdef _WIN32
  DWORD attrib = GetFileAttributesA(path);
  return (attrib != INVALID_FILE_ATTRIBUTES);
#else
  struct stat buffer;
  return (stat(path, &buffer) == 0);
#endif
}

// Get the directory containing the executable
// Returns true on success, false on failure (buffer too small or error)
static inline bool stygian_get_binary_dir(char *buffer, size_t size) {
#ifdef _WIN32
  if (!buffer || size == 0)
    return false;
  DWORD len = GetModuleFileNameA(NULL, buffer, (DWORD)size);
  if (len == 0 || len == size)
    return false;

  // Strip filename to get directory
  char *last_slash = strrchr(buffer, '\\');
  if (last_slash) {
    *last_slash = '\0';
  }
  return true;
#else
  if (!buffer || size == 0)
    return false;
  ssize_t len = 0;
#ifdef __APPLE__
  uint32_t bufsize = (uint32_t)size;
  if (_NSGetExecutablePath(buffer, &bufsize) != 0)
    return false;
  len = strlen(buffer);
#else
  len = readlink("/proc/self/exe", buffer, size - 1);
  if (len == -1)
    return false;
  buffer[len] = '\0';
#endif

  // Strip filename to get directory
  char *last_slash = strrchr(buffer, '/');
  if (last_slash) {
    *last_slash = '\0';
  }
  return true;
#endif
}

#endif // STYGIAN_PLATFORM_H
