// stygian_mtsdf.c - MTSDF Atlas Loading Implementation (NO GL - pure file I/O)
#include "stygian_mtsdf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple JSON parsing helpers (minimal, specific to atlas.json format)
static const char *skip_whitespace(const char *p) {
  while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
    p++;
  return p;
}

static const char *find_key(const char *json, const char *key) {
  char search[64];
  snprintf(search, sizeof(search), "\"%s\"", key);
  const char *found = strstr(json, search);
  if (found) {
    found += strlen(search);
    found = skip_whitespace(found);
    if (*found == ':')
      found++;
    found = skip_whitespace(found);
  }
  return found;
}

static double parse_number(const char *p, const char **end) {
  char buf[64];
  int i = 0;
  while (*p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' ||
                *p == '+' || *p == 'e' || *p == 'E')) {
    if (i < 63)
      buf[i++] = *p;
    p++;
  }
  buf[i] = 0;
  if (end)
    *end = p;
  double val = strtod(buf, NULL);
  if (!isfinite(val))
    return 0.0;
  return val;
}

static int parse_int(const char *p, const char **end) {
  return (int)parse_number(p, end);
}

static uint32_t mtsdf_hash_u32(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7feb352du;
  v ^= v >> 15;
  v *= 0x846ca68bu;
  v ^= v >> 16;
  return v;
}

static int mtsdf_next_pow2(int v) {
  int p = 1;
  while (p < v && p > 0)
    p <<= 1;
  return p > 0 ? p : v;
}

static bool mtsdf_rebuild_hash(MTSDFAtlas *atlas, int min_capacity) {
  int i;
  int cap;
  int *hash;
  if (!atlas)
    return false;
  cap = mtsdf_next_pow2(min_capacity < 16 ? 16 : min_capacity);
  hash = (int *)malloc((size_t)cap * sizeof(int));
  if (!hash)
    return false;
  for (i = 0; i < cap; i++)
    hash[i] = -1;

  for (i = 0; i < atlas->glyph_count; i++) {
    uint32_t codepoint = atlas->glyph_entries[i].codepoint;
    int slot = (int)(mtsdf_hash_u32(codepoint) & (uint32_t)(cap - 1));
    while (hash[slot] != -1)
      slot = (slot + 1) & (cap - 1);
    hash[slot] = i;
  }

  if (atlas->glyph_hash)
    free(atlas->glyph_hash);
  atlas->glyph_hash = hash;
  atlas->glyph_hash_capacity = cap;
  return true;
}

static int mtsdf_find_glyph_index(const MTSDFAtlas *atlas, uint32_t codepoint) {
  int slot;
  if (!atlas || !atlas->glyph_hash || atlas->glyph_hash_capacity <= 0)
    return -1;
  slot = (int)(mtsdf_hash_u32(codepoint) &
               (uint32_t)(atlas->glyph_hash_capacity - 1));
  while (atlas->glyph_hash[slot] != -1) {
    int idx = atlas->glyph_hash[slot];
    if (idx >= 0 && idx < atlas->glyph_count &&
        atlas->glyph_entries[idx].codepoint == codepoint)
      return idx;
    slot = (slot + 1) & (atlas->glyph_hash_capacity - 1);
  }
  return -1;
}

static bool mtsdf_add_glyph(MTSDFAtlas *atlas, uint32_t codepoint,
                            const MTSDFGlyph *glyph) {
  int idx;
  if (!atlas || !glyph)
    return false;

  if (codepoint < 256u) {
    atlas->glyphs[codepoint] = *glyph;
  }

  idx = mtsdf_find_glyph_index(atlas, codepoint);
  if (idx >= 0) {
    atlas->glyph_entries[idx].glyph = *glyph;
    return true;
  }

  if (atlas->glyph_count >= atlas->glyph_capacity) {
    int new_capacity = atlas->glyph_capacity ? atlas->glyph_capacity * 2 : 512;
    MTSDFGlyphEntry *new_entries = (MTSDFGlyphEntry *)realloc(
        atlas->glyph_entries, (size_t)new_capacity * sizeof(MTSDFGlyphEntry));
    if (!new_entries)
      return false;
    atlas->glyph_entries = new_entries;
    atlas->glyph_capacity = new_capacity;
  }

  idx = atlas->glyph_count++;
  atlas->glyph_entries[idx].codepoint = codepoint;
  atlas->glyph_entries[idx].glyph = *glyph;

  if (!atlas->glyph_hash ||
      atlas->glyph_hash_capacity < atlas->glyph_count * 2) {
    if (!mtsdf_rebuild_hash(atlas, atlas->glyph_count * 2))
      return false;
  } else {
    int slot = (int)(mtsdf_hash_u32(codepoint) &
                     (uint32_t)(atlas->glyph_hash_capacity - 1));
    while (atlas->glyph_hash[slot] != -1)
      slot = (slot + 1) & (atlas->glyph_hash_capacity - 1);
    atlas->glyph_hash[slot] = idx;
  }

  return true;
}

