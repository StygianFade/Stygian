// stygian_mtsdf.h - MTSDF Font Atlas Loading and Rendering
#ifndef STYGIAN_MTSDF_H
#define STYGIAN_MTSDF_H

#include <stdbool.h>
#include <stdint.h>

// Forward declaration
struct StygianContext;

// Glyph information from MTSDF atlas
typedef struct {
  float u0, v0, u1, v1; // UV coordinates in atlas (normalized 0-1)
  float advance;        // Horizontal advance (normalized to em)
  float plane_left;     // Left bearing in em units
  float plane_bottom;   // Bottom bearing in em units
  float plane_right;    // Right edge in em units
  float plane_top;      // Top edge in em units
  bool has_glyph;       // Whether this glyph exists
} MTSDFGlyph;

typedef struct {
  uint32_t codepoint;
  MTSDFGlyph glyph;
} MTSDFGlyphEntry;

// Kerning pair
typedef struct {
  int unicode1;
  int unicode2;
  float advance;
} MTSDFKernPair;

// MTSDF Atlas structure (raw data only - no GL)
typedef struct {
  unsigned char *pixels; // Raw RGBA pixel data (freed after texture upload)
  int atlas_width;
  int atlas_height;
  float px_range;         // Distance field range in pixels
  float em_size;          // Em size used for generation
  float line_height;      // Line height in em units
  float ascender;         // Ascender in em units
  float descender;        // Descender in em units
  MTSDFGlyph glyphs[256]; // ASCII glyph lookup
  MTSDFGlyphEntry *glyph_entries;
  int glyph_count;
  int glyph_capacity;
  int *glyph_hash;
  int glyph_hash_capacity;
  MTSDFKernPair *kerning; // Dynamic kerning pairs array
  int kerning_count;
  float kerning_table[256][256];
  bool kerning_has[256][256];
  bool kerning_ready;
  bool loaded;
} MTSDFAtlas;

// Load MTSDF atlas from PNG and JSON files
// Returns true on success
bool mtsdf_load_atlas(MTSDFAtlas *atlas, const char *png_path,
                      const char *json_path);

// Free atlas resources
void mtsdf_free_atlas(MTSDFAtlas *atlas);

// Get kerning between two characters (in em units)
float mtsdf_get_kerning(const MTSDFAtlas *atlas, int char1, int char2);

// Get glyph by Unicode codepoint.
const MTSDFGlyph *mtsdf_get_glyph(const MTSDFAtlas *atlas, uint32_t codepoint);

#endif // STYGIAN_MTSDF_H
