// stygian_icc.h - Minimal ICC profile loader for color-space selection
#ifndef STYGIAN_ICC_H
#define STYGIAN_ICC_H

#include "stygian_color.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct StygianICCInfo {
  char path[260];
  char description[128];
  bool loaded;
} StygianICCInfo;

// Load ICC profile and populate color profile as best-effort.
// Returns false if profile cannot be parsed/loaded.
bool stygian_icc_load_profile(const char *path, StygianColorProfile *out_profile,
                              StygianICCInfo *out_info);

#endif // STYGIAN_ICC_H