// Parse a single glyph object from JSON
static const char *parse_glyph(const char *p, MTSDFGlyph *g, int *unicode,
                               int atlas_w, int atlas_h) {
  *unicode = -1;
  memset(g, 0, sizeof(MTSDFGlyph));

  // Skip to opening brace
  while (*p && *p != '{')
    p++;
  if (!*p)
    return p;
  p++;

  // Find closing brace
  const char *end = p;
  int depth = 1;
  while (*end && depth > 0) {
    if (*end == '{')
      depth++;
    else if (*end == '}')
      depth--;
    end++;
  }

  // Parse fields within this object
  const char *unicode_pos = find_key(p, "unicode");
  if (unicode_pos && unicode_pos < end) {
    *unicode = parse_int(unicode_pos, NULL);
  }

  const char *advance_pos = find_key(p, "advance");
  if (advance_pos && advance_pos < end) {
    g->advance = (float)parse_number(advance_pos, NULL);
    g->has_glyph = true;
  }

  // Parse planeBounds (glyph positioning in em units)
  const char *plane_pos = find_key(p, "planeBounds");
  if (plane_pos && plane_pos < end) {
    const char *left = find_key(plane_pos, "left");
    const char *bottom = find_key(plane_pos, "bottom");
    const char *right = find_key(plane_pos, "right");
    const char *top = find_key(plane_pos, "top");

    if (left)
      g->plane_left = (float)parse_number(left, NULL);
    if (bottom)
      g->plane_bottom = (float)parse_number(bottom, NULL);
    if (right)
      g->plane_right = (float)parse_number(right, NULL);
    if (top)
      g->plane_top = (float)parse_number(top, NULL);
  }

  // Parse atlasBounds (texture coordinates in pixels)
  const char *atlas_pos = find_key(p, "atlasBounds");
  if (atlas_pos && atlas_pos < end) {
    const char *left = find_key(atlas_pos, "left");
    const char *bottom = find_key(atlas_pos, "bottom");
    const char *right = find_key(atlas_pos, "right");
    const char *top = find_key(atlas_pos, "top");

    float aleft = 0, abottom = 0, aright = 0, atop = 0;
    if (left)
      aleft = (float)parse_number(left, NULL);
    if (bottom)
      abottom = (float)parse_number(bottom, NULL);
    if (right)
      aright = (float)parse_number(right, NULL);
    if (top)
      atop = (float)parse_number(top, NULL);

    // Convert pixel coordinates to normalized UVs
    g->u0 = aleft / (float)atlas_w;
    g->v0 = (atlas_h - abottom) / (float)atlas_h;
    g->u1 = aright / (float)atlas_w;
    g->v1 = (atlas_h - atop) / (float)atlas_h;
  }

  return end;
}

