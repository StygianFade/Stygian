// stygian_color.h - Color profile conversion helpers
#ifndef STYGIAN_COLOR_H
#define STYGIAN_COLOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum StygianColorSpace {
  STYGIAN_COLOR_SPACE_UNKNOWN = 0,
  STYGIAN_COLOR_SPACE_SRGB = 1,
  STYGIAN_COLOR_SPACE_DISPLAY_P3 = 2,
  STYGIAN_COLOR_SPACE_BT2020 = 3,
} StygianColorSpace;

typedef struct StygianColorProfile {
  StygianColorSpace space;
  float rgb_to_xyz[9];
  float xyz_to_rgb[9];
  float gamma;
  bool srgb_transfer;
  bool valid;
  char name[64];
} StygianColorProfile;

void stygian_color_profile_init_builtin(StygianColorProfile *profile,
                                        StygianColorSpace space);
bool stygian_color_profile_init_custom(StygianColorProfile *profile,
                                       const char *name,
                                       const float rgb_to_xyz[9],
                                       bool srgb_transfer, float gamma);
bool stygian_color_profile_copy(StygianColorProfile *dst,
                                const StygianColorProfile *src);

// Transform one RGB triplet in-place from src profile to dst profile.
void stygian_color_transform_rgb_f32(const StygianColorProfile *src,
                                     const StygianColorProfile *dst, float *r,
                                     float *g, float *b);

// Transform RGBA8 buffer in-place. Alpha is preserved.
void stygian_color_transform_rgba8(const StygianColorProfile *src,
                                   const StygianColorProfile *dst,
                                   uint8_t *rgba, size_t pixel_count);

#endif // STYGIAN_COLOR_H
