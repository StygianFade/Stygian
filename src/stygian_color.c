#include "stygian_color.h"
#include "stygian_internal.h" // stygian_cpystr

#include <math.h>
#include <string.h>

static float clamp01(float v) {
  if (v < 0.0f)
    return 0.0f;
  if (v > 1.0f)
    return 1.0f;
  return v;
}

static float srgb_to_linear(float c) {
  c = clamp01(c);
  if (c <= 0.04045f)
    return c / 12.92f;
  return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float linear_to_srgb(float c) {
  c = clamp01(c);
  if (c <= 0.0031308f)
    return c * 12.92f;
  return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static void mul3x3_vec(const float m[9], float x, float y, float z, float *ox,
                       float *oy, float *oz) {
  *ox = m[0] * x + m[1] * y + m[2] * z;
  *oy = m[3] * x + m[4] * y + m[5] * z;
  *oz = m[6] * x + m[7] * y + m[8] * z;
}

static bool invert3x3(const float m[9], float out[9]) {
  float a = m[0], b = m[1], c = m[2];
  float d = m[3], e = m[4], f = m[5];
  float g = m[6], h = m[7], i = m[8];
  float A = e * i - f * h;
  float B = -(d * i - f * g);
  float C = d * h - e * g;
  float D = -(b * i - c * h);
  float E = a * i - c * g;
  float F = -(a * h - b * g);
  float G = b * f - c * e;
  float H = -(a * f - c * d);
  float I = a * e - b * d;
  float det = a * A + b * B + c * C;
  float inv_det;

  if (fabsf(det) < 1e-12f)
    return false;
  inv_det = 1.0f / det;

  out[0] = A * inv_det;
  out[1] = D * inv_det;
  out[2] = G * inv_det;
  out[3] = B * inv_det;
  out[4] = E * inv_det;
  out[5] = H * inv_det;
  out[6] = C * inv_det;
  out[7] = F * inv_det;
  out[8] = I * inv_det;
  return true;
}

static void set_profile(StygianColorProfile *profile, StygianColorSpace space,
                        const char *name, const float rgb_to_xyz[9],
                        bool srgb_xfer, float gamma) {
  if (!profile)
    return;
  memset(profile, 0, sizeof(*profile));
  profile->space = space;
  memcpy(profile->rgb_to_xyz, rgb_to_xyz, sizeof(float) * 9);
  profile->srgb_transfer = srgb_xfer;
  profile->gamma = gamma;
  profile->valid = invert3x3(profile->rgb_to_xyz, profile->xyz_to_rgb);
  if (name) {
    stygian_cpystr(profile->name, sizeof(profile->name), name);
  }
}

bool stygian_color_profile_init_custom(StygianColorProfile *profile,
                                       const char *name,
                                       const float rgb_to_xyz[9],
                                       bool srgb_transfer, float gamma) {
  if (!profile || !rgb_to_xyz)
    return false;
  set_profile(profile, STYGIAN_COLOR_SPACE_UNKNOWN, name, rgb_to_xyz,
              srgb_transfer, gamma);
  return profile->valid;
}

void stygian_color_profile_init_builtin(StygianColorProfile *profile,
                                        StygianColorSpace space) {
  static const float srgb_to_xyz[9] = {
      0.4124564f, 0.3575761f, 0.1804375f, 0.2126729f, 0.7151522f,
      0.0721750f, 0.0193339f, 0.1191920f, 0.9503041f,
  };
  static const float p3_to_xyz[9] = {
      0.48657095f, 0.26566769f, 0.19821729f, 0.22897456f, 0.69173852f,
      0.07928691f, 0.00000000f, 0.04511338f, 1.04394437f,
  };
  static const float bt2020_to_xyz[9] = {
      0.6369580f, 0.1446169f, 0.1688809f, 0.2627002f, 0.6779981f,
      0.0593017f, 0.0000000f, 0.0280727f, 1.0609851f,
  };

  switch (space) {
  case STYGIAN_COLOR_SPACE_DISPLAY_P3:
    set_profile(profile, space, "Display P3", p3_to_xyz, true, 2.4f);
    break;
  case STYGIAN_COLOR_SPACE_BT2020:
    set_profile(profile, space, "BT.2020", bt2020_to_xyz, false, 2.4f);
    break;
  case STYGIAN_COLOR_SPACE_SRGB:
  default:
    set_profile(profile, STYGIAN_COLOR_SPACE_SRGB, "sRGB", srgb_to_xyz, true,
                2.4f);
    break;
  }
}

bool stygian_color_profile_copy(StygianColorProfile *dst,
                                const StygianColorProfile *src) {
  if (!dst || !src)
    return false;
  memcpy(dst, src, sizeof(*dst));
  return true;
}

void stygian_color_transform_rgb_f32(const StygianColorProfile *src,
                                     const StygianColorProfile *dst, float *r,
                                     float *g, float *b) {
  float in_r, in_g, in_b;
  float x, y, z;
  float out_r, out_g, out_b;

  if (!r || !g || !b)
    return;

  in_r = clamp01(*r);
  in_g = clamp01(*g);
  in_b = clamp01(*b);

  if (!src || !dst || !src->valid || !dst->valid) {
    *r = in_r;
    *g = in_g;
    *b = in_b;
    return;
  }

  if (src->srgb_transfer) {
    in_r = srgb_to_linear(in_r);
    in_g = srgb_to_linear(in_g);
    in_b = srgb_to_linear(in_b);
  } else {
    float inv_gamma = (src->gamma > 0.0f) ? src->gamma : 2.2f;
    in_r = powf(in_r, inv_gamma);
    in_g = powf(in_g, inv_gamma);
    in_b = powf(in_b, inv_gamma);
  }

  mul3x3_vec(src->rgb_to_xyz, in_r, in_g, in_b, &x, &y, &z);
  mul3x3_vec(dst->xyz_to_rgb, x, y, z, &out_r, &out_g, &out_b);

  out_r = clamp01(out_r);
  out_g = clamp01(out_g);
  out_b = clamp01(out_b);

  if (dst->srgb_transfer) {
    out_r = linear_to_srgb(out_r);
    out_g = linear_to_srgb(out_g);
    out_b = linear_to_srgb(out_b);
  } else {
    float gamma = (dst->gamma > 0.0f) ? dst->gamma : 2.2f;
    out_r = powf(out_r, 1.0f / gamma);
    out_g = powf(out_g, 1.0f / gamma);
    out_b = powf(out_b, 1.0f / gamma);
  }

  *r = clamp01(out_r);
  *g = clamp01(out_g);
  *b = clamp01(out_b);
}

void stygian_color_transform_rgba8(const StygianColorProfile *src,
                                   const StygianColorProfile *dst,
                                   uint8_t *rgba, size_t pixel_count) {
  size_t i;
  if (!rgba || pixel_count == 0)
    return;
  if (!src || !dst || !src->valid || !dst->valid)
    return;
  for (i = 0; i < pixel_count; i++) {
    float r = rgba[i * 4 + 0] / 255.0f;
    float g = rgba[i * 4 + 1] / 255.0f;
    float b = rgba[i * 4 + 2] / 255.0f;
    stygian_color_transform_rgb_f32(src, dst, &r, &g, &b);
    rgba[i * 4 + 0] = (uint8_t)(clamp01(r) * 255.0f + 0.5f);
    rgba[i * 4 + 1] = (uint8_t)(clamp01(g) * 255.0f + 0.5f);
    rgba[i * 4 + 2] = (uint8_t)(clamp01(b) * 255.0f + 0.5f);
  }
}