bool mtsdf_load_atlas(MTSDFAtlas *atlas, const char *png_path,
                      const char *json_path) {
  if (!atlas || !png_path || !json_path)
    return false;

  memset(atlas, 0, sizeof(MTSDFAtlas));

  // Load PNG image - keep raw pixels for backend to upload
  int w, h, channels;
  unsigned char *data = stbi_load(png_path, &w, &h, &channels, 4);
  if (!data) {
    fprintf(stderr, "MTSDF: Failed to load atlas image: %s\n", png_path);
    return false;
  }

  atlas->atlas_width = w;
  atlas->atlas_height = h;
  atlas->pixels = data; // Keep raw pixels - caller will upload via backend

  // Load JSON metadata
  FILE *f = fopen(json_path, "rb");
  if (!f) {
    fprintf(stderr, "MTSDF: Failed to load atlas JSON: %s\n", json_path);
    stbi_image_free(atlas->pixels);
    atlas->pixels = NULL;
    return false;
  }

  fseek(f, 0, SEEK_END);
  long json_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *json = (char *)malloc(json_size + 1);
  fread(json, 1, json_size, f);
  json[json_size] = 0;
  fclose(f);

  // Parse atlas metadata
  const char *atlas_section = find_key(json, "atlas");
  if (atlas_section) {
    const char *px_range = find_key(atlas_section, "distanceRange");
    if (px_range)
      atlas->px_range = (float)parse_number(px_range, NULL);

    const char *size = find_key(atlas_section, "size");
    if (size)
      atlas->em_size = (float)parse_number(size, NULL);
  }

  // Parse metrics
  const char *metrics_section = find_key(json, "metrics");
  if (metrics_section) {
    const char *line_height = find_key(metrics_section, "lineHeight");
    if (line_height)
      atlas->line_height = (float)parse_number(line_height, NULL);

    const char *ascender = find_key(metrics_section, "ascender");
    if (ascender)
      atlas->ascender = (float)parse_number(ascender, NULL);

    const char *descender = find_key(metrics_section, "descender");
    if (descender)
      atlas->descender = (float)parse_number(descender, NULL);
  }

  // Parse glyphs array
  const char *glyphs_section = find_key(json, "glyphs");
  if (glyphs_section) {
    while (*glyphs_section && *glyphs_section != '[')
      glyphs_section++;
    if (*glyphs_section == '[')
      glyphs_section++;

    const char *p = glyphs_section;
    while (*p && *p != ']') {
      int unicode = -1;
      MTSDFGlyph g;
      p = parse_glyph(p, &g, &unicode, w, h);

      if (unicode >= 0) {
        (void)mtsdf_add_glyph(atlas, (uint32_t)unicode, &g);
      }

      while (*p && (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r'))
        p++;
    }
  }

  // Parse kerning array
  const char *kerning_section = find_key(json, "kerning");
  if (kerning_section) {
    const char *count_p = kerning_section;
    int count = 0;
    while (*count_p && *count_p != ']') {
      if (*count_p == '{')
        count++;
      count_p++;
    }

    if (count > 0) {
      atlas->kerning = (MTSDFKernPair *)malloc(sizeof(MTSDFKernPair) * count);
      atlas->kerning_count = 0;

      while (*kerning_section && *kerning_section != '[')
        kerning_section++;
      if (*kerning_section == '[')
        kerning_section++;

      const char *p = kerning_section;
      while (*p && *p != ']' && atlas->kerning_count < count) {
        while (*p && *p != '{')
          p++;
        if (!*p)
          break;
        p++;

        const char *end = p;
        int depth = 1;
        while (*end && depth > 0) {
          if (*end == '{')
            depth++;
          else if (*end == '}')
            depth--;
          end++;
        }

        MTSDFKernPair *kp = &atlas->kerning[atlas->kerning_count];

        const char *u1 = find_key(p, "unicode1");
        const char *u2 = find_key(p, "unicode2");
        const char *adv = find_key(p, "advance");

        if (u1 && u2 && adv) {
          kp->unicode1 = parse_int(u1, NULL);
          kp->unicode2 = parse_int(u2, NULL);
          kp->advance = (float)parse_number(adv, NULL);
          atlas->kerning_count++;
        }

        p = end;
      }
    }
  }

  // Build O(1) kerning lookup for ASCII range
  if (atlas->kerning_count > 0) {
    for (int i = 0; i < atlas->kerning_count; i++) {
      int c1 = atlas->kerning[i].unicode1;
      int c2 = atlas->kerning[i].unicode2;
      if (c1 >= 0 && c1 < 256 && c2 >= 0 && c2 < 256) {
        atlas->kerning_table[c1][c2] = atlas->kerning[i].advance;
        atlas->kerning_has[c1][c2] = true;
      }
    }
    atlas->kerning_ready = true;
  }

  free(json);
  atlas->loaded = true;

  printf("MTSDF: Loaded atlas %dx%d, px_range=%.1f, em_size=%.0f, %d kerning "
         "pairs\n",
         w, h, atlas->px_range, atlas->em_size, atlas->kerning_count);

  return true;
}

void mtsdf_free_atlas(MTSDFAtlas *atlas) {
  if (!atlas)
    return;

  // Free raw pixels if still present
  if (atlas->pixels) {
    stbi_image_free(atlas->pixels);
    atlas->pixels = NULL;
  }

  if (atlas->kerning) {
    free(atlas->kerning);
    atlas->kerning = NULL;
  }
  if (atlas->glyph_entries) {
    free(atlas->glyph_entries);
    atlas->glyph_entries = NULL;
  }
  if (atlas->glyph_hash) {
    free(atlas->glyph_hash);
    atlas->glyph_hash = NULL;
  }

  atlas->kerning_count = 0;
  atlas->kerning_ready = false;
  atlas->glyph_count = 0;
  atlas->glyph_capacity = 0;
  atlas->glyph_hash_capacity = 0;
  atlas->loaded = false;
}

float mtsdf_get_kerning(const MTSDFAtlas *atlas, int char1, int char2) {
  if (!atlas)
    return 0.0f;

  if (atlas->kerning_ready && char1 >= 0 && char1 < 256 && char2 >= 0 &&
      char2 < 256 && atlas->kerning_has[char1][char2]) {
    return atlas->kerning_table[char1][char2];
  }

  if (!atlas->kerning)
    return 0.0f;

  for (int i = 0; i < atlas->kerning_count; i++) {
    if (atlas->kerning[i].unicode1 == char1 &&
        atlas->kerning[i].unicode2 == char2) {
      return atlas->kerning[i].advance;
    }
  }
  return 0.0f;
}

const MTSDFGlyph *mtsdf_get_glyph(const MTSDFAtlas *atlas, uint32_t codepoint) {
  int idx;
  if (!atlas)
    return NULL;
  if (codepoint < 256u) {
    if (atlas->glyphs[codepoint].has_glyph)
      return &atlas->glyphs[codepoint];
    return NULL;
  }
  idx = mtsdf_find_glyph_index(atlas, codepoint);
  if (idx < 0 || idx >= atlas->glyph_count)
    return NULL;
  return &atlas->glyph_entries[idx].glyph;
}
