#include "stygian_icc.h"
#include "stygian_internal.h" // stygian_cpystr

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ICCTagRecord {
  uint32_t sig;
  uint32_t offset;
  uint32_t size;
} ICCTagRecord;

static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int32_t be32s(const uint8_t *p) { return (int32_t)be32(p); }

static uint32_t fourcc(const char a, const char b, const char c, const char d) {
  return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16) |
         ((uint32_t)(uint8_t)c << 8) | (uint32_t)(uint8_t)d;
}

static bool read_file(const char *path, uint8_t **out_data, size_t *out_size) {
  FILE *f;
  long n;
  uint8_t *buf;
  if (!path || !out_data || !out_size)
    return false;
  f = fopen(path, "rb");
  if (!f)
    return false;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  n = ftell(f);
  if (n <= 0) {
    fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }
  buf = (uint8_t *)malloc((size_t)n);
  if (!buf) {
    fclose(f);
    return false;
  }
  if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf);
    fclose(f);
    return false;
  }
  fclose(f);
  *out_data = buf;
  *out_size = (size_t)n;
  return true;
}

static const ICCTagRecord *find_tag(const ICCTagRecord *tags, uint32_t count,
                                    uint32_t sig) {
  uint32_t i;
  for (i = 0; i < count; i++) {
    if (tags[i].sig == sig)
      return &tags[i];
  }
  return NULL;
}

static bool parse_xyz_tag(const uint8_t *buf, size_t len, const ICCTagRecord *t,
                          float out_xyz[3]) {
  const uint8_t *p;
  if (!buf || !t || !out_xyz)
    return false;
  if ((size_t)t->offset + (size_t)t->size > len || t->size < 20u)
    return false;
  p = buf + t->offset;
  if (be32(p) != fourcc('X', 'Y', 'Z', ' '))
    return false;
  p += 8; // type + reserved
  out_xyz[0] = (float)be32s(p + 0) / 65536.0f;
  out_xyz[1] = (float)be32s(p + 4) / 65536.0f;
  out_xyz[2] = (float)be32s(p + 8) / 65536.0f;
  return true;
}

static bool parse_desc_tag(const uint8_t *buf, size_t len,
                           const ICCTagRecord *t, char *out, size_t out_size) {
  const uint8_t *p;
  uint32_t text_len;
  if (!buf || !t || !out || out_size == 0u)
    return false;
  if ((size_t)t->offset + (size_t)t->size > len || t->size < 16u)
    return false;
  p = buf + t->offset;
  if (be32(p) != fourcc('d', 'e', 's', 'c'))
    return false;
  text_len = be32(p + 8);
  if (text_len == 0u)
    return false;
  if (12u + text_len > t->size)
    return false;
  if (text_len >= out_size)
    text_len = (uint32_t)out_size - 1u;
  memcpy(out, p + 12, text_len);
  out[text_len] = '\0';
  return true;
}

bool stygian_icc_load_profile(const char *path,
                              StygianColorProfile *out_profile,
                              StygianICCInfo *out_info) {
  uint8_t *buf = NULL;
  size_t len = 0;
  uint32_t tag_count;
  ICCTagRecord *tags = NULL;
  uint32_t i;
  uint32_t color_space_sig;
  float r_xyz[3], g_xyz[3], b_xyz[3];
  bool has_r = false, has_g = false, has_b = false;

  if (!path || !out_profile)
    return false;

  stygian_color_profile_init_builtin(out_profile, STYGIAN_COLOR_SPACE_SRGB);
  if (out_info) {
    memset(out_info, 0, sizeof(*out_info));
    stygian_cpystr(out_info->path, sizeof(out_info->path), path);
  }

  if (!read_file(path, &buf, &len))
    return false;
  if (len < 132u)
    goto fail;
  if (be32(buf + 36) != fourcc('a', 'c', 's', 'p'))
    goto fail;

  color_space_sig = be32(buf + 16);
  if (color_space_sig != fourcc('R', 'G', 'B', ' '))
    goto fail;

  tag_count = be32(buf + 128);
  if (tag_count > 4096u)
    goto fail;
  if (128u + 4u + (size_t)tag_count * 12u > len)
    goto fail;

  tags =
      (ICCTagRecord *)calloc(tag_count ? tag_count : 1u, sizeof(ICCTagRecord));
  if (!tags)
    goto fail;
  for (i = 0; i < tag_count; i++) {
    const uint8_t *p = buf + 128u + 4u + (size_t)i * 12u;
    tags[i].sig = be32(p + 0);
    tags[i].offset = be32(p + 4);
    tags[i].size = be32(p + 8);
  }

  {
    const ICCTagRecord *d =
        find_tag(tags, tag_count, fourcc('d', 'e', 's', 'c'));
    if (d && out_info) {
      parse_desc_tag(buf, len, d, out_info->description,
                     sizeof(out_info->description));
    }
  }

  {
    const ICCTagRecord *t;
    t = find_tag(tags, tag_count, fourcc('r', 'X', 'Y', 'Z'));
    if (t)
      has_r = parse_xyz_tag(buf, len, t, r_xyz);
    t = find_tag(tags, tag_count, fourcc('g', 'X', 'Y', 'Z'));
    if (t)
      has_g = parse_xyz_tag(buf, len, t, g_xyz);
    t = find_tag(tags, tag_count, fourcc('b', 'X', 'Y', 'Z'));
    if (t)
      has_b = parse_xyz_tag(buf, len, t, b_xyz);
  }

  if (has_r && has_g && has_b) {
    float m[9];
    m[0] = r_xyz[0];
    m[1] = g_xyz[0];
    m[2] = b_xyz[0];
    m[3] = r_xyz[1];
    m[4] = g_xyz[1];
    m[5] = b_xyz[1];
    m[6] = r_xyz[2];
    m[7] = g_xyz[2];
    m[8] = b_xyz[2];
    if (!stygian_color_profile_init_custom(out_profile, "ICC RGB", m, true,
                                           2.4f)) {
      stygian_color_profile_init_builtin(out_profile, STYGIAN_COLOR_SPACE_SRGB);
    }
  } else {
    // Heuristic fallback by description/path
    const char *probe = NULL;
    if (out_info && out_info->description[0])
      probe = out_info->description;
    else
      probe = path;
    if (probe && (strstr(probe, "P3") || strstr(probe, "Display P3") ||
                  strstr(probe, "display p3"))) {
      stygian_color_profile_init_builtin(out_profile,
                                         STYGIAN_COLOR_SPACE_DISPLAY_P3);
    } else if (probe && (strstr(probe, "2020") || strstr(probe, "BT.2020") ||
                         strstr(probe, "Rec.2020"))) {
      stygian_color_profile_init_builtin(out_profile,
                                         STYGIAN_COLOR_SPACE_BT2020);
    } else {
      stygian_color_profile_init_builtin(out_profile, STYGIAN_COLOR_SPACE_SRGB);
    }
  }

  if (out_info)
    out_info->loaded = true;
  free(tags);
  free(buf);
  return true;

fail:
  free(tags);
  free(buf);
  return false;
}
