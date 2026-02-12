# stygian.h
// ============================================================================
// STYGIAN - GPU-Resident UI Library
// MIT License | https://github.com/[user]/stygian
// ============================================================================
#ifndef STYGIAN_H
#define STYGIAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stygian_color.h"
#include "stygian_icc.h"
#include "stygian_unicode.h"

// Forward declaration for window abstraction
typedef struct StygianWindow StygianWindow;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

#ifndef STYGIAN_MAX_ELEMENTS
#define STYGIAN_MAX_ELEMENTS 16384
#endif

#ifndef STYGIAN_MAX_TEXTURES
#define STYGIAN_MAX_TEXTURES 256
#endif

#ifndef STYGIAN_MAX_CLIPS
#define STYGIAN_MAX_CLIPS 256
#endif

#ifndef STYGIAN_DEFAULT_TRIAD_DIR
#define STYGIAN_DEFAULT_TRIAD_DIR "assets/triad"
#endif

// ============================================================================
// Types
// ============================================================================

typedef uint32_t StygianElement; // Opaque element handle (0 = invalid)
typedef uint32_t StygianTexture; // Opaque texture handle (0 = invalid)
typedef uint32_t StygianFont;    // Opaque font handle (0 = invalid)
typedef uint64_t StygianScopeId; // Opaque retained scope id (0 = invalid)

// DDI: Overlay scope ID range (separate tick domain from base UI)
#define STYGIAN_OVERLAY_SCOPE_BASE 0xFFFF000000000000ULL
#define STYGIAN_OVERLAY_SCOPE_END 0xFFFFFFFFFFFFFFFFULL
#define STYGIAN_IS_OVERLAY_SCOPE(id) ((id) >= STYGIAN_OVERLAY_SCOPE_BASE)

typedef struct StygianContext StygianContext;
typedef struct StygianAllocator StygianAllocator;

typedef enum StygianBackendType {
  STYGIAN_BACKEND_OPENGL,
  STYGIAN_BACKEND_VULKAN,
  STYGIAN_BACKEND_DX12,
  STYGIAN_BACKEND_METAL,
} StygianBackendType;

typedef enum StygianType {
  STYGIAN_RECT = 0,
  STYGIAN_RECT_OUTLINE = 1,
  STYGIAN_CIRCLE = 2,
  STYGIAN_METABALL_LEFT = 3,
  STYGIAN_METABALL_RIGHT = 4,
  STYGIAN_WINDOW_BODY = 5,
  STYGIAN_TEXT = 6,
  STYGIAN_ICON_CLOSE = 7,
  STYGIAN_ICON_MAXIMIZE = 8,
  STYGIAN_ICON_MINIMIZE = 9,
  STYGIAN_TEXTURE = 10,
  STYGIAN_SEPARATOR = 11,
  STYGIAN_METABALL_GROUP = 12, // Dynamic SDF Blending Container
  STYGIAN_ICON_PLUS = 13,
  STYGIAN_ICON_CHEVRON = 14,
  STYGIAN_LINE =
      15, // SDF Line Segment (endpoints in UV, thickness in radius.x)
  STYGIAN_BEZIER = 16, // SDF Quadratic Bezier (control points in UV + reserved)
  STYGIAN_WIRE = 17,   // SDF Cubic Bezier (A, B, C, D in UV + reserved)
} StygianType;

typedef enum StygianGlyphFeatureFlags {
  STYGIAN_GLYPH_TRIAD_PRIMARY = 1u << 0,
  STYGIAN_GLYPH_TRIAD_FALLBACK_R8 = 1u << 1,
  STYGIAN_GLYPH_FALLBACK_MTSDF = 1u << 2,
  STYGIAN_GLYPH_PREDECODE_STARTUP = 1u << 3,
  STYGIAN_GLYPH_DECODE_ON_ZOOM = 1u << 4,
  STYGIAN_GLYPH_DECODE_ON_CACHE_MISS = 1u << 5,
  STYGIAN_GLYPH_CACHE_ENABLED = 1u << 6,
} StygianGlyphFeatureFlags;

#define STYGIAN_GLYPH_FEATURE_DEFAULT                                          \
  (STYGIAN_GLYPH_TRIAD_PRIMARY | STYGIAN_GLYPH_TRIAD_FALLBACK_R8 |             \
   STYGIAN_GLYPH_FALLBACK_MTSDF | STYGIAN_GLYPH_PREDECODE_STARTUP |            \
   STYGIAN_GLYPH_DECODE_ON_ZOOM | STYGIAN_GLYPH_DECODE_ON_CACHE_MISS |         \
   STYGIAN_GLYPH_CACHE_ENABLED)

#define STYGIAN_GLYPH_FEATURE_DGPU_INTERACTIVE STYGIAN_GLYPH_FEATURE_DEFAULT

#define STYGIAN_GLYPH_FEATURE_IGPU_BG_DECODE                                   \
  (STYGIAN_GLYPH_TRIAD_PRIMARY | STYGIAN_GLYPH_TRIAD_FALLBACK_R8 |             \
   STYGIAN_GLYPH_FALLBACK_MTSDF | STYGIAN_GLYPH_PREDECODE_STARTUP |            \
   STYGIAN_GLYPH_DECODE_ON_CACHE_MISS | STYGIAN_GLYPH_CACHE_ENABLED)

typedef enum StygianGlyphPath {
  STYGIAN_GLYPH_PATH_TRIAD_BC4 = 0,
  STYGIAN_GLYPH_PATH_TRIAD_R8 = 1,
  STYGIAN_GLYPH_PATH_MTSDF = 2,
  STYGIAN_GLYPH_PATH_DISABLED = 3,
} StygianGlyphPath;

typedef enum StygianGlyphProfile {
  STYGIAN_GLYPH_PROFILE_DEFAULT = 0,
  STYGIAN_GLYPH_PROFILE_DGPU_INTERACTIVE = 1,
  STYGIAN_GLYPH_PROFILE_IGPU_BG_DECODE = 2,
} StygianGlyphProfile;

typedef struct StygianTriadPackInfo {
  uint32_t version;
  uint32_t encoding;
  uint32_t tier;
  uint32_t entry_count;
  uint64_t data_offset;
} StygianTriadPackInfo;

typedef struct StygianTriadEntryInfo {
  uint64_t glyph_hash;
  uint64_t blob_hash;
  uint64_t payload_offset;
  uint32_t payload_size;
  uint32_t raw_blob_size;
  uint32_t glyph_len;
  uint32_t codec;
} StygianTriadEntryInfo;

// ... after other primitive functions ...
void stygian_wire(StygianContext *ctx, float x1, float y1, float cp1x,
                  float cp1y, float cp2x, float cp2y, float x2, float y2,
                  float thickness, float r, float g, float b, float a);

typedef struct StygianConfig {
  StygianBackendType backend;
  uint32_t max_elements;        // Default: STYGIAN_MAX_ELEMENTS
  uint32_t max_textures;        // Default: STYGIAN_MAX_TEXTURES
  uint32_t glyph_feature_flags; // Default: STYGIAN_GLYPH_FEATURE_DEFAULT
  StygianWindow *window;        // Required: window from stygian_window_create()
  const char *shader_dir;       // Optional: Override shader directory
  StygianAllocator *persistent_allocator; // Optional: defaults to CRT allocator
} StygianConfig;

// StygianGPUElement struct removed (AoS elimination)
// Data now stored in SoA structures (StygianSoAHot, StygianSoAAppearance, etc.)

// ============================================================================
// Context Lifecycle
// ============================================================================
// Wait for events (blocks until input arrives)
void stygian_wait_for_events(StygianContext *ctx);

// Main context functions
StygianContext *stygian_create(const StygianConfig *config);
void stygian_destroy(StygianContext *ctx);

// Runtime glyph policy
void stygian_set_glyph_feature_flags(StygianContext *ctx, uint32_t flags);
uint32_t stygian_get_glyph_feature_flags(const StygianContext *ctx);
bool stygian_glyph_feature_enabled(const StygianContext *ctx,
                                   uint32_t feature_flag);
void stygian_set_glyph_profile(StygianContext *ctx,
                               StygianGlyphProfile profile);
uint32_t stygian_glyph_profile_flags(StygianGlyphProfile profile);
StygianGlyphPath stygian_select_glyph_path(const StygianContext *ctx,
                                           bool triad_data_available,
                                           bool bc4_supported);
bool stygian_set_output_color_space(StygianContext *ctx,
                                    StygianColorSpace color_space);
bool stygian_set_output_icc_profile(StygianContext *ctx, const char *icc_path,
                                    StygianICCInfo *out_info);
bool stygian_get_output_color_profile(const StygianContext *ctx,
                                      StygianColorProfile *out_profile);
bool stygian_set_glyph_source_color_space(StygianContext *ctx,
                                          StygianColorSpace color_space);
bool stygian_get_glyph_source_color_profile(const StygianContext *ctx,
                                            StygianColorProfile *out_profile);
bool stygian_triad_mount(StygianContext *ctx, const char *triad_path);
void stygian_triad_unmount(StygianContext *ctx);
bool stygian_triad_is_mounted(const StygianContext *ctx);
bool stygian_triad_get_pack_info(const StygianContext *ctx,
                                 StygianTriadPackInfo *out_info);
bool stygian_triad_lookup(const StygianContext *ctx, uint64_t glyph_hash,
                          StygianTriadEntryInfo *out_entry);
uint64_t stygian_triad_hash_key(const char *glyph_id, const char *source_tag);
bool stygian_triad_lookup_glyph_id(const StygianContext *ctx,
                                   const char *glyph_id,
                                   StygianTriadEntryInfo *out_entry);
bool stygian_triad_read_svg_blob(const StygianContext *ctx, uint64_t glyph_hash,
                                 uint8_t **out_svg_data,
                                 uint32_t *out_svg_size);
bool stygian_triad_decode_rgba(const StygianContext *ctx, uint64_t glyph_hash,
                               uint8_t **out_rgba_data, uint32_t *out_width,
                               uint32_t *out_height);
void stygian_triad_free_blob(void *ptr);

// Debug
void stygian_debug_overlay_draw(StygianContext *ctx);

// Access the graphics access point (for hot-reload, etc)
typedef struct StygianAP StygianAP;
StygianAP *stygian_get_ap(StygianContext *ctx);

// ============================================================================
// Frame Management
// ============================================================================

typedef enum StygianRepaintReasonFlags {
  STYGIAN_REPAINT_REASON_NONE = 0u,
  STYGIAN_REPAINT_REASON_EVENT_MUTATION = 1u << 0,
  STYGIAN_REPAINT_REASON_TIMER = 1u << 1,
  STYGIAN_REPAINT_REASON_ANIMATION = 1u << 2,
  STYGIAN_REPAINT_REASON_ASYNC = 1u << 3,
  STYGIAN_REPAINT_REASON_FORCED = 1u << 4,
} StygianRepaintReasonFlags;

typedef enum StygianFrameIntent {
  STYGIAN_FRAME_RENDER = 0,
  STYGIAN_FRAME_EVAL_ONLY = 1,
} StygianFrameIntent;

void stygian_repaint_begin_frame(StygianContext *ctx);
void stygian_repaint_end_frame(StygianContext *ctx);
void stygian_request_repaint_hz(StygianContext *ctx, uint32_t hz);
void stygian_request_repaint_after_ms(StygianContext *ctx, uint32_t ms);
bool stygian_has_pending_repaint(const StygianContext *ctx);
uint32_t stygian_next_repaint_wait_ms(const StygianContext *ctx,
                                      uint32_t idle_wait_ms);
void stygian_set_repaint_source(StygianContext *ctx, const char *source);
const char *stygian_get_repaint_source(const StygianContext *ctx);
uint32_t stygian_get_repaint_reason_flags(const StygianContext *ctx);

void stygian_begin_frame(StygianContext *ctx, int width, int height);
void stygian_begin_frame_intent(StygianContext *ctx, int width, int height,
                                StygianFrameIntent intent);
void stygian_end_frame(StygianContext *ctx); // Single draw call by default
bool stygian_is_eval_only_frame(const StygianContext *ctx);

// Frame stats
uint32_t stygian_get_frame_draw_calls(const StygianContext *ctx);
uint32_t stygian_get_last_frame_draw_calls(const StygianContext *ctx);
uint32_t stygian_get_last_frame_element_count(const StygianContext *ctx);
uint32_t stygian_get_last_frame_clip_count(const StygianContext *ctx);
uint32_t stygian_get_last_frame_upload_bytes(const StygianContext *ctx);
uint32_t stygian_get_last_frame_upload_ranges(const StygianContext *ctx);
uint32_t stygian_get_last_frame_scope_replay_hits(const StygianContext *ctx);
uint32_t stygian_get_last_frame_scope_replay_misses(const StygianContext *ctx);
uint32_t
stygian_get_last_frame_scope_forced_rebuilds(const StygianContext *ctx);
float stygian_get_last_frame_build_ms(const StygianContext *ctx);
float stygian_get_last_frame_submit_ms(const StygianContext *ctx);
float stygian_get_last_frame_present_ms(const StygianContext *ctx);
float stygian_get_last_frame_gpu_ms(const StygianContext *ctx);
uint32_t stygian_get_last_frame_reason_flags(const StygianContext *ctx);
uint32_t stygian_get_last_frame_eval_only(const StygianContext *ctx);
uint32_t stygian_get_active_element_count(const StygianContext *ctx);
uint32_t stygian_get_element_capacity(const StygianContext *ctx);
uint32_t stygian_get_free_element_count(const StygianContext *ctx);
uint32_t stygian_get_font_count(const StygianContext *ctx);
uint32_t stygian_get_inline_emoji_cache_count(const StygianContext *ctx);
uint16_t stygian_get_clip_capacity(const StygianContext *ctx);

// Optional render layering (multi-pass draws)
// Use when a widget needs its own pass without breaking core UI batching.
void stygian_layer_begin(StygianContext *ctx);
void stygian_layer_end(StygianContext *ctx);

// Retained scope invalidation helpers (DDI).
void stygian_scope_begin(StygianContext *ctx, StygianScopeId id);
void stygian_scope_end(StygianContext *ctx);
void stygian_scope_invalidate(StygianContext *ctx, StygianScopeId id);
void stygian_scope_invalidate_now(StygianContext *ctx, StygianScopeId id);
void stygian_scope_invalidate_next(StygianContext *ctx, StygianScopeId id);
bool stygian_scope_is_dirty(StygianContext *ctx, StygianScopeId id);

// DDI: Overlay scope convenience APIs (separate tick domain from base UI)
void stygian_overlay_scope_begin(StygianContext *ctx, uint32_t overlay_id);
void stygian_overlay_scope_end(StygianContext *ctx);
void stygian_request_overlay_hz(StygianContext *ctx, uint32_t hz);
void stygian_invalidate_overlay_scopes(StygianContext *ctx);

// ============================================================================
// Elements (GPU-Resident)
// ============================================================================

StygianElement stygian_element(StygianContext *ctx);
StygianElement stygian_element_transient(StygianContext *ctx);
// Batch allocate N elements. Returns count actually allocated (≤ count).
// out_ids must point to at least count StygianElement slots.
uint32_t stygian_element_batch(StygianContext *ctx, uint32_t count,
                               StygianElement *out_ids);
void stygian_element_free(StygianContext *ctx, StygianElement e);

// Core setters
void stygian_set_bounds(StygianContext *ctx, StygianElement e, float x, float y,
                        float w, float h);
void stygian_set_color(StygianContext *ctx, StygianElement e, float r, float g,
                       float b, float a);
void stygian_set_border(StygianContext *ctx, StygianElement e, float r, float g,
                        float b, float a);
void stygian_set_radius(StygianContext *ctx, StygianElement e, float tl,
                        float tr, float br, float bl);
void stygian_set_type(StygianContext *ctx, StygianElement e, StygianType type);
void stygian_set_visible(StygianContext *ctx, StygianElement e, bool visible);
void stygian_set_z(StygianContext *ctx, StygianElement e, float z);

// Texture
void stygian_set_texture(StygianContext *ctx, StygianElement e,
                         StygianTexture tex, float u0, float v0, float u1,
                         float v1);

// Shadow
void stygian_set_shadow(StygianContext *ctx, StygianElement e, float offset_x,
                        float offset_y, float blur, float spread, float r,
                        float g, float b, float a);

// Gradient
void stygian_set_gradient(StygianContext *ctx, StygianElement e, float angle,
                          float r1, float g1, float b1, float a1, float r2,
                          float g2, float b2, float a2);

// Effects
void stygian_set_hover(StygianContext *ctx, StygianElement e, float hover);
void stygian_set_blend(StygianContext *ctx, StygianElement e, float blend);
void stygian_set_blur(StygianContext *ctx, StygianElement e, float radius);
void stygian_set_glow(StygianContext *ctx, StygianElement e, float intensity);

// Clipping
void stygian_set_clip(StygianContext *ctx, StygianElement e, uint8_t clip_id);
uint8_t stygian_clip_push(StygianContext *ctx, float x, float y, float w,
                          float h);
void stygian_clip_pop(StygianContext *ctx);

// ============================================================================
// Textures
// ============================================================================

StygianTexture stygian_texture_create(StygianContext *ctx, int w, int h,
                                      const void *rgba);
bool stygian_texture_update(StygianContext *ctx, StygianTexture tex, int x,
                            int y, int w, int h, const void *rgba);
void stygian_texture_destroy(StygianContext *ctx, StygianTexture tex);

// ============================================================================
// Text (MTSDF)
// ============================================================================

StygianFont stygian_font_load(StygianContext *ctx, const char *atlas_png,
                              const char *atlas_json);
void stygian_font_destroy(StygianContext *ctx, StygianFont font);

// Returns first element ID of the text run
StygianElement stygian_text(StygianContext *ctx, StygianFont font,
                            const char *str, float x, float y, float size,
                            float r, float g, float b, float a);

float stygian_text_width(StygianContext *ctx, StygianFont font, const char *str,
                         float size);

// ============================================================================
// Convenience (Immediate-style, uses internal transient pool)
// ============================================================================

StygianElement stygian_rect(StygianContext *ctx, float x, float y, float w,
                            float h, float r, float g, float b, float a);

void stygian_rect_rounded(StygianContext *ctx, float x, float y, float w,
                          float h, float r, float g, float b, float a,
                          float radius);

// SDF line segment from (x1,y1) to (x2,y2)
void stygian_line(StygianContext *ctx, float x1, float y1, float x2, float y2,
                  float thickness, float r, float g, float b, float a);

void stygian_image(StygianContext *ctx, StygianTexture tex, float x, float y,
                   float w, float h);
void stygian_image_uv(StygianContext *ctx, StygianTexture tex, float x, float y,
                      float w, float h, float u0, float v0, float u1, float v1);

// ============================================================================
// Utilities
// ============================================================================

void stygian_get_size(StygianContext *ctx, int *w, int *h);
void stygian_set_vsync(StygianContext *ctx, bool enable);

// Metaball Groups
StygianElement stygian_begin_metaball_group(StygianContext *ctx);
void stygian_end_metaball_group(StygianContext *ctx, StygianElement group);

// Get the associated window
StygianWindow *stygian_get_window(StygianContext *ctx);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_H


# stygian_clipboard.h
#ifndef STYGIAN_CLIPBOARD_H
#define STYGIAN_CLIPBOARD_H

#include "../include/stygian.h"

#ifdef __cplusplus
extern "C" {
#endif

// Advanced Clipboard API

typedef enum {
    STYGIAN_CLIP_TEXT,
    STYGIAN_CLIP_FILE_REF, // Just a file path
    STYGIAN_CLIP_OBJECT    // Complex object
} StygianClipType;

// Push content to Universal Clipboard (OS + History)
// text: The raw text representation
// metadata: Optional JSON metadata or file path if type is FILE_REF
void stygian_clipboard_push(StygianContext *ctx, const char *text, const char *metadata);

// Pop content from Universal Clipboard
// Returns heap string (must be freed)
char *stygian_clipboard_pop(StygianContext *ctx);

// Get internal history count
int stygian_clipboard_history_count(StygianContext *ctx);

// Get history item (read-only)
const char *stygian_clipboard_history_get(StygianContext *ctx, int index);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_CLIPBOARD_H


# stygian_color.h
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


# stygian_error.h
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


# stygian_icc.h
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


# stygian_memory.h
// stygian_memory.h - Memory Management System for Stygian
// Part of Phase 5.5 - Advanced Features

#ifndef STYGIAN_MEMORY_H
#define STYGIAN_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Allocator Interface
// ============================================================================

typedef struct StygianAllocator StygianAllocator;

typedef void *(*StygianAllocFn)(StygianAllocator *allocator, size_t size,
                                size_t alignment);
typedef void (*StygianFreeFn)(StygianAllocator *allocator, void *ptr);
typedef void (*StygianResetFn)(StygianAllocator *allocator);

struct StygianAllocator {
  StygianAllocFn alloc;
  StygianFreeFn free;
  StygianResetFn reset;
  void *user_data;
};

// ============================================================================
// Arena Allocator (Per-Frame Reset)
// ============================================================================

typedef struct StygianArena {
  StygianAllocator base;
  uint8_t *buffer;
  size_t capacity;
  size_t offset;
  bool owns_memory;
} StygianArena;

// Create arena with internal allocation
StygianArena *stygian_arena_create(size_t capacity);

// Create arena with external buffer
StygianArena *stygian_arena_create_from_buffer(void *buffer, size_t capacity);

void stygian_arena_destroy(StygianArena *arena);
void stygian_arena_reset(StygianArena *arena);
void *stygian_arena_alloc(StygianArena *arena, size_t size, size_t alignment);

// ============================================================================
// Pool Allocator (Fixed-Size Blocks)
// ============================================================================

typedef struct StygianPoolBlock {
  struct StygianPoolBlock *next;
} StygianPoolBlock;

typedef struct StygianPool {
  StygianAllocator base;
  uint8_t *buffer;
  size_t capacity;
  size_t block_size;
  StygianPoolBlock *free_list;
  bool owns_memory;
} StygianPool;

StygianPool *stygian_pool_create(size_t block_size, size_t block_count);
StygianPool *stygian_pool_create_from_buffer(void *buffer, size_t capacity,
                                             size_t block_size);
void stygian_pool_destroy(StygianPool *pool);
void stygian_pool_reset(StygianPool *pool);
void *stygian_pool_alloc(StygianPool *pool);
void stygian_pool_free(StygianPool *pool, void *ptr);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_MEMORY_H


# stygian_unicode.h
// stygian_unicode.h - UTF-8, grapheme, and emoji shortcode helpers
#ifndef STYGIAN_UNICODE_H
#define STYGIAN_UNICODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct StygianGraphemeSpan {
  size_t byte_start;
  size_t byte_len;
  uint32_t first_codepoint;
  uint32_t flags;
} StygianGraphemeSpan;

enum {
  STYGIAN_GRAPHEME_HAS_ZWJ = 1u << 0,
  STYGIAN_GRAPHEME_HAS_VARIATION = 1u << 1,
  STYGIAN_GRAPHEME_HAS_SKIN_TONE = 1u << 2,
  STYGIAN_GRAPHEME_IS_REGIONAL_PAIR = 1u << 3,
};

// Decode next UTF-8 codepoint from text[*io_index...].
// Returns false only when end of buffer is reached.
// Invalid sequences advance by one byte and return U+FFFD.
bool stygian_utf8_next(const char *text, size_t text_len, size_t *io_index,
                       uint32_t *out_codepoint);

// Iterate one grapheme cluster-like span using a pragmatic rule set suitable
// for emoji + combining mark text flows.
bool stygian_grapheme_next(const char *text, size_t text_len, size_t *io_index,
                           StygianGraphemeSpan *out_span);

// Normalize user shortcode forms to canonical "emoji_u..." ids.
// Accepts forms like:
//   :emoji_u1f600:
//   emoji_u1f600
//   U+1F600
//   1f468-200d-1f4bb
bool stygian_shortcode_normalize(const char *input, char *out, size_t out_size);

#endif // STYGIAN_UNICODE_H


# stygian_input.h
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


# stygian_window.h
// stygian_window.h - Platform-Agnostic Window API
// Part of Stygian UI Library
#ifndef STYGIAN_WINDOW_H
#define STYGIAN_WINDOW_H

#include "stygian_input.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Window Handle
// ============================================================================

typedef struct StygianWindow StygianWindow;

// ============================================================================
// Window Configuration
// ============================================================================

typedef enum StygianWindowFlags {
  STYGIAN_WINDOW_NONE = 0,
  STYGIAN_WINDOW_RESIZABLE = (1 << 0),
  STYGIAN_WINDOW_BORDERLESS = (1 << 1),
  STYGIAN_WINDOW_TRANSPARENT = (1 << 2), // Per-pixel alpha
  STYGIAN_WINDOW_ALWAYS_ON_TOP = (1 << 3),
  STYGIAN_WINDOW_MAXIMIZED = (1 << 4),
  STYGIAN_WINDOW_CENTERED = (1 << 5),
  STYGIAN_WINDOW_OPENGL = (1 << 6), // Create GL context
  STYGIAN_WINDOW_VULKAN = (1 << 7), // Prepare for Vulkan
} StygianWindowFlags;

typedef enum StygianWindowRole {
  STYGIAN_ROLE_MAIN = 0,    // Standard app window
  STYGIAN_ROLE_TOOL = 1,    // Floating tool palette (no taskbar)
  STYGIAN_ROLE_POPUP = 2,   // Transient popup (menu, combo)
  STYGIAN_ROLE_TOOLTIP = 3, // Tooltip / Overlay (no focus)
} StygianWindowRole;

typedef struct StygianWindowConfig {
  int width, height;
  const char *title;
  uint32_t flags;         // StygianWindowFlags
  StygianWindowRole role; // Semantic role
  int gl_major, gl_minor; // OpenGL version (if OPENGL flag)
} StygianWindowConfig;

// ============================================================================
// Window Lifecycle
// ============================================================================

// Create window with config
StygianWindow *stygian_window_create(const StygianWindowConfig *config);

// Create window with defaults (800x600, resizable, OpenGL 4.3)
StygianWindow *stygian_window_create_simple(int w, int h, const char *title);

// Wrap an existing native window handle (HWND, NSWindow*, etc.)
// Use this when the application manages its own window but needs Stygian
// integration The wrapped window is NOT destroyed by stygian_window_destroy()
StygianWindow *stygian_window_from_native(void *native_handle);

// Pixel density (DPI scale)
float stygian_window_get_scale(StygianWindow *win);

// Convert screen coordinates to client coordinates
void stygian_window_screen_to_client(StygianWindow *win, int screen_x,
                                     int screen_y, int *client_x,
                                     int *client_y);

// Destroy window and free resources
// Note: Windows created via stygian_window_from_native() are NOT destroyed
void stygian_window_destroy(StygianWindow *win);

// ============================================================================
// Window State
// ============================================================================

bool stygian_window_should_close(StygianWindow *win);
void stygian_window_request_close(StygianWindow *win);

void stygian_window_get_size(StygianWindow *win, int *w, int *h);
void stygian_window_set_size(StygianWindow *win, int w, int h);

void stygian_window_get_position(StygianWindow *win, int *x, int *y);
void stygian_window_set_position(StygianWindow *win, int x, int y);

void stygian_window_set_title(StygianWindow *win, const char *title);

void stygian_window_minimize(StygianWindow *win);
void stygian_window_maximize(StygianWindow *win);
void stygian_window_restore(StygianWindow *win);
bool stygian_window_is_maximized(StygianWindow *win);
bool stygian_window_is_minimized(StygianWindow *win);

void stygian_window_focus(StygianWindow *win);
bool stygian_window_is_focused(StygianWindow *win);

// ============================================================================
// Event Processing
// ============================================================================

// Poll events (non-blocking)
// Returns true if event was retrieved
bool stygian_window_poll_event(StygianWindow *win, StygianEvent *event);

// Wait for event (blocking)
void stygian_window_wait_event(StygianWindow *win, StygianEvent *event);

// Wait for event up to timeout (milliseconds). Returns true if an event
// was received, false on timeout.
bool stygian_window_wait_event_timeout(StygianWindow *win, StygianEvent *event,
                                       uint32_t timeout_ms);

// Process all pending events (call callbacks)
void stygian_window_process_events(StygianWindow *win);

// ============================================================================
// OpenGL Context
// ============================================================================

void stygian_window_make_current(StygianWindow *win);
void stygian_window_swap_buffers(StygianWindow *win);
void stygian_window_set_vsync(StygianWindow *win, bool enabled);
void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx);
void stygian_window_gl_destroy_context(void *ctx);
bool stygian_window_gl_make_current(StygianWindow *win, void *ctx);
void stygian_window_gl_swap_buffers(StygianWindow *win);
void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled);
bool stygian_window_gl_set_pixel_format(StygianWindow *win);
void *stygian_window_gl_get_proc_address(const char *name);

// ============================================================================
// Vulkan Surface
// ============================================================================

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts);
bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **vk_surface);

// ============================================================================
// Native Handle (Platform-Specific)
// ============================================================================

// Returns platform-specific handle:
// - Windows: HWND
// - Linux/X11: Window (X11 window ID)
// - macOS: NSWindow*
void *stygian_window_native_handle(StygianWindow *win);

// Returns platform-specific graphics context:
// - Windows: HDC (for OpenGL) or HWND (for Vulkan)
// - Linux/X11: Display* (for OpenGL/Vulkan)
// - macOS: CAMetalLayer* (for Metal/Vulkan)
void *stygian_window_native_context(StygianWindow *win);

// ============================================================================
// Cursor
// ============================================================================

typedef enum StygianCursor {
  STYGIAN_CURSOR_ARROW = 0,
  STYGIAN_CURSOR_IBEAM,
  STYGIAN_CURSOR_CROSSHAIR,
  STYGIAN_CURSOR_HAND,
  STYGIAN_CURSOR_RESIZE_H,
  STYGIAN_CURSOR_RESIZE_V,
  STYGIAN_CURSOR_RESIZE_NWSE,
  STYGIAN_CURSOR_RESIZE_NESW,
  STYGIAN_CURSOR_RESIZE_ALL,
  STYGIAN_CURSOR_NOT_ALLOWED,
} StygianCursor;

void stygian_window_set_cursor(StygianWindow *win, StygianCursor cursor);
void stygian_window_hide_cursor(StygianWindow *win);
void stygian_window_show_cursor(StygianWindow *win);

// ============================================================================
// High-DPI Support
// ============================================================================

float stygian_window_get_dpi_scale(StygianWindow *win);
void stygian_window_get_framebuffer_size(StygianWindow *win, int *w, int *h);

// Clipboard API
void stygian_clipboard_write(StygianWindow *win, const char *text);
char *stygian_clipboard_read(
    StygianWindow *win); // Returns heap-allocated string (caller must free)

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_WINDOW_H


# stygian_cocoa_shim.h
// stygian_cocoa_shim.h - macOS Cocoa shim (C ABI)
#pragma once

#if defined(__APPLE__)

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool stygian_cocoa_gl_set_pixel_format(void *win);
void *stygian_cocoa_gl_create_context(void *win, void *share_ctx);
void stygian_cocoa_gl_destroy_context(void *ctx);
bool stygian_cocoa_gl_make_current(void *win, void *ctx);
void stygian_cocoa_gl_swap_buffers(void *win);
void stygian_cocoa_gl_set_vsync(void *win, bool enabled);
void *stygian_cocoa_gl_get_proc_address(const char *name);

uint32_t stygian_cocoa_vk_get_instance_extensions(const char **out_exts,
                                                  uint32_t max_exts);
bool stygian_cocoa_vk_create_surface(void *win, void *vk_instance,
                                     void **out_surface);

#ifdef __cplusplus
}
#endif

#endif // __APPLE__


# stygian_wayland.c
// stygian_wayland.c - Linux/Wayland Platform Implementation (STUB)
// Part of Stygian UI Library
// TODO: Implement when Wayland support is needed

#if defined(__linux__) && !defined(__ANDROID__) && defined(STYGIAN_USE_WAYLAND)

#include "../stygian_window.h"
#include <stdio.h>
#include <stdlib.h>

// Stub implementations - will be filled in when Wayland support is added

struct StygianWindow {
  int width, height;
  bool should_close;
  // TODO: Wayland handles (wl_display, wl_surface, etc.)
};

StygianWindow *stygian_window_create(const StygianWindowConfig *config) {
  fprintf(stderr, "[Stygian] Linux/Wayland support not yet implemented\n");
  (void)config;
  return NULL;
}

StygianWindow *stygian_window_create_simple(int w, int h, const char *title) {
  fprintf(stderr, "[Stygian] Linux/Wayland support not yet implemented\n");
  (void)w;
  (void)h;
  (void)title;
  return NULL;
}

void stygian_window_destroy(StygianWindow *win) { free(win); }
bool stygian_window_should_close(StygianWindow *win) {
  return win ? win->should_close : true;
}
void stygian_window_request_close(StygianWindow *win) {
  if (win)
    win->should_close = true;
}
void stygian_window_get_size(StygianWindow *win, int *w, int *h) {
  if (win) {
    if (w)
      *w = win->width;
    if (h)
      *h = win->height;
  }
}
void stygian_window_set_size(StygianWindow *win, int w, int h) {
  (void)win;
  (void)w;
  (void)h;
}
void stygian_window_get_position(StygianWindow *win, int *x, int *y) {
  (void)win;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}
void stygian_window_set_position(StygianWindow *win, int x, int y) {
  (void)win;
  (void)x;
  (void)y;
}
void stygian_window_set_title(StygianWindow *win, const char *title) {
  (void)win;
  (void)title;
}
void stygian_window_minimize(StygianWindow *win) { (void)win; }
void stygian_window_maximize(StygianWindow *win) { (void)win; }
void stygian_window_restore(StygianWindow *win) { (void)win; }
bool stygian_window_is_maximized(StygianWindow *win) {
  (void)win;
  return false;
}
bool stygian_window_is_minimized(StygianWindow *win) {
  (void)win;
  return false;
}
void stygian_window_focus(StygianWindow *win) { (void)win; }
bool stygian_window_is_focused(StygianWindow *win) {
  (void)win;
  return false;
}
bool stygian_window_poll_event(StygianWindow *win, StygianEvent *event) {
  (void)win;
  event->type = STYGIAN_EVENT_NONE;
  return false;
}
void stygian_window_wait_event(StygianWindow *win, StygianEvent *event) {
  (void)win;
  event->type = STYGIAN_EVENT_NONE;
}
bool stygian_window_wait_event_timeout(StygianWindow *win, StygianEvent *event,
                                       uint32_t timeout_ms) {
  (void)win;
  (void)timeout_ms;
  if (event)
    event->type = STYGIAN_EVENT_NONE;
  return false;
}
void stygian_window_process_events(StygianWindow *win) { (void)win; }
void stygian_window_make_current(StygianWindow *win) { (void)win; }
void stygian_window_swap_buffers(StygianWindow *win) { (void)win; }
void stygian_window_set_vsync(StygianWindow *win, bool enabled) {
  (void)win;
  (void)enabled;
}
void *stygian_window_native_handle(StygianWindow *win) {
  (void)win;
  return NULL;
}
void *stygian_window_native_context(StygianWindow *win) {
  (void)win;
  return NULL;
}
void stygian_window_set_cursor(StygianWindow *win, StygianCursor cursor) {
  (void)win;
  (void)cursor;
}
void stygian_window_hide_cursor(StygianWindow *win) { (void)win; }
void stygian_window_show_cursor(StygianWindow *win) { (void)win; }
float stygian_window_get_dpi_scale(StygianWindow *win) {
  (void)win;
  return 1.0f;
}
void stygian_window_get_framebuffer_size(StygianWindow *win, int *w, int *h) {
  stygian_window_get_size(win, w, h);
}
bool stygian_key_down(StygianWindow *win, StygianKey key) {
  (void)win;
  (void)key;
  return false;
}
bool stygian_mouse_down(StygianWindow *win, StygianMouseButton button) {
  (void)win;
  (void)button;
  return false;
}
void stygian_mouse_pos(StygianWindow *win, int *x, int *y) {
  (void)win;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}
uint32_t stygian_get_mods(StygianWindow *win) {
  (void)win;
  return 0;
}

// ============================================================================
// OpenGL Hooks (stub)
// ============================================================================

bool stygian_window_gl_set_pixel_format(StygianWindow *win) {
  (void)win;
  return false;
}

void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx) {
  (void)win;
  (void)share_ctx;
  return NULL;
}

void stygian_window_gl_destroy_context(void *ctx) { (void)ctx; }

bool stygian_window_gl_make_current(StygianWindow *win, void *ctx) {
  (void)win;
  (void)ctx;
  return false;
}

void stygian_window_gl_swap_buffers(StygianWindow *win) { (void)win; }

void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled) {
  (void)win;
  (void)enabled;
}

void *stygian_window_gl_get_proc_address(const char *name) {
  (void)name;
  return NULL;
}

// ============================================================================
// Vulkan Hooks (stub)
// ============================================================================

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts) {
  (void)out_exts;
  (void)max_exts;
  return 0;
}

bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **out_surface) {
  (void)win;
  (void)vk_instance;
  (void)out_surface;
  return false;
}

#endif // __linux__ && STYGIAN_USE_WAYLAND


# stygian_win32.c
// stygian_win32.c - Win32 Platform Implementation
// Part of Stygian UI Library
// GRAPHICS-AGNOSTIC: Only handles window, input, events
// OpenGL/Vulkan context creation is handled by backends/
#ifdef _WIN32

#include "../stygian_window.h"
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <windowsx.h>
#ifdef STYGIAN_VULKAN
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#endif

// DWM constants
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// ============================================================================
// Window Structure (NO graphics context - that's backend's job)
// ============================================================================

struct StygianWindow {
  HWND hwnd;
  HDC hdc; // Device context for graphics backends to use

  int width, height;
  bool should_close;
  bool focused;
  bool maximized;
  bool minimized;
  bool external_owned; // True if wrapped via stygian_window_from_native()

  // Event queue (ring buffer)
  StygianEvent events[256];
  int event_head;
  int event_tail;

  // Input state
  bool keys[STYGIAN_KEY_COUNT];
  bool mouse_buttons[5];
  int mouse_x, mouse_y;
  uint32_t mods;

  // Config
  uint32_t flags;
  bool gl_pixel_format_set;
  bool in_size_move;
  UINT_PTR live_tick_timer_id;
  uint32_t live_tick_hz;
  bool nc_drag_active;
  int nc_drag_hit;
  POINT nc_drag_start_cursor;
  RECT nc_drag_start_rect;
};

static const UINT_PTR STYGIAN_WIN32_LIVE_TICK_TIMER_ID = 0x51A7u;
static const uint32_t STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ = 30u;
static const int STYGIAN_WIN32_MIN_TRACK_W = 320;
static const int STYGIAN_WIN32_MIN_TRACK_H = 200;
static void push_event(StygianWindow *win, StygianEvent *e);

static void stygian_win32_start_live_ticks(StygianWindow *win, uint32_t hz) {
  UINT interval_ms;
  if (!win || !win->hwnd)
    return;
  if (hz == 0u)
    hz = STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ;
  interval_ms = (UINT)(1000u / hz);
  if (interval_ms < 1u)
    interval_ms = 1u;
  SetTimer(win->hwnd, STYGIAN_WIN32_LIVE_TICK_TIMER_ID, interval_ms, NULL);
  win->live_tick_timer_id = STYGIAN_WIN32_LIVE_TICK_TIMER_ID;
}

static void stygian_win32_stop_live_ticks(StygianWindow *win) {
  if (!win || !win->hwnd)
    return;
  if (win->live_tick_timer_id != 0u) {
    KillTimer(win->hwnd, win->live_tick_timer_id);
    win->live_tick_timer_id = 0u;
  }
}

static bool stygian_win32_is_resize_hit(int hit) {
  switch (hit) {
  case HTLEFT:
  case HTRIGHT:
  case HTTOP:
  case HTBOTTOM:
  case HTTOPLEFT:
  case HTTOPRIGHT:
  case HTBOTTOMLEFT:
  case HTBOTTOMRIGHT:
    return true;
  default:
    return false;
  }
}

static bool stygian_win32_is_move_or_resize_hit(int hit) {
  return (hit == HTCAPTION) || stygian_win32_is_resize_hit(hit);
}

static void stygian_win32_end_nc_drag(StygianWindow *win) {
  StygianEvent e = {0};
  if (!win || !win->nc_drag_active)
    return;
  win->nc_drag_active = false;
  win->nc_drag_hit = 0;
  win->in_size_move = false;
  stygian_win32_stop_live_ticks(win);
  ReleaseCapture();
  e.type = STYGIAN_EVENT_TICK;
  push_event(win, &e);
}

static void stygian_win32_apply_nc_drag(StygianWindow *win) {
  POINT cursor;
  RECT next_rect;
  LONG dx;
  LONG dy;
  LONG min_w = STYGIAN_WIN32_MIN_TRACK_W;
  LONG min_h = STYGIAN_WIN32_MIN_TRACK_H;
  if (!win || !win->nc_drag_active)
    return;
  if (!GetCursorPos(&cursor))
    return;

  dx = cursor.x - win->nc_drag_start_cursor.x;
  dy = cursor.y - win->nc_drag_start_cursor.y;
  next_rect = win->nc_drag_start_rect;

  if (win->nc_drag_hit == HTCAPTION) {
    OffsetRect(&next_rect, dx, dy);
  } else {
    switch (win->nc_drag_hit) {
    case HTLEFT:
      next_rect.left += dx;
      break;
    case HTRIGHT:
      next_rect.right += dx;
      break;
    case HTTOP:
      next_rect.top += dy;
      break;
    case HTBOTTOM:
      next_rect.bottom += dy;
      break;
    case HTTOPLEFT:
      next_rect.left += dx;
      next_rect.top += dy;
      break;
    case HTTOPRIGHT:
      next_rect.right += dx;
      next_rect.top += dy;
      break;
    case HTBOTTOMLEFT:
      next_rect.left += dx;
      next_rect.bottom += dy;
      break;
    case HTBOTTOMRIGHT:
      next_rect.right += dx;
      next_rect.bottom += dy;
      break;
    default:
      break;
    }

    if (next_rect.right - next_rect.left < min_w) {
      if (win->nc_drag_hit == HTLEFT || win->nc_drag_hit == HTTOPLEFT ||
          win->nc_drag_hit == HTBOTTOMLEFT) {
        next_rect.left = next_rect.right - min_w;
      } else {
        next_rect.right = next_rect.left + min_w;
      }
    }
    if (next_rect.bottom - next_rect.top < min_h) {
      if (win->nc_drag_hit == HTTOP || win->nc_drag_hit == HTTOPLEFT ||
          win->nc_drag_hit == HTTOPRIGHT) {
        next_rect.top = next_rect.bottom - min_h;
      } else {
        next_rect.bottom = next_rect.top + min_h;
      }
    }
  }

  SetWindowPos(win->hwnd, NULL, next_rect.left, next_rect.top,
               next_rect.right - next_rect.left,
               next_rect.bottom - next_rect.top,
               SWP_NOACTIVATE | SWP_NOZORDER);
}

static bool stygian_win32_use_dwm_flush(void) {
  static int initialized = 0;
  static bool enabled = false;
  if (!initialized) {
    const char *env = getenv("STYGIAN_GL_DWM_FLUSH");
    enabled = (env && env[0] && env[0] != '0');
    initialized = 1;
  }
  return enabled;
}

// ============================================================================
// Key Translation
// ============================================================================

static StygianKey translate_key(WPARAM vk) {
  if (vk >= 'A' && vk <= 'Z')
    return STYGIAN_KEY_A + (vk - 'A');
  if (vk >= '0' && vk <= '9')
    return STYGIAN_KEY_0 + (vk - '0');
  if (vk >= VK_F1 && vk <= VK_F12)
    return STYGIAN_KEY_F1 + (vk - VK_F1);

  switch (vk) {
  case VK_SHIFT:
    return STYGIAN_KEY_SHIFT;
  case VK_CONTROL:
    return STYGIAN_KEY_CTRL;
  case VK_MENU:
    return STYGIAN_KEY_ALT;
  case VK_LWIN:
  case VK_RWIN:
    return STYGIAN_KEY_SUPER;
  case VK_UP:
    return STYGIAN_KEY_UP;
  case VK_DOWN:
    return STYGIAN_KEY_DOWN;
  case VK_LEFT:
    return STYGIAN_KEY_LEFT;
  case VK_RIGHT:
    return STYGIAN_KEY_RIGHT;
  case VK_HOME:
    return STYGIAN_KEY_HOME;
  case VK_END:
    return STYGIAN_KEY_END;
  case VK_PRIOR:
    return STYGIAN_KEY_PAGE_UP;
  case VK_NEXT:
    return STYGIAN_KEY_PAGE_DOWN;
  case VK_INSERT:
    return STYGIAN_KEY_INSERT;
  case VK_DELETE:
    return STYGIAN_KEY_DELETE;
  case VK_ESCAPE:
    return STYGIAN_KEY_ESCAPE;
  case VK_RETURN:
    return STYGIAN_KEY_ENTER;
  case VK_TAB:
    return STYGIAN_KEY_TAB;
  case VK_BACK:
    return STYGIAN_KEY_BACKSPACE;
  case VK_SPACE:
    return STYGIAN_KEY_SPACE;
  default:
    return STYGIAN_KEY_UNKNOWN;
  }
}

static uint32_t get_mods(void) {
  uint32_t mods = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000)
    mods |= STYGIAN_MOD_SHIFT;
  if (GetKeyState(VK_CONTROL) & 0x8000)
    mods |= STYGIAN_MOD_CTRL;
  if (GetKeyState(VK_MENU) & 0x8000)
    mods |= STYGIAN_MOD_ALT;
  if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
    mods |= STYGIAN_MOD_SUPER;
  return mods;
}

// ============================================================================
// Event Queue
// ============================================================================

static void push_event(StygianWindow *win, StygianEvent *e) {
  int prev;
  int next = (win->event_head + 1) % 256;

  // Coalesce high-rate move/resize events so the app sees the latest state
  // without processing every intermediate OS message.
  if (win->event_head != win->event_tail) {
    prev = (win->event_head - 1 + 256) % 256;
    if ((e->type == STYGIAN_EVENT_MOUSE_MOVE &&
         win->events[prev].type == STYGIAN_EVENT_MOUSE_MOVE) ||
        (e->type == STYGIAN_EVENT_RESIZE &&
         win->events[prev].type == STYGIAN_EVENT_RESIZE) ||
        (e->type == STYGIAN_EVENT_SCROLL &&
         win->events[prev].type == STYGIAN_EVENT_SCROLL) ||
        (e->type == STYGIAN_EVENT_TICK &&
         win->events[prev].type == STYGIAN_EVENT_TICK)) {
      if (e->type == STYGIAN_EVENT_SCROLL &&
          win->events[prev].type == STYGIAN_EVENT_SCROLL) {
        // Merge wheel bursts into one event to avoid queue tail-lag.
        win->events[prev].scroll.dx += e->scroll.dx;
        win->events[prev].scroll.dy += e->scroll.dy;
        win->events[prev].scroll.x = e->scroll.x;
        win->events[prev].scroll.y = e->scroll.y;
      } else {
        win->events[prev] = *e;
      }
      return;
    }
  }

  if (next != win->event_tail) {
    win->events[win->event_head] = *e;
    win->event_head = next;
  }
}

// ============================================================================
// Window Procedure
// ============================================================================

static LRESULT CALLBACK win32_wndproc(HWND hwnd, UINT msg, WPARAM wp,
                                      LPARAM lp) {
  StygianWindow *win = (StygianWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (!win)
    return DefWindowProc(hwnd, msg, wp, lp);

  StygianEvent e = {0};

  switch (msg) {
  case WM_CLOSE:
    e.type = STYGIAN_EVENT_CLOSE;
    push_event(win, &e);
    win->should_close = true;
    return 0;

  case WM_SIZE:
    win->width = LOWORD(lp);
    win->height = HIWORD(lp);
    win->maximized = (wp == SIZE_MAXIMIZED);
    win->minimized = (wp == SIZE_MINIMIZED);
    e.type = STYGIAN_EVENT_RESIZE;
    e.resize.width = win->width;
    e.resize.height = win->height;
    push_event(win, &e);
    return 0;

  case WM_ENTERSIZEMOVE:
    return 0;

  case WM_EXITSIZEMOVE:
    return 0;

  case WM_MOVING:
  case WM_SIZING:
    return 0;

  case WM_SETFOCUS:
    win->focused = true;
    e.type = STYGIAN_EVENT_FOCUS;
    push_event(win, &e);
    return 0;

  case WM_KILLFOCUS:
    win->focused = false;
    e.type = STYGIAN_EVENT_BLUR;
    push_event(win, &e);
    return 0;

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    e.type = STYGIAN_EVENT_KEY_DOWN;
    e.key.key = translate_key(wp);
    e.key.mods = get_mods();
    e.key.repeat = (lp & 0x40000000) != 0;
    win->keys[e.key.key] = true;
    win->mods = e.key.mods;
    push_event(win, &e);
    return 0;

  case WM_KEYUP:
  case WM_SYSKEYUP:
    e.type = STYGIAN_EVENT_KEY_UP;
    e.key.key = translate_key(wp);
    e.key.mods = get_mods();
    e.key.repeat = false;
    win->keys[e.key.key] = false;
    win->mods = e.key.mods;
    push_event(win, &e);
    return 0;

  case WM_CHAR:
    if (wp >= 32) {
      e.type = STYGIAN_EVENT_CHAR;
      e.chr.codepoint = (uint32_t)wp;
      push_event(win, &e);
    }
    return 0;

  case WM_MOUSEMOVE:
    e.type = STYGIAN_EVENT_MOUSE_MOVE;
    e.mouse_move.x = GET_X_LPARAM(lp);
    e.mouse_move.y = GET_Y_LPARAM(lp);
    e.mouse_move.dx = e.mouse_move.x - win->mouse_x;
    e.mouse_move.dy = e.mouse_move.y - win->mouse_y;
    win->mouse_x = e.mouse_move.x;
    win->mouse_y = e.mouse_move.y;
    push_event(win, &e);
    return 0;

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
    e.type = STYGIAN_EVENT_MOUSE_DOWN;
    e.mouse_button.x = GET_X_LPARAM(lp);
    e.mouse_button.y = GET_Y_LPARAM(lp);
    e.mouse_button.button = (msg == WM_LBUTTONDOWN)   ? STYGIAN_MOUSE_LEFT
                            : (msg == WM_RBUTTONDOWN) ? STYGIAN_MOUSE_RIGHT
                                                      : STYGIAN_MOUSE_MIDDLE;
    e.mouse_button.mods = get_mods();
    e.mouse_button.clicks = 1;
    win->mouse_buttons[e.mouse_button.button] = true;
    push_event(win, &e);
    SetCapture(hwnd);
    return 0;

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
    e.type = STYGIAN_EVENT_MOUSE_UP;
    e.mouse_button.x = GET_X_LPARAM(lp);
    e.mouse_button.y = GET_Y_LPARAM(lp);
    e.mouse_button.button = (msg == WM_LBUTTONUP)   ? STYGIAN_MOUSE_LEFT
                            : (msg == WM_RBUTTONUP) ? STYGIAN_MOUSE_RIGHT
                                                    : STYGIAN_MOUSE_MIDDLE;
    e.mouse_button.mods = get_mods();
    win->mouse_buttons[e.mouse_button.button] = false;
    push_event(win, &e);
    if (win->nc_drag_active) {
      stygian_win32_end_nc_drag(win);
    } else {
      ReleaseCapture();
    }
    return 0;

  case WM_NCLBUTTONDOWN: {
    int hit = (int)wp;
    if (stygian_win32_is_move_or_resize_hit(hit)) {
      win->nc_drag_active = true;
      win->nc_drag_hit = hit;
      win->in_size_move = true;
      stygian_win32_start_live_ticks(win, win->live_tick_hz);
      GetCursorPos(&win->nc_drag_start_cursor);
      GetWindowRect(hwnd, &win->nc_drag_start_rect);
      SetCapture(hwnd);
      e.type = STYGIAN_EVENT_TICK;
      push_event(win, &e);
      return 0;
    }
  } break;

  case WM_NCLBUTTONUP:
    stygian_win32_end_nc_drag(win);
    return 0;

  case WM_CAPTURECHANGED:
    stygian_win32_end_nc_drag(win);
    return 0;

  case WM_MOUSEWHEEL:
    e.type = STYGIAN_EVENT_SCROLL;
    e.scroll.x = GET_X_LPARAM(lp);
    e.scroll.y = GET_Y_LPARAM(lp);
    e.scroll.dx = 0;
    e.scroll.dy = (float)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
    push_event(win, &e);
    return 0;

  case WM_TIMER:
    if (wp == STYGIAN_WIN32_LIVE_TICK_TIMER_ID &&
        (win->in_size_move || win->nc_drag_active)) {
      if (win->nc_drag_active) {
        stygian_win32_apply_nc_drag(win);
      }
      e.type = STYGIAN_EVENT_TICK;
      push_event(win, &e);
      return 0;
    }
    break;
  }

  return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
// Window Creation (NO graphics context - that's backend's job)
// ============================================================================

static const char *WIN_CLASS = "StygianWindowClass";
static bool class_registered = false;

StygianWindow *stygian_window_create(const StygianWindowConfig *config) {
  // Register window class
  if (!class_registered) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = win32_wndproc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WIN_CLASS;
    RegisterClassEx(&wc);
    class_registered = true;
  }

  // Allocate window
  StygianWindow *win = (StygianWindow *)calloc(1, sizeof(StygianWindow));
  if (!win)
    return NULL;

  win->width = config->width;
  win->height = config->height;
  win->flags = config->flags;
  win->focused = true;
  win->live_tick_hz = STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ;

  // Window style
  DWORD style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = WS_EX_APPWINDOW;

  // Apply Role Styles
  switch (config->role) {
  case STYGIAN_ROLE_MAIN:
    ex_style = WS_EX_APPWINDOW;
    break;
  case STYGIAN_ROLE_TOOL:
    ex_style = WS_EX_TOOLWINDOW;
    style = WS_OVERLAPPEDWINDOW; // Or WS_POPUP | WS_CAPTION?
    break;
  case STYGIAN_ROLE_POPUP:
    ex_style = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    style = WS_POPUP | WS_BORDER;
    break;
  case STYGIAN_ROLE_TOOLTIP:
    ex_style =
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    style = WS_POPUP;
    break;
  }

  if (config->flags & STYGIAN_WINDOW_BORDERLESS) {
    style = WS_POPUP;
  }
  if (!(config->flags & STYGIAN_WINDOW_RESIZABLE)) {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  if (config->flags & STYGIAN_WINDOW_ALWAYS_ON_TOP) {
    ex_style |= WS_EX_TOPMOST;
  }

  // Adjust window size for borders
  RECT rc = {0, 0, config->width, config->height};
  AdjustWindowRectEx(&rc, style, FALSE, ex_style);
  int adj_w = rc.right - rc.left;
  int adj_h = rc.bottom - rc.top;

  // Position
  int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
  if (config->flags & STYGIAN_WINDOW_CENTERED) {
    x = (GetSystemMetrics(SM_CXSCREEN) - adj_w) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - adj_h) / 2;
  }

  // Create window
  win->hwnd =
      CreateWindowEx(ex_style, WIN_CLASS, config->title, style, x, y, adj_w,
                     adj_h, NULL, NULL, GetModuleHandle(NULL), NULL);
  if (!win->hwnd) {
    free(win);
    return NULL;
  }

  SetWindowLongPtr(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);

  // Dark mode
  BOOL dark = TRUE;
  DwmSetWindowAttribute(win->hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                        sizeof(dark));

  // Get device context for graphics backends
  win->hdc = GetDC(win->hwnd);

  // If OpenGL window, set pixel format early
  if (win->flags & STYGIAN_WINDOW_OPENGL) {
    if (!stygian_window_gl_set_pixel_format(win)) {
      printf("[Stygian Window] Failed to set OpenGL pixel format\n");
      DestroyWindow(win->hwnd);
      free(win);
      return NULL;
    }
  }

  // Show window
  ShowWindow(win->hwnd, (config->flags & STYGIAN_WINDOW_MAXIMIZED)
                            ? SW_SHOWMAXIMIZED
                            : SW_SHOW);
  UpdateWindow(win->hwnd);

  return win;
}

StygianWindow *stygian_window_create_simple(int w, int h, const char *title) {
  StygianWindowConfig cfg = {.width = w,
                             .height = h,
                             .title = title,
                             .flags = STYGIAN_WINDOW_RESIZABLE |
                                      STYGIAN_WINDOW_CENTERED,
                             .gl_major = 4,
                             .gl_minor = 3};
  return stygian_window_create(&cfg);
}

StygianWindow *stygian_window_from_native(void *native_handle) {
  if (!native_handle)
    return NULL;

  HWND hwnd = (HWND)native_handle;

  StygianWindow *win = (StygianWindow *)calloc(1, sizeof(StygianWindow));
  if (!win)
    return NULL;

  win->hwnd = hwnd;
  win->hdc = GetDC(hwnd);
  win->external_owned = true; // Don't destroy window on cleanup
  win->focused = true;

  // Get current size
  RECT rc;
  if (GetClientRect(hwnd, &rc)) {
    win->width = rc.right;
    win->height = rc.bottom;
  }

  return win;
}

void stygian_window_destroy(StygianWindow *win) {
  if (!win)
    return;

  stygian_win32_stop_live_ticks(win);

  // NOTE: Graphics context cleanup is backend's responsibility
  // We only handle window resources here

  if (win->hdc) {
    ReleaseDC(win->hwnd, win->hdc);
  }

  // Only destroy window if we own it
  if (!win->external_owned && win->hwnd) {
    DestroyWindow(win->hwnd);
  }

  free(win);
}

// ============================================================================
// Window State
// ============================================================================

bool stygian_window_should_close(StygianWindow *win) {
  return win ? win->should_close : true;
}

void stygian_window_request_close(StygianWindow *win) {
  if (win)
    win->should_close = true;
}

void stygian_window_get_size(StygianWindow *win, int *w, int *h) {
  if (win) {
    if (w)
      *w = win->width;
    if (h)
      *h = win->height;
  }
}

void stygian_window_set_size(StygianWindow *win, int w, int h) {
  if (win && win->hwnd) {
    SetWindowPos(win->hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
  }
}

void stygian_window_get_position(StygianWindow *win, int *x, int *y) {
  if (win && win->hwnd) {
    RECT rc;
    GetWindowRect(win->hwnd, &rc);
    if (x)
      *x = rc.left;
    if (y)
      *y = rc.top;
  }
}

void stygian_window_set_position(StygianWindow *win, int x, int y) {
  if (win && win->hwnd) {
    SetWindowPos(win->hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }
}

void stygian_window_set_title(StygianWindow *win, const char *title) {
  if (win && win->hwnd) {
    SetWindowText(win->hwnd, title);
  }
}

void stygian_window_minimize(StygianWindow *win) {
  if (win && win->hwnd)
    ShowWindow(win->hwnd, SW_MINIMIZE);
}

void stygian_window_maximize(StygianWindow *win) {
  if (win && win->hwnd)
    ShowWindow(win->hwnd, SW_MAXIMIZE);
}

void stygian_window_restore(StygianWindow *win) {
  if (win && win->hwnd)
    ShowWindow(win->hwnd, SW_RESTORE);
}

bool stygian_window_is_maximized(StygianWindow *win) {
  return win ? win->maximized : false;
}

bool stygian_window_is_minimized(StygianWindow *win) {
  return win ? win->minimized : false;
}

void stygian_window_focus(StygianWindow *win) {
  if (win && win->hwnd)
    SetForegroundWindow(win->hwnd);
}

bool stygian_window_is_focused(StygianWindow *win) {
  return win ? win->focused : false;
}

// ============================================================================
// Event Processing
// ============================================================================

bool stygian_window_poll_event(StygianWindow *win, StygianEvent *event) {
  if (!win)
    return false;

  // Process Windows messages first
  MSG msg;
  while (PeekMessage(&msg, win->hwnd, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Return queued event
  if (win->event_head != win->event_tail) {
    *event = win->events[win->event_tail];
    win->event_tail = (win->event_tail + 1) % 256;
    return true;
  }

  event->type = STYGIAN_EVENT_NONE;
  return false;
}

void stygian_window_wait_event(StygianWindow *win, StygianEvent *event) {
  if (!win || !win->hwnd)
    return;

  // If internal queue is empty, wait for OS message
  if (win->event_head == win->event_tail) {
    WaitMessage();
  }

  // Then process normally (poll will pick it up)
  stygian_window_poll_event(win, event);
}

bool stygian_window_wait_event_timeout(StygianWindow *win, StygianEvent *event,
                                       uint32_t timeout_ms) {
  DWORD wait_res;
  if (!win || !win->hwnd || !event)
    return false;

  if (win->in_size_move) {
    uint32_t hz = win->live_tick_hz ? win->live_tick_hz
                                    : STYGIAN_WIN32_DEFAULT_LIVE_TICK_HZ;
    uint32_t tick_ms = 1000u / hz;
    if (tick_ms < 1u)
      tick_ms = 1u;
    if (timeout_ms == 0u || timeout_ms > tick_ms)
      timeout_ms = tick_ms;
  }

  // Fast path: already queued.
  if (win->event_head != win->event_tail) {
    return stygian_window_poll_event(win, event);
  }

  wait_res = MsgWaitForMultipleObjectsEx(
      0, NULL, timeout_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
  if (wait_res == WAIT_TIMEOUT) {
    event->type = STYGIAN_EVENT_NONE;
    return false;
  }

  return stygian_window_poll_event(win, event);
}

void stygian_window_process_events(StygianWindow *win) {
  if (!win)
    return;

  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      win->should_close = true;
      return;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

// ============================================================================
// OpenGL Context (Win32 implementation)
// ============================================================================

typedef BOOL(WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int interval);
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;

bool stygian_window_gl_set_pixel_format(StygianWindow *win) {
  if (!win || !win->hdc)
    return false;
  if (win->gl_pixel_format_set)
    return true;

  PIXELFORMATDESCRIPTOR pfd = {
      .nSize = sizeof(pfd),
      .nVersion = 1,
      .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      .iPixelType = PFD_TYPE_RGBA,
      .cColorBits = 32,
      .cDepthBits = 24,
      .iLayerType = PFD_MAIN_PLANE,
  };

  int format = ChoosePixelFormat(win->hdc, &pfd);
  if (!format)
    return false;
  if (SetPixelFormat(win->hdc, format, &pfd) == TRUE) {
    win->gl_pixel_format_set = true;
    return true;
  }
  return false;
}

void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx) {
  if (!win || !win->hdc)
    return NULL;

  HGLRC ctx = wglCreateContext(win->hdc);
  if (!ctx)
    return NULL;

  if (share_ctx) {
    wglShareLists((HGLRC)share_ctx, ctx);
  }

  return (void *)ctx;
}

void stygian_window_gl_destroy_context(void *ctx) {
  if (ctx) {
    wglDeleteContext((HGLRC)ctx);
  }
}

bool stygian_window_gl_make_current(StygianWindow *win, void *ctx) {
  if (!win || !win->hdc || !ctx)
    return false;
  return wglMakeCurrent(win->hdc, (HGLRC)ctx) == TRUE;
}

void stygian_window_gl_swap_buffers(StygianWindow *win) {
  if (!win || !win->hdc)
    return;
  SwapBuffers(win->hdc);
  if (stygian_win32_use_dwm_flush()) {
    DwmFlush();
  }
}

void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled) {
  (void)win;

  if (!wglSwapIntervalEXT) {
    wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
  }

  if (wglSwapIntervalEXT) {
    wglSwapIntervalEXT(enabled ? 1 : 0);
  } else {
    HMODULE gl = GetModuleHandle("opengl32.dll");
    if (gl) {
      wglSwapIntervalEXT =
          (PFNWGLSWAPINTERVALEXTPROC)GetProcAddress(gl, "wglSwapIntervalEXT");
      if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(enabled ? 1 : 0);
    }
  }
}

void *stygian_window_gl_get_proc_address(const char *name) {
  void *p = (void *)wglGetProcAddress(name);
  if (!p) {
    HMODULE gl = GetModuleHandle("opengl32.dll");
    if (gl)
      p = (void *)GetProcAddress(gl, name);
  }
  return p;
}

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts) {
#ifdef STYGIAN_VULKAN
  static const char *exts[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                               VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
  uint32_t count = (uint32_t)(sizeof(exts) / sizeof(exts[0]));
  if (out_exts && max_exts >= count) {
    for (uint32_t i = 0; i < count; i++) {
      out_exts[i] = exts[i];
    }
  }
  return count;
#else
  (void)out_exts;
  (void)max_exts;
  return 0;
#endif
}

bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **vk_surface) {
#ifdef STYGIAN_VULKAN
  if (!win || !vk_instance || !vk_surface)
    return false;

  VkWin32SurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = GetModuleHandle(NULL),
      .hwnd = win->hwnd,
  };

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult res = vkCreateWin32SurfaceKHR((VkInstance)vk_instance, &info, NULL,
                                         &surface);
  if (res != VK_SUCCESS)
    return false;

  *vk_surface = (void *)surface;
  return true;
#else
  (void)win;
  (void)vk_instance;
  (void)vk_surface;
  return false;
#endif
}

void stygian_window_make_current(StygianWindow *win) {
  (void)win;
  // Deprecated: use stygian_window_gl_make_current
}

void stygian_window_swap_buffers(StygianWindow *win) {
  (void)win;
  // Swap is handled by backend (stygian_ap_swap).
}

void stygian_window_set_vsync(StygianWindow *win, bool enabled) {
  stygian_window_gl_set_vsync(win, enabled);
}

// ============================================================================
// Native Handle (for backends to use)
// ============================================================================

void *stygian_window_native_handle(StygianWindow *win) {
  return win ? win->hwnd : NULL;
}

void *stygian_window_native_context(StygianWindow *win) {
  return win ? win->hdc : NULL;
}

// ============================================================================
// Cursor
// ============================================================================

void stygian_window_set_cursor(StygianWindow *win, StygianCursor cursor) {
  (void)win;
  LPCSTR cur = IDC_ARROW;
  switch (cursor) {
  case STYGIAN_CURSOR_IBEAM:
    cur = IDC_IBEAM;
    break;
  case STYGIAN_CURSOR_CROSSHAIR:
    cur = IDC_CROSS;
    break;
  case STYGIAN_CURSOR_HAND:
    cur = IDC_HAND;
    break;
  case STYGIAN_CURSOR_RESIZE_H:
    cur = IDC_SIZEWE;
    break;
  case STYGIAN_CURSOR_RESIZE_V:
    cur = IDC_SIZENS;
    break;
  case STYGIAN_CURSOR_RESIZE_NWSE:
    cur = IDC_SIZENWSE;
    break;
  case STYGIAN_CURSOR_RESIZE_NESW:
    cur = IDC_SIZENESW;
    break;
  case STYGIAN_CURSOR_RESIZE_ALL:
    cur = IDC_SIZEALL;
    break;
  case STYGIAN_CURSOR_NOT_ALLOWED:
    cur = IDC_NO;
    break;
  default:
    break;
  }
  SetCursor(LoadCursor(NULL, cur));
}

void stygian_window_hide_cursor(StygianWindow *win) {
  (void)win;
  ShowCursor(FALSE);
}

void stygian_window_show_cursor(StygianWindow *win) {
  (void)win;
  ShowCursor(TRUE);
}

// ============================================================================
// DPI
// ============================================================================

float stygian_window_get_dpi_scale(StygianWindow *win) {
  if (!win || !win->hdc)
    return 1.0f;
  return (float)GetDeviceCaps(win->hdc, LOGPIXELSX) / 96.0f;
}

void stygian_window_get_framebuffer_size(StygianWindow *win, int *w, int *h) {
  stygian_window_get_size(win, w, h);
}

float stygian_window_get_scale(StygianWindow *win) {
  return stygian_window_get_dpi_scale(win);
}

void stygian_window_screen_to_client(StygianWindow *win, int screen_x,
                                     int screen_y, int *client_x,
                                     int *client_y) {
  if (!win || !win->hwnd)
    return;
  POINT pt = {screen_x, screen_y};
  ScreenToClient(win->hwnd, &pt);
  if (client_x)
    *client_x = pt.x;
  if (client_y)
    *client_y = pt.y;
}

// ============================================================================
// Input State Query
// ============================================================================

bool stygian_key_down(StygianWindow *win, StygianKey key) {
  return (win && key < STYGIAN_KEY_COUNT) ? win->keys[key] : false;
}

bool stygian_mouse_down(StygianWindow *win, StygianMouseButton button) {
  return (win && button < 5) ? win->mouse_buttons[button] : false;
}

void stygian_mouse_pos(StygianWindow *win, int *x, int *y) {
  if (win) {
    if (x)
      *x = win->mouse_x;
    if (y)
      *y = win->mouse_y;
  }
}

uint32_t stygian_get_mods(StygianWindow *win) { return win ? win->mods : 0; }

// ============================================================================
// Clipboard Implementation
// ============================================================================

void stygian_clipboard_write(StygianWindow *win, const char *text) {
  if (!win || !win->hwnd || !text)
    return;

  if (!OpenClipboard(win->hwnd))
    return;
  EmptyClipboard();

  size_t len = strlen(text);
  HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, len + 1);
  if (!hGlob) {
    CloseClipboard();
    return;
  }

  char *p = (char *)GlobalLock(hGlob);
  memcpy(p, text, len + 1);
  GlobalUnlock(hGlob);

  SetClipboardData(CF_TEXT, hGlob);
  CloseClipboard();
}

char *stygian_clipboard_read(StygianWindow *win) {
  if (!win || !win->hwnd)
    return NULL;

  if (!OpenClipboard(win->hwnd))
    return NULL;

  HANDLE hData = GetClipboardData(CF_TEXT);
  if (!hData) {
    CloseClipboard();
    return NULL;
  }

  char *pszText = (char *)GlobalLock(hData);
  if (!pszText) {
    CloseClipboard();
    return NULL;
  }

  // Copy out
  char *result = _strdup(pszText);

  GlobalUnlock(hData);
  CloseClipboard();

  return result;
}

#endif // _WIN32


# stygian_x11.c
// stygian_x11.c - Linux/X11 Platform Implementation (STUB)
// Part of Stygian UI Library
// TODO: Implement when Linux support is needed

#if defined(__linux__) && !defined(__ANDROID__) && !defined(STYGIAN_USE_WAYLAND)

#include "../stygian_window.h"
#include <stdio.h>
#include <stdlib.h>


// Stub implementations - will be filled in when Linux support is added

struct StygianWindow {
  int width, height;
  bool should_close;
  // TODO: X11 handles (Display, Window, GLXContext)
};

StygianWindow *stygian_window_create(const StygianWindowConfig *config) {
  fprintf(stderr, "[Stygian] Linux/X11 support not yet implemented\n");
  (void)config;
  return NULL;
}

StygianWindow *stygian_window_create_simple(int w, int h, const char *title) {
  fprintf(stderr, "[Stygian] Linux/X11 support not yet implemented\n");
  (void)w;
  (void)h;
  (void)title;
  return NULL;
}

void stygian_window_destroy(StygianWindow *win) { free(win); }
bool stygian_window_should_close(StygianWindow *win) {
  return win ? win->should_close : true;
}
void stygian_window_request_close(StygianWindow *win) {
  if (win)
    win->should_close = true;
}
void stygian_window_get_size(StygianWindow *win, int *w, int *h) {
  if (win) {
    if (w)
      *w = win->width;
    if (h)
      *h = win->height;
  }
}
void stygian_window_set_size(StygianWindow *win, int w, int h) {
  (void)win;
  (void)w;
  (void)h;
}
void stygian_window_get_position(StygianWindow *win, int *x, int *y) {
  (void)win;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}
void stygian_window_set_position(StygianWindow *win, int x, int y) {
  (void)win;
  (void)x;
  (void)y;
}
void stygian_window_set_title(StygianWindow *win, const char *title) {
  (void)win;
  (void)title;
}
void stygian_window_minimize(StygianWindow *win) { (void)win; }
void stygian_window_maximize(StygianWindow *win) { (void)win; }
void stygian_window_restore(StygianWindow *win) { (void)win; }
bool stygian_window_is_maximized(StygianWindow *win) {
  (void)win;
  return false;
}
bool stygian_window_is_minimized(StygianWindow *win) {
  (void)win;
  return false;
}
void stygian_window_focus(StygianWindow *win) { (void)win; }
bool stygian_window_is_focused(StygianWindow *win) {
  (void)win;
  return false;
}
bool stygian_window_poll_event(StygianWindow *win, StygianEvent *event) {
  (void)win;
  event->type = STYGIAN_EVENT_NONE;
  return false;
}
void stygian_window_wait_event(StygianWindow *win, StygianEvent *event) {
  (void)win;
  event->type = STYGIAN_EVENT_NONE;
}
bool stygian_window_wait_event_timeout(StygianWindow *win, StygianEvent *event,
                                       uint32_t timeout_ms) {
  (void)win;
  (void)timeout_ms;
  if (event)
    event->type = STYGIAN_EVENT_NONE;
  return false;
}
void stygian_window_process_events(StygianWindow *win) { (void)win; }
void stygian_window_make_current(StygianWindow *win) { (void)win; }
void stygian_window_swap_buffers(StygianWindow *win) { (void)win; }
void stygian_window_set_vsync(StygianWindow *win, bool enabled) {
  (void)win;
  (void)enabled;
}
void *stygian_window_native_handle(StygianWindow *win) {
  (void)win;
  return NULL;
}
void *stygian_window_native_context(StygianWindow *win) {
  (void)win;
  return NULL;
}
void stygian_window_set_cursor(StygianWindow *win, StygianCursor cursor) {
  (void)win;
  (void)cursor;
}
void stygian_window_hide_cursor(StygianWindow *win) { (void)win; }
void stygian_window_show_cursor(StygianWindow *win) { (void)win; }
float stygian_window_get_dpi_scale(StygianWindow *win) {
  (void)win;
  return 1.0f;
}
void stygian_window_get_framebuffer_size(StygianWindow *win, int *w, int *h) {
  stygian_window_get_size(win, w, h);
}
bool stygian_key_down(StygianWindow *win, StygianKey key) {
  (void)win;
  (void)key;
  return false;
}
bool stygian_mouse_down(StygianWindow *win, StygianMouseButton button) {
  (void)win;
  (void)button;
  return false;
}
void stygian_mouse_pos(StygianWindow *win, int *x, int *y) {
  (void)win;
  if (x)
    *x = 0;
  if (y)
    *y = 0;
}
uint32_t stygian_get_mods(StygianWindow *win) {
  (void)win;
  return 0;
}

// ============================================================================
// OpenGL Hooks (stub)
// ============================================================================

bool stygian_window_gl_set_pixel_format(StygianWindow *win) {
  (void)win;
  return false;
}

void *stygian_window_gl_create_context(StygianWindow *win, void *share_ctx) {
  (void)win;
  (void)share_ctx;
  return NULL;
}

void stygian_window_gl_destroy_context(void *ctx) { (void)ctx; }

bool stygian_window_gl_make_current(StygianWindow *win, void *ctx) {
  (void)win;
  (void)ctx;
  return false;
}

void stygian_window_gl_swap_buffers(StygianWindow *win) { (void)win; }

void stygian_window_gl_set_vsync(StygianWindow *win, bool enabled) {
  (void)win;
  (void)enabled;
}

void *stygian_window_gl_get_proc_address(const char *name) {
  (void)name;
  return NULL;
}

// ============================================================================
// Vulkan Hooks (stub)
// ============================================================================

uint32_t stygian_window_vk_get_instance_extensions(const char **out_exts,
                                                   uint32_t max_exts) {
  (void)out_exts;
  (void)max_exts;
  return 0;
}

bool stygian_window_vk_create_surface(StygianWindow *win, void *vk_instance,
                                      void **out_surface) {
  (void)win;
  (void)vk_instance;
  (void)out_surface;
  return false;
}

#endif // __linux__ && !STYGIAN_USE_WAYLAND


# stb_include.h
// stb_include.h - v0.02 - parse and process #include directives - public domain
//
// To build this, in one source file that includes this file do
//      #define STB_INCLUDE_IMPLEMENTATION
//
// This program parses a string and replaces lines of the form
//         #include "foo"
// with the contents of a file named "foo". It also embeds the
// appropriate #line directives. Note that all include files must
// reside in the location specified in the path passed to the API;
// it does not check multiple directories.
//
// If the string contains a line of the form
//         #inject
// then it will be replaced with the contents of the string 'inject' passed to the API.
//
// Options:
//
//      Define STB_INCLUDE_LINE_GLSL to get GLSL-style #line directives
//      which use numbers instead of filenames.
//
//      Define STB_INCLUDE_LINE_NONE to disable output of #line directives.
//
// Standard libraries:
//
//      stdio.h     FILE, fopen, fclose, fseek, ftell
//      stdlib.h    malloc, realloc, free
//      string.h    strcpy, strncmp, memcpy
//
// Credits:
//
// Written by Sean Barrett.
//
// Fixes:
//  Michal Klos

#ifndef STB_INCLUDE_STB_INCLUDE_H
#define STB_INCLUDE_STB_INCLUDE_H

// Do include-processing on the string 'str'. To free the return value, pass it to free()
char *stb_include_string(char *str, char *inject, char *path_to_includes, char *filename_for_line_directive, char error[256]);

// Concatenate the strings 'strs' and do include-processing on the result. To free the return value, pass it to free()
char *stb_include_strings(char **strs, int count, char *inject, char *path_to_includes, char *filename_for_line_directive, char error[256]);

// Load the file 'filename' and do include-processing on the string therein. note that
// 'filename' is opened directly; 'path_to_includes' is not used. To free the return value, pass it to free()
char *stb_include_file(char *filename, char *inject, char *path_to_includes, char error[256]);

#endif


#ifdef STB_INCLUDE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *stb_include_load_file(char *filename, size_t *plen)
{
   char *text;
   size_t len;
   FILE *f = fopen(filename, "rb");
   if (f == 0) return 0;
   fseek(f, 0, SEEK_END);
   len = (size_t) ftell(f);
   if (plen) *plen = len;
   text = (char *) malloc(len+1);
   if (text == 0) return 0;
   fseek(f, 0, SEEK_SET);
   fread(text, 1, len, f);
   fclose(f);
   text[len] = 0;
   return text;
}

typedef struct
{
   int offset;
   int end;
   char *filename;
   int next_line_after;
} include_info;

static include_info *stb_include_append_include(include_info *array, int len, int offset, int end, char *filename, int next_line)
{
   include_info *z = (include_info *) realloc(array, sizeof(*z) * (len+1));
   z[len].offset   = offset;
   z[len].end      = end;
   z[len].filename = filename;
   z[len].next_line_after = next_line;
   return z;
}

static void stb_include_free_includes(include_info *array, int len)
{
   int i;
   for (i=0; i < len; ++i)
      free(array[i].filename);
   free(array);
}

static int stb_include_isspace(int ch)
{
   return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

// find location of all #include and #inject
static int stb_include_find_includes(char *text, include_info **plist)
{
   int line_count = 1;
   int inc_count = 0;
   char *s = text, *start;
   include_info *list = NULL;
   while (*s) {
      // parse is always at start of line when we reach here
      start = s;
      while (*s == ' ' || *s == '\t')
         ++s;
      if (*s == '#') {
         ++s;
         while (*s == ' ' || *s == '\t')
            ++s;
         if (0==strncmp(s, "include", 7) && stb_include_isspace(s[7])) {
            s += 7;
            while (*s == ' ' || *s == '\t')
               ++s;
            if (*s == '"') {
               char *t = ++s;
               while (*t != '"' && *t != '\n' && *t != '\r' && *t != 0)
                  ++t;
               if (*t == '"') {
                  char *filename = (char *) malloc(t-s+1);
                  memcpy(filename, s, t-s);
                  filename[t-s] = 0;
                  s=t;
                  while (*s != '\r' && *s != '\n' && *s != 0)
                     ++s;
                  // s points to the newline, so s-start is everything except the newline
                  list = stb_include_append_include(list, inc_count++, start-text, s-text, filename, line_count+1);
               }
            }
         } else if (0==strncmp(s, "inject", 6) && (stb_include_isspace(s[6]) || s[6]==0)) {
            while (*s != '\r' && *s != '\n' && *s != 0)
               ++s;
            list = stb_include_append_include(list, inc_count++, start-text, s-text, NULL, line_count+1);
         }
      }
      while (*s != '\r' && *s != '\n' && *s != 0)
         ++s;
      if (*s == '\r' || *s == '\n') {
         s = s + (s[0] + s[1] == '\r' + '\n' ? 2 : 1);
      }
      ++line_count;
   }
   *plist = list;
   return inc_count;
}

// avoid dependency on sprintf()
static void stb_include_itoa(char str[9], int n)
{
   int i;
   for (i=0; i < 8; ++i)
      str[i] = ' ';
   str[i] = 0;

   for (i=1; i < 8; ++i) {
      str[7-i] = '0' + (n % 10);
      n /= 10;
      if (n == 0)
         break;
   }
}

static char *stb_include_append(char *str, size_t *curlen, char *addstr, size_t addlen)
{
   str = (char *) realloc(str, *curlen + addlen);
   memcpy(str + *curlen, addstr, addlen);
   *curlen += addlen;
   return str;
}

char *stb_include_string(char *str, char *inject, char *path_to_includes, char *filename, char error[256])
{
   char temp[4096];
   include_info *inc_list;
   int i, num = stb_include_find_includes(str, &inc_list);
   size_t source_len = strlen(str);
   char *text=0;
   size_t textlen=0, last=0;
   for (i=0; i < num; ++i) {
      text = stb_include_append(text, &textlen, str+last, inc_list[i].offset - last);
      // write out line directive for the include
      #ifndef STB_INCLUDE_LINE_NONE
      #ifdef STB_INCLUDE_LINE_GLSL
      if (textlen != 0)  // GLSL #version must appear first, so don't put a #line at the top
      #endif
      {
         strcpy(temp, "#line ");
         stb_include_itoa(temp+6, 1);
         strcat(temp, " ");
         #ifdef STB_INCLUDE_LINE_GLSL
         stb_include_itoa(temp+15, i+1);
         #else
         strcat(temp, "\"");
         if (inc_list[i].filename == 0)
            strcmp(temp, "INJECT");
         else
            strcat(temp, inc_list[i].filename);
         strcat(temp, "\"");
         #endif
         strcat(temp, "\n");
         text = stb_include_append(text, &textlen, temp, strlen(temp));
      }
      #endif
      if (inc_list[i].filename == 0) {
         if (inject != 0)
            text = stb_include_append(text, &textlen, inject, strlen(inject));
      } else {
         char *inc;
         strcpy(temp, path_to_includes);
         strcat(temp, "/");
         strcat(temp, inc_list[i].filename);
         inc = stb_include_file(temp, inject, path_to_includes, error);
         if (inc == NULL) {
            stb_include_free_includes(inc_list, num);
            return NULL;
         }
         text = stb_include_append(text, &textlen, inc, strlen(inc));
         free(inc);
      }
      // write out line directive
      #ifndef STB_INCLUDE_LINE_NONE
      strcpy(temp, "\n#line ");
      stb_include_itoa(temp+6, inc_list[i].next_line_after);
      strcat(temp, " ");
      #ifdef STB_INCLUDE_LINE_GLSL
      stb_include_itoa(temp+15, 0);
      #else
      strcat(temp, filename != 0 ? filename : "source-file");
      #endif
      text = stb_include_append(text, &textlen, temp, strlen(temp));
      // no newlines, because we kept the #include newlines, which will get appended next
      #endif
      last = inc_list[i].end;
   }
   text = stb_include_append(text, &textlen, str+last, source_len - last + 1); // append '\0'
   stb_include_free_includes(inc_list, num);
   return text;
}

char *stb_include_strings(char **strs, int count, char *inject, char *path_to_includes, char *filename, char error[256])
{
   char *text;
   char *result;
   int i;
   size_t length=0;
   for (i=0; i < count; ++i)
      length += strlen(strs[i]);
   text = (char *) malloc(length+1);
   length = 0;
   for (i=0; i < count; ++i) {
      strcpy(text + length, strs[i]);
      length += strlen(strs[i]);
   }
   result = stb_include_string(text, inject, path_to_includes, filename, error);
   free(text);
   return result;
}

char *stb_include_file(char *filename, char *inject, char *path_to_includes, char error[256])
{
   size_t len;
   char *result;
   char *text = stb_include_load_file(filename, &len);
   if (text == NULL) {
      strcpy(error, "Error: couldn't load '");
      strcat(error, filename);
      strcat(error, "'");
      return 0;
   }
   result = stb_include_string(text, inject, path_to_includes, filename, error);
   free(text);
   return result;
}

#if 0 // @TODO, GL_ARB_shader_language_include-style system that doesn't touch filesystem
char *stb_include_preloaded(char *str, char *inject, char *includes[][2], char error[256])
{

}
#endif

#endif // STB_INCLUDE_IMPLEMENTATION


# sdf_common.glsl
// sdf_common.glsl - Shared SDF primitives for Stygian UI
// Include this in stygian.frag

float sdRoundedBox(vec2 p, vec2 b, vec4 r) {
    // b is half-size. p is relative to center.
    // r order: tl, tr, br, bl
    // With Top-Down p: p.y < 0 is TOP, p.y > 0 is BOTTOM
    r.xy = (p.x > 0.0) ? r.yz : r.xw; // x > 0: tr/br, x < 0: tl/bl
    r.x  = (p.y > 0.0) ? r.y  : r.x;  // y > 0: br/bl, y < 0: tr/tl
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// Helper for squared length
float dot2(vec2 v) { return dot(v, v); }

// Metaball smooth union (for future use)
float smoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

// ============================================================================
// Advanced SDF Primitives (Phase 5.5)
// ============================================================================

float sdTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

float sdEllipse(vec2 p, vec2 ab) {
    p = abs(p);
    if (p.x > p.y) { p = p.yx; ab = ab.yx; }
    float l = ab.y * ab.y - ab.x * ab.x;
    float m = ab.x * p.x / l;
    float m2 = m * m;
    float n = ab.y * p.y / l;
    float n2 = n * n;
    float c = (m2 + n2 - 1.0) / 3.0;
    float c3 = c * c * c;
    float q = c3 + m2 * n2 * 2.0;
    float d = c3 + m2 * n2;
    float g = m + m * n2;
    float co;
    if (d < 0.0) {
        float h = acos(q / c3) / 3.0;
        float s = cos(h);
        float t = sin(h) * sqrt(3.0);
        float rx = sqrt(-c * (s + t + 2.0) + m2);
        float ry = sqrt(-c * (s - t + 2.0) + m2);
        co = (ry + sign(l) * rx + abs(g) / (rx * ry) - m) / 2.0;
    } else {
        float h = 2.0 * m * n * sqrt(d);
        float s = sign(q + h) * pow(abs(q + h), 1.0 / 3.0);
        float u = sign(q - h) * pow(abs(q - h), 1.0 / 3.0);
        float rx = -s - u - c * 4.0 + 2.0 * m2;
        float ry = (s - u) * sqrt(3.0);
        float rm = sqrt(rx * rx + ry * ry);
        co = (ry / sqrt(rm - rx) + 2.0 * g / rm - m) / 2.0;
    }
    vec2 r = ab * vec2(co, sqrt(1.0 - co * co));
    return length(r - p) * sign(p.y - r.y);
}

float sdArc(vec2 p, vec2 sc, float ra, float rb) {
    // sc is the sin/cos of the arc's aperture
    p.x = abs(p.x);
    return ((sc.y * p.x > sc.x * p.y) ? length(p - sc * ra) : 
            abs(length(p) - ra)) - rb;
}

// Quadratic Bezier
float sdBezier(vec2 pos, vec2 A, vec2 B, vec2 C) {    
    vec2 a = B - A;
    vec2 b = A - 2.0 * B + C;
    vec2 c = a * 2.0;
    vec2 d = A - pos;
    float kk = 1.0 / dot(b, b);
    float kx = kk * dot(a, b);
    float ky = kk * (2.0 * dot(a, a) + dot(d, b)) / 3.0;
    float kz = kk * dot(d, a);
    float res = 0.0;
    float p = ky - kx * kx;
    float p3 = p * p * p;
    float q = kx * (2.0 * kx * kx - 3.0 * ky) + kz;
    float h = q * q + 4.0 * p3;
    if (h >= 0.0) { 
        h = sqrt(h);
        vec2 x = (vec2(h, -h) - q) / 2.0;
        vec2 uv = sign(x) * pow(abs(x), vec2(1.0 / 3.0));
        float t = clamp(uv.x + uv.y - kx, 0.0, 1.0);
        res = dot2(d + (c + b * t) * t);
    } else {
        float z = sqrt(-p);
        float v = acos(q / (p * z * 2.0)) / 3.0;
        float m = cos(v);
        float n = sin(v) * 1.732050808;
        vec3 t = clamp(vec3(m + m, -n - m, n - m) * z - kx, 0.0, 1.0);
        res = min(dot2(d + (c + b * t.x) * t.x),
                  dot2(d + (c + b * t.y) * t.y));
    }
    return sqrt(res);
}

// Cubic Bezier approximation using two Quadratic Beziers split at t=0.5
float sdCubicBezierApprox(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3) {
    vec2 m = (p0 + 3.0*p1 + 3.0*p2 + p3) * 0.125;
    vec2 q1_ctrl = (3.0*p1 + p0) * 0.25;
    vec2 q2_ctrl = (3.0*p2 + p3) * 0.25;
    
    float d1 = sdBezier(p, p0, q1_ctrl, m);
    float d2 = sdBezier(p, m, q2_ctrl, p3);
    return min(d1, d2);
}


// Polygon (N-sided, convex)
float sdPolygon(vec2 p, vec2 v[8], int N) {
    float d = dot(p - v[0], p - v[0]);
    float s = 1.0;
    for (int i = 0, j = N - 1; i < N; j = i, i++) {
        vec2 e = v[j] - v[i];
        vec2 w = p - v[i];
        vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        d = min(d, dot(b, b));
        bvec3 c = bvec3(p.y >= v[i].y, p.y < v[j].y, e.x * w.y > e.y * w.x);
        if (all(c) || all(not(c))) s *= -1.0;
    }
    return s * sqrt(d);
}


# stygian.frag
#version 430 core

// Stygian Main Fragment Shader
// Includes all UI templates and dispatches by element type

// Varyings from vertex shader (must match vertex output locations)
layout(location = 0) flat in vec4 vColor;
layout(location = 1) flat in vec4 vBorderColor;
layout(location = 2) flat in vec4 vRadius;
layout(location = 3) flat in vec4 vUV;
layout(location = 4) flat in uint vType;
layout(location = 5) flat in float vBlend;
layout(location = 6) flat in float vHover;
layout(location = 7) in vec2 vLocalPos;
layout(location = 8) in vec2 vSize;
layout(location = 9) flat in uint vInstanceID;
layout(location = 10) flat in uint vTextureID;
layout(location = 11) flat in vec4 vReserved0; // control points for bezier/wire/metaball

layout(location = 0) out vec4 fragColor;

// Clip rect buffer (binding 3)
layout(std430, binding = 3) readonly buffer ClipBuffer {
    vec4 clip_rects[];
};

// === SoA SSBOs (sole data source) ===
struct SoAHot {
    float x, y, w, h;     // 16 - bounds
    vec4 color;            // 16 - primary RGBA
    uint texture_id;       //  4
    uint type;             //  4 - element type | (render_mode << 16)
    uint flags;            //  4
    float z;               //  4
};                         // 48 bytes

struct SoAAppearance {
    vec4 border_color;     // 16
    vec4 radius;           // 16 - corners (tl,tr,br,bl)
    vec4 uv;               // 16 - tex coords (u0,v0,u1,v1)
    vec4 control_points;   // 16 - bezier/wire/metaball control data
};                         // 64 bytes

struct SoAEffects {
    vec2 shadow_offset;    //  8
    float shadow_blur;     //  4
    float shadow_spread;   //  4
    vec4 shadow_color;     // 16
    vec4 gradient_start;   // 16
    vec4 gradient_end;     // 16
    float hover;           //  4
    float blend;           //  4
    float gradient_angle;  //  4
    float blur_radius;     //  4
    float glow_intensity;  //  4
    uint parent_id;        //  4
    vec2 _pad;             //  8
};                         // 96 bytes

layout(std430, binding = 4) readonly buffer SoAHotBuffer {
    SoAHot soa_hot[];
};

layout(std430, binding = 5) readonly buffer SoAAppearanceBuffer {
    SoAAppearance soa_appearance[];
};

layout(std430, binding = 6) readonly buffer SoAEffectsBuffer {
    SoAEffects soa_effects[];
};

#ifndef STYGIAN_GL
layout(push_constant) uniform PushConstants {
    vec4 uScreenAtlas;   // xy=screen size, zw=atlas size
    vec4 uPxRangeFlags;  // x=px range, y=enabled, z=src sRGB, w=dst sRGB
    vec4 uOutputRow0;    // xyz=row0
    vec4 uOutputRow1;    // xyz=row1
    vec4 uOutputRow2;    // xyz=row2
    vec4 uGamma;         // x=src gamma, y=dst gamma
} pc;
#endif

// Include shared SDF primitives
#include "sdf_common.glsl"

// Include template modules
#include "text.glsl"
#include "window.glsl"
#include "ui.glsl"

#ifdef STYGIAN_GL
uniform int uOutputColorTransformEnabled;
uniform mat3 uOutputColorMatrix;
uniform int uOutputSrcIsSRGB;
uniform float uOutputSrcGamma;
uniform int uOutputDstIsSRGB;
uniform float uOutputDstGamma;
#endif

float stygian_to_linear_channel(float c, bool srgb_transfer, float gamma_val) {
    if (srgb_transfer) {
        if (c <= 0.04045) {
            return c / 12.92;
        }
        return pow((c + 0.055) / 1.055, 2.4);
    }
    return pow(c, max(gamma_val, 0.0001));
}

float stygian_from_linear_channel(float c, bool srgb_transfer, float gamma_val) {
    if (srgb_transfer) {
        if (c <= 0.0031308) {
            return c * 12.92;
        }
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    }
    return pow(c, 1.0 / max(gamma_val, 0.0001));
}

vec4 apply_output_color_transform(vec4 color_in) {
#ifdef STYGIAN_GL
    if (uOutputColorTransformEnabled == 0) {
        return color_in;
    }
    vec3 c = clamp(color_in.rgb, 0.0, 1.0);
    vec3 linear_rgb = vec3(
        stygian_to_linear_channel(c.r, uOutputSrcIsSRGB != 0, uOutputSrcGamma),
        stygian_to_linear_channel(c.g, uOutputSrcIsSRGB != 0, uOutputSrcGamma),
        stygian_to_linear_channel(c.b, uOutputSrcIsSRGB != 0, uOutputSrcGamma)
    );
    vec3 output_linear = clamp(uOutputColorMatrix * linear_rgb, 0.0, 1.0);
    vec3 output_rgb = vec3(
        stygian_from_linear_channel(output_linear.r, uOutputDstIsSRGB != 0, uOutputDstGamma),
        stygian_from_linear_channel(output_linear.g, uOutputDstIsSRGB != 0, uOutputDstGamma),
        stygian_from_linear_channel(output_linear.b, uOutputDstIsSRGB != 0, uOutputDstGamma)
    );
    return vec4(clamp(output_rgb, 0.0, 1.0), color_in.a);
#else
    if (pc.uPxRangeFlags.y < 0.5) {
        return color_in;
    }
    vec3 c = clamp(color_in.rgb, 0.0, 1.0);
    vec3 linear_rgb = vec3(
        stygian_to_linear_channel(c.r, pc.uPxRangeFlags.z > 0.5, pc.uGamma.x),
        stygian_to_linear_channel(c.g, pc.uPxRangeFlags.z > 0.5, pc.uGamma.x),
        stygian_to_linear_channel(c.b, pc.uPxRangeFlags.z > 0.5, pc.uGamma.x)
    );
    mat3 output_m = mat3(
        pc.uOutputRow0.xyz,
        pc.uOutputRow1.xyz,
        pc.uOutputRow2.xyz
    );
    vec3 output_linear = clamp(output_m * linear_rgb, 0.0, 1.0);
    vec3 output_rgb = vec3(
        stygian_from_linear_channel(output_linear.r, pc.uPxRangeFlags.w > 0.5, pc.uGamma.y),
        stygian_from_linear_channel(output_linear.g, pc.uPxRangeFlags.w > 0.5, pc.uGamma.y),
        stygian_from_linear_channel(output_linear.b, pc.uPxRangeFlags.w > 0.5, pc.uGamma.y)
    );
    return vec4(clamp(output_rgb, 0.0, 1.0), color_in.a);
#endif
}

// Type 12: STYGIAN_METABALL_GROUP - Render children as dynamic SDF blob
float render_metaball_group(vec2 p, uint id, vec4 reserved0, float k) {
    SoAHot h = soa_hot[id];
    
    // Unpack start/count from reserved (passed via vReserved0)
    uint start = uint(reserved0.x);
    uint count = uint(reserved0.y);
    
    vec2 container_pos = vec2(h.x, h.y);
    vec2 container_size = vec2(h.w, h.h);
    vec2 container_center = container_pos + container_size * 0.5;
    
    vec2 frag_screen_pos = container_center + p;
    
    float d = 1000.0;
    
    for (uint i = 0; i < count; i++) {
        uint child_idx = start + i;
        SoAHot ch = soa_hot[child_idx];
        SoAAppearance ca = soa_appearance[child_idx];
        
        vec2 child_size = vec2(ch.w, ch.h);
        vec2 child_center = vec2(ch.x, ch.y) + child_size * 0.5;
        vec2 child_p = frag_screen_pos - child_center;
        
        float child_d = sdRoundedBox(child_p, child_size * 0.5, ca.radius);
        
        if (i == 0u) d = child_d;
        else d = smoothUnion(d, child_d, k);
    }
    
    return d;
}

void main() {
    vec2 center = vSize * 0.5;
    vec2 p = vLocalPos - center;
    vec4 r = vRadius;
    uint type = vType;

    // Clip test — read flags from SoA hot buffer
    SoAHot h_clip = soa_hot[vInstanceID];
    uint clip_id = (h_clip.flags & 0x0000FF00u) >> 8u;
    if (clip_id != 0u) {
        vec4 clip_rect = clip_rects[clip_id];
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        if (worldP.x < clip_rect.x || worldP.y < clip_rect.y ||
            worldP.x > clip_rect.x + clip_rect.z ||
            worldP.y > clip_rect.y + clip_rect.w) {
            discard;
        }
    }

    vec4 col = vColor;
    float d = 1000.0;
    float aa = 1.5;
    
    // Type dispatch - ALL in one shader, one draw call
    // Type 0: STYGIAN_RECT - Rounded rectangle
    if (type == 0u) {
        d = render_rect(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 1: STYGIAN_RECT_OUTLINE - Rounded rectangle outline
    else if (type == 1u) {
        d = render_rect_outline(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 2: STYGIAN_CIRCLE - Circle
    else if (type == 2u) {
        d = render_circle(p, center);
        aa = fwidth(d) * 1.5;
    }
    // Type 3: STYGIAN_METABALL_LEFT - Metaball menu (left anchor)
    else if (type == 3u) {
        d = render_metaball(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 4: STYGIAN_METABALL_RIGHT - Metaball menu (right anchor)
    else if (type == 4u) {
        d = render_metaball(p, center, r);
        aa = fwidth(d) * 1.5;
    }
    // Type 5: STYGIAN_WINDOW_BODY - Window body with gradient border
    else if (type == 5u) {
        d = render_window_body(p, center, r, vBorderColor, col);
        aa = fwidth(d) * 1.5;
    }
    // Type 6: STYGIAN_TEXT - MTSDF text
    else if (type == 6u) {
        fragColor = apply_output_color_transform(render_text(vLocalPos, vSize, vUV, col, vBlend));
        if (fragColor.a < 0.01) discard;
        return;
    }
    // Type 7: STYGIAN_ICON_CLOSE - Close icon (X)
    else if (type == 7u) {
        d = render_icon_close(p, center);
        aa = 1.5;
    }
    // Type 8: STYGIAN_ICON_MAXIMIZE - Maximize icon (square outline)
    else if (type == 8u) {
        d = render_icon_maximize(p, center);
        aa = 1.5;
    }
    // Type 9: STYGIAN_ICON_MINIMIZE - Minimize icon (horizontal line)
    else if (type == 9u) {
        d = render_icon_minimize(p, center);
        aa = 1.5;
    }
    // Type 10: STYGIAN_TEXTURE - Texture/image
    else if (type == 10u) {
        fragColor = apply_output_color_transform(render_texture(vLocalPos, vSize, vUV, col, vTextureID));
        return;
    }
    // Type 11: STYGIAN_SEPARATOR - Separator line
    else if (type == 11u) {
        d = render_separator(p);
        aa = 1.5;
    }
    // Type 12: STYGIAN_METABALL_GROUP - Dynamic SDF Blending
    else if (type == 12u) {
        // vBlend holds the smoothness factor (k)
        float k = vBlend; 
        if (k < 0.1) k = 10.0; // Default smooth if 0
        
        d = render_metaball_group(p, vInstanceID, vReserved0, k);
        aa = fwidth(d) * 1.5;
    }
    // Type 15: STYGIAN_LINE - SDF Line Segment
    else if (type == 15u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 a = vUV.xy;
        vec2 b = vUV.zw;
        float half_thick = vRadius.x;
        d = sdSegment(worldP, a, b) - half_thick;
        aa = fwidth(d) * 1.5;
    }
    // Type 16: STYGIAN_BEZIER - SDF Quadratic Bezier
    else if (type == 16u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 A = vUV.xy;
        vec2 C = vUV.zw;
        vec2 B = vReserved0.xy;
        float half_thick = vRadius.x;
        d = sdBezier(worldP, A, B, C) - half_thick;
        aa = fwidth(d) * 1.5;
    }
    // Type 17: STYGIAN_WIRE - SDF Cubic Bezier
    else if (type == 17u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 A = vUV.xy; 
        vec2 D = vUV.zw;
        vec2 B = vReserved0.xy;
        vec2 C = vReserved0.zw;
        float half_thick = vRadius.x;
        d = sdCubicBezierApprox(worldP, A, B, C, D) - half_thick;
        aa = fwidth(d) * 1.5;
    }
    else {
        // Fallback: simple box
        d = sdBox(p, center - 1.0);
        aa = 1.5;
    }
    
    // Hover effect
    if (vHover > 0.0) {
        col.rgb = mix(col.rgb, col.rgb * 1.3, vHover);
    }
    
    float alpha = 1.0 - smoothstep(-aa, aa, d);
    col.a *= alpha * vBlend;
    
    if (col.a < 0.01) discard;
    fragColor = apply_output_color_transform(col);
}


# stygian.vert
#version 430 core
layout(location = 0) in vec2 aPos;

// === SoA SSBOs (sole data source) ===
struct SoAHot {
    float x, y, w, h;     // 16 - bounds
    vec4 color;            // 16 - primary RGBA
    uint texture_id;       //  4
    uint type;             //  4 - element type | (render_mode << 16)
    uint flags;            //  4
    float z;               //  4
};                         // 48 bytes

struct SoAAppearance {
    vec4 border_color;     // 16
    vec4 radius;           // 16 - corners (tl,tr,br,bl)
    vec4 uv;               // 16 - tex coords (u0,v0,u1,v1)
    vec4 control_points;   // 16 - bezier/wire/metaball control data
};                         // 64 bytes

struct SoAEffects {
    vec2 shadow_offset;    //  8
    float shadow_blur;     //  4
    float shadow_spread;   //  4
    vec4 shadow_color;     // 16
    vec4 gradient_start;   // 16
    vec4 gradient_end;     // 16
    float hover;           //  4
    float blend;           //  4
    float gradient_angle;  //  4
    float blur_radius;     //  4
    float glow_intensity;  //  4
    uint parent_id;        //  4
    vec2 _pad;             //  8
};                         // 96 bytes

layout(std430, binding = 4) readonly buffer SoAHotBuffer {
    SoAHot soa_hot[];
};

layout(std430, binding = 5) readonly buffer SoAAppearanceBuffer {
    SoAAppearance soa_appearance[];
};

layout(std430, binding = 6) readonly buffer SoAEffectsBuffer {
    SoAEffects soa_effects[];
};

// Per-frame uniforms - different for OpenGL vs Vulkan
#ifdef STYGIAN_GL
uniform vec2 uScreenSize;
#define SCREEN_SIZE uScreenSize
#define INSTANCE_ID gl_InstanceID
#else
layout(push_constant) uniform PushConstants {
    vec4 uScreenAtlas;   // xy=screen size, zw=atlas size
    vec4 uPxRangeFlags;  // x=px range, y=enabled, z=src sRGB, w=dst sRGB
    vec4 uOutputRow0;    // xyz=row0
    vec4 uOutputRow1;    // xyz=row1
    vec4 uOutputRow2;    // xyz=row2
    vec4 uGamma;         // x=src gamma, y=dst gamma
} pc;
#define SCREEN_SIZE pc.uScreenAtlas.xy
#define INSTANCE_ID gl_InstanceIndex
#endif

// Pass element data to fragment shader via flat varyings
layout(location = 0) flat out vec4 vColor;
layout(location = 1) flat out vec4 vBorderColor;
layout(location = 2) flat out vec4 vRadius;
layout(location = 3) flat out vec4 vUV;
layout(location = 4) flat out uint vType;
layout(location = 5) flat out float vBlend;
layout(location = 6) flat out float vHover;
layout(location = 7) out vec2 vLocalPos;
layout(location = 8) out vec2 vSize;
layout(location = 9) flat out uint vInstanceID;
layout(location = 10) flat out uint vTextureID;
layout(location = 11) flat out vec4 vReserved0; // _reserved[0] for bezier/wire/metaball

void main() {
    // Read from SoA (primary path)
    SoAHot h = soa_hot[INSTANCE_ID];

    if ((h.flags & 1u) == 0u) {
        gl_Position = vec4(-2.0, -2.0, 0.0, 1.0);
        return;
    }

    vec2 uv01 = aPos * 0.5 + 0.5;
    vec2 size = vec2(h.w, h.h);

    // Pixel space position (Y-down)
    vec2 pixelPos = vec2(h.x, h.y) + vec2(uv01.x, 1.0 - uv01.y) * size;
    vec2 ndc = (pixelPos / SCREEN_SIZE) * 2.0 - 1.0;

    // Flip Y for OpenGL viewport
    #ifdef STYGIAN_GL
    ndc.y = -ndc.y;
    #endif

    gl_Position = vec4(ndc, h.z, 1.0);

    // Hot data
    vColor = h.color;
    vType = h.type;
    vTextureID = h.texture_id;

    // Appearance data
    SoAAppearance a = soa_appearance[INSTANCE_ID];
    vBorderColor = a.border_color;
    vRadius = a.radius;
    vUV = a.uv;

    // Effects data
    SoAEffects fx = soa_effects[INSTANCE_ID];
    vBlend = fx.blend;
    vHover = fx.hover;

    // Geometry
    vLocalPos = vec2(uv01.x, 1.0 - uv01.y) * size;
    vSize = size;
    vInstanceID = INSTANCE_ID;

    // Pass control points from SoA (bezier/wire/metaball)
    vReserved0 = a.control_points;
}


# text.glsl
// text.glsl - MTSDF text rendering

// Font texture sampler (binding 1)
layout(binding = 1) uniform sampler2D uFontTex;
layout(binding = 2) uniform sampler2D uImageTex[16];

// Text uniforms - different for OpenGL vs Vulkan
#ifdef STYGIAN_GL
uniform vec2 uAtlasSize;
uniform float uPxRange;
#define ATLAS_SIZE uAtlasSize
#define PX_RANGE uPxRange
#else
// Vulkan uses push constants declared in stygian.frag.
#define ATLAS_SIZE pc.uScreenAtlas.zw
#define PX_RANGE pc.uPxRangeFlags.x
#endif

// Type 6: STYGIAN_TEXT - MTSDF text
vec4 render_text(vec2 localPos, vec2 size, vec4 uv, vec4 color, float blend) {
    vec2 uv_norm = localPos / size;
    uv_norm.y = 1.0 - uv_norm.y;
    vec2 texCoord = mix(uv.xy, uv.zw, uv_norm);
    vec4 mtsdf = texture(uFontTex, texCoord);
    
    // Multi-channel signed distance field decode
    float sd = max(min(mtsdf.r, mtsdf.g), min(max(mtsdf.r, mtsdf.g), mtsdf.b));
    
    // Screen-space anti-aliasing (msdfgen-style)
    vec2 unitRange = vec2(PX_RANGE) / ATLAS_SIZE;
    vec2 screenTexSize = vec2(1.0) / fwidth(texCoord);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
    float alpha = clamp((sd - 0.5) * screenPxRange + 0.5, 0.0, 1.0);
    
    return vec4(color.rgb, alpha * color.a * blend);
}

// Type 10: STYGIAN_TEXTURE - Texture/image
vec4 render_texture(vec2 localPos, vec2 size, vec4 uv_rect, vec4 color, uint texSlot) {
    vec2 uv01 = localPos / size;
    vec2 uv = mix(uv_rect.xy, uv_rect.zw, uv01);
    if (texSlot >= 16u) {
        return vec4(1.0, 0.0, 1.0, 1.0) * color;
    }
    return texture(uImageTex[int(texSlot)], uv) * color;
}


# ui.glsl
// ui.glsl - UI element rendering 
// Includes: rect, outline, circle, icons, metaball, separator

// Type 0: STYGIAN_RECT - Rounded rectangle
float render_rect(vec2 p, vec2 center, vec4 r) {
    return sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
}

// Type 1: STYGIAN_RECT_OUTLINE - Rounded rectangle outline
float render_rect_outline(vec2 p, vec2 center, vec4 r) {
    float outer = sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
    float inner = sdRoundedBox(p, center - 3.0, vec4(max(0.0, r.z-2.0), max(0.0, r.y-2.0), max(0.0, r.w-2.0), max(0.0, r.x-2.0)));
    return max(outer, -inner);
}

// Type 2: STYGIAN_CIRCLE - Circle
float render_circle(vec2 p, vec2 center) {
    return sdCircle(p, min(center.x, center.y) - 1.0);
}

// Type 3 & 4: STYGIAN_METABALL_LEFT/RIGHT - Metaball menu
float render_metaball(vec2 p, vec2 center, vec4 r) {
    return sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
}

// Type 7: STYGIAN_ICON_CLOSE - Close icon (X)
float render_icon_close(vec2 p, vec2 center) {
    float arm = min(center.x, center.y) * 0.35;
    float d1 = sdSegment(p, vec2(-arm, -arm), vec2(arm, arm)) - 1.5;
    float d2 = sdSegment(p, vec2(-arm, arm), vec2(arm, -arm)) - 1.5;
    return min(d1, d2);
}

// Type 8: STYGIAN_ICON_MAXIMIZE - Maximize icon (square outline)
float render_icon_maximize(vec2 p, vec2 center) {
    float box_size = min(center.x, center.y) * 0.4;
    float outer = sdBox(p, vec2(box_size));
    float inner = sdBox(p, vec2(box_size - 1.5));
    return max(outer, -inner);
}

// Type 9: STYGIAN_ICON_MINIMIZE - Minimize icon (horizontal line)
float render_icon_minimize(vec2 p, vec2 center) {
    float line_w = center.x * 0.5;
    return sdBox(p, vec2(line_w, 1.0));
}

// Type 11: STYGIAN_SEPARATOR - Separator line
float render_separator(vec2 p) {
    return abs(p.y) - 0.5;
}


# window.glsl
// window.glsl - Window chrome rendering
// Includes: window body with gradient border

// Type 5: STYGIAN_WINDOW_BODY - Window body with gradient border
float render_window_body(vec2 p, vec2 center, vec4 r, vec4 borderColor, inout vec4 col) {
    float d = sdRoundedBox(p, center, vec4(r.z, r.y, r.w, r.x));
    float aa = fwidth(d) * 1.5;
    float border_t = smoothstep(-6.0, -1.0, d);
    vec3 bot_col = vec3(0.06);
    float t = clamp((p.y / center.y) * 0.5 + 0.5, 0.0, 1.0);
    vec3 border_grad = mix(bot_col, borderColor.rgb, t);
    col.rgb = mix(col.rgb, border_grad, border_t);
    return d;
}


# stygian.frag.glsl
#version 430 core






layout(location = 0) flat in vec4 vColor;
layout(location = 1) flat in vec4 vBorderColor;
layout(location = 2) flat in vec4 vRadius;
layout(location = 3) flat in vec4 vUV;
layout(location = 4) flat in uint vType;
layout(location = 5) flat in float vBlend;
layout(location = 6) flat in float vHover;
layout(location = 7) in vec2 vLocalPos;
layout(location = 8) in vec2 vSize;
layout(location = 9) flat in uint vInstanceID;
layout(location = 10) flat in uint vTextureID;
layout(location = 11) flat in vec4 vReserved0;

layout(location = 0) out vec4 fragColor;


layout(std430, binding = 3) readonly buffer ClipBuffer {
    vec4 clip_rects[];
};


struct SoAHot {
    float x, y, w, h;
    vec4 color;
    uint texture_id;
    uint type;
    uint flags;
    float z;
};

struct SoAAppearance {
    vec4 border_color;
    vec4 radius;
    vec4 uv;
    vec4 control_points;
};

struct SoAEffects {
    vec2 shadow_offset;
    float shadow_blur;
    float shadow_spread;
    vec4 shadow_color;
    vec4 gradient_start;
    vec4 gradient_end;
    float hover;
    float blend;
    float gradient_angle;
    float blur_radius;
    float glow_intensity;
    uint parent_id;
    vec2 _pad;
};

layout(std430, binding = 4) readonly buffer SoAHotBuffer {
    SoAHot soa_hot[];
};

layout(std430, binding = 5) readonly buffer SoAAppearanceBuffer {
    SoAAppearance soa_appearance[];
};

layout(std430, binding = 6) readonly buffer SoAEffectsBuffer {
    SoAEffects soa_effects[];
};
















float sdRoundedBox(vec2 p, vec2 b, vec4 r) {



    r.xy = (p.x > 0.0) ? r.yz : r.xw;
    r.x = (p.y > 0.0) ? r.y : r.x;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}


float dot2(vec2 v) { return dot(v, v); }


float smoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}





float sdTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return - sqrt(d.x) * sign(d.y);
}

float sdEllipse(vec2 p, vec2 ab) {
    p = abs(p);
    if (p.x > p.y) { p = p.yx; ab = ab.yx; }
    float l = ab.y * ab.y - ab.x * ab.x;
    float m = ab.x * p.x / l;
    float m2 = m * m;
    float n = ab.y * p.y / l;
    float n2 = n * n;
    float c = (m2 + n2 - 1.0) / 3.0;
    float c3 = c * c * c;
    float q = c3 + m2 * n2 * 2.0;
    float d = c3 + m2 * n2;
    float g = m + m * n2;
    float co;
    if (d < 0.0) {
        float h = acos(q / c3) / 3.0;
        float s = cos(h);
        float t = sin(h) * sqrt(3.0);
        float rx = sqrt(- c * (s + t + 2.0) + m2);
        float ry = sqrt(- c * (s - t + 2.0) + m2);
        co = (ry + sign(l) * rx + abs(g) / (rx * ry) - m) / 2.0;
    } else {
        float h = 2.0 * m * n * sqrt(d);
        float s = sign(q + h) * pow(abs(q + h), 1.0 / 3.0);
        float u = sign(q - h) * pow(abs(q - h), 1.0 / 3.0);
        float rx = - s - u - c * 4.0 + 2.0 * m2;
        float ry = (s - u) * sqrt(3.0);
        float rm = sqrt(rx * rx + ry * ry);
        co = (ry / sqrt(rm - rx) + 2.0 * g / rm - m) / 2.0;
    }
    vec2 r = ab * vec2(co, sqrt(1.0 - co * co));
    return length(r - p) * sign(p.y - r.y);
}

float sdArc(vec2 p, vec2 sc, float ra, float rb) {

    p.x = abs(p.x);
    return( (sc.y * p.x > sc.x * p.y) ? length(p - sc * ra) :
            abs(length(p) - ra)) - rb;
}


float sdBezier(vec2 pos, vec2 A, vec2 B, vec2 C) {
    vec2 a = B - A;
    vec2 b = A - 2.0 * B + C;
    vec2 c = a * 2.0;
    vec2 d = A - pos;
    float kk = 1.0 / dot(b, b);
    float kx = kk * dot(a, b);
    float ky = kk * (2.0 * dot(a, a) + dot(d, b)) / 3.0;
    float kz = kk * dot(d, a);
    float res = 0.0;
    float p = ky - kx * kx;
    float p3 = p * p * p;
    float q = kx * (2.0 * kx * kx - 3.0 * ky) + kz;
    float h = q * q + 4.0 * p3;
    if (h >= 0.0) {
        h = sqrt(h);
        vec2 x = (vec2(h, - h) - q) / 2.0;
        vec2 uv = sign(x) * pow(abs(x), vec2(1.0 / 3.0));
        float t = clamp(uv.x + uv.y - kx, 0.0, 1.0);
        res = dot2(d + (c + b * t) * t);
    } else {
        float z = sqrt(- p);
        float v = acos(q / (p * z * 2.0)) / 3.0;
        float m = cos(v);
        float n = sin(v) * 1.732050808;
        vec3 t = clamp(vec3(m + m, - n - m, n - m) * z - kx, 0.0, 1.0);
        res = min(dot2(d + (c + b * t.x) * t.x),
                  dot2(d + (c + b * t.y) * t.y));
    }
    return sqrt(res);
}


float sdCubicBezierApprox(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3) {
    vec2 m = (p0 + 3.0 * p1 + 3.0 * p2 + p3) * 0.125;
    vec2 q1_ctrl = (3.0 * p1 + p0) * 0.25;
    vec2 q2_ctrl = (3.0 * p2 + p3) * 0.25;

    float d1 = sdBezier(p, p0, q1_ctrl, m);
    float d2 = sdBezier(p, m, q2_ctrl, p3);
    return min(d1, d2);
}



float sdPolygon(vec2 p, vec2 v[8], int N) {
    float d = dot(p - v[0], p - v[0]);
    float s = 1.0;
    for (int i = 0, j = N - 1; i < N; j = i, i ++) {
        vec2 e = v[j] - v[i];
        vec2 w = p - v[i];
        vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        d = min(d, dot(b, b));
        bvec3 c = bvec3(p.y >= v[i].y, p.y < v[j].y, e.x * w.y > e.y * w.x);
        if (all(c) || all(not(c))) s *= - 1.0;
    }
    return s * sqrt(d);
}





layout(binding = 1) uniform sampler2D uFontTex;
layout(binding = 2) uniform sampler2D uImageTex[16];



uniform vec2 uAtlasSize;
uniform float uPxRange;









vec4 render_text(vec2 localPos, vec2 size, vec4 uv, vec4 color, float blend) {
    vec2 uv_norm = localPos / size;
    uv_norm.y = 1.0 - uv_norm.y;
    vec2 texCoord = mix(uv.xy, uv.zw, uv_norm);
    vec4 mtsdf = texture(uFontTex, texCoord);


    float sd = max(min(mtsdf.r, mtsdf.g), min(max(mtsdf.r, mtsdf.g), mtsdf.b));


    vec2 unitRange = vec2(uPxRange) / uAtlasSize;
    vec2 screenTexSize = vec2(1.0) / fwidth(texCoord);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
    float alpha = clamp( (sd - 0.5) * screenPxRange + 0.5, 0.0, 1.0);

    return vec4(color.rgb, alpha * color.a * blend);
}


vec4 render_texture(vec2 localPos, vec2 size, vec4 uv_rect, vec4 color, uint texSlot) {
    vec2 uv01 = localPos / size;
    vec2 uv = mix(uv_rect.xy, uv_rect.zw, uv01);
    if (texSlot >= 16u) {
        return vec4(1.0, 0.0, 1.0, 1.0) * color;
    }
    return texture(uImageTex[int(texSlot)], uv) * color;
}




float render_window_body(vec2 p, vec2 center, vec4 r, vec4 borderColor, inout vec4 col) {
    float d = sdRoundedBox(p, center, vec4(r.z, r.y, r.w, r.x));
    float aa = fwidth(d) * 1.5;
    float border_t = smoothstep(- 6.0, - 1.0, d);
    vec3 bot_col = vec3(0.06);
    float t = clamp( (p.y / center.y) * 0.5 + 0.5, 0.0, 1.0);
    vec3 border_grad = mix(bot_col, borderColor.rgb, t);
    col.rgb = mix(col.rgb, border_grad, border_t);
    return d;
}




float render_rect(vec2 p, vec2 center, vec4 r) {
    return sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
}


float render_rect_outline(vec2 p, vec2 center, vec4 r) {
    float outer = sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
    float inner = sdRoundedBox(p, center - 3.0, vec4(max(0.0, r.z - 2.0), max(0.0, r.y - 2.0), max(0.0, r.w - 2.0), max(0.0, r.x - 2.0)));
    return max(outer, - inner);
}


float render_circle(vec2 p, vec2 center) {
    return sdCircle(p, min(center.x, center.y) - 1.0);
}


float render_metaball(vec2 p, vec2 center, vec4 r) {
    return sdRoundedBox(p, center - 1.0, vec4(r.z, r.y, r.w, r.x));
}


float render_icon_close(vec2 p, vec2 center) {
    float arm = min(center.x, center.y) * 0.35;
    float d1 = sdSegment(p, vec2(- arm, - arm), vec2(arm, arm)) - 1.5;
    float d2 = sdSegment(p, vec2(- arm, arm), vec2(arm, - arm)) - 1.5;
    return min(d1, d2);
}


float render_icon_maximize(vec2 p, vec2 center) {
    float box_size = min(center.x, center.y) * 0.4;
    float outer = sdBox(p, vec2(box_size));
    float inner = sdBox(p, vec2(box_size - 1.5));
    return max(outer, - inner);
}


float render_icon_minimize(vec2 p, vec2 center) {
    float line_w = center.x * 0.5;
    return sdBox(p, vec2(line_w, 1.0));
}


float render_separator(vec2 p) {
    return abs(p.y) - 0.5;
}


uniform int uOutputColorTransformEnabled;
uniform mat3 uOutputColorMatrix;
uniform int uOutputSrcIsSRGB;
uniform float uOutputSrcGamma;
uniform int uOutputDstIsSRGB;
uniform float uOutputDstGamma;


float stygian_to_linear_channel(float c, bool srgb_transfer, float gamma_val) {
    if (srgb_transfer) {
        if (c <= 0.04045) {
            return c / 12.92;
        }
        return pow( (c + 0.055) / 1.055, 2.4);
    }
    return pow(c, max(gamma_val, 0.0001));
}

float stygian_from_linear_channel(float c, bool srgb_transfer, float gamma_val) {
    if (srgb_transfer) {
        if (c <= 0.0031308) {
            return c * 12.92;
        }
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    }
    return pow(c, 1.0 / max(gamma_val, 0.0001));
}

vec4 apply_output_color_transform(vec4 color_in) {

    if (uOutputColorTransformEnabled == 0) {
        return color_in;
    }
    vec3 c = clamp(color_in.rgb, 0.0, 1.0);
    vec3 linear_rgb = vec3(
        stygian_to_linear_channel(c.r, uOutputSrcIsSRGB != 0, uOutputSrcGamma),
        stygian_to_linear_channel(c.g, uOutputSrcIsSRGB != 0, uOutputSrcGamma),
        stygian_to_linear_channel(c.b, uOutputSrcIsSRGB != 0, uOutputSrcGamma)
    );
    vec3 output_linear = clamp(uOutputColorMatrix * linear_rgb, 0.0, 1.0);
    vec3 output_rgb = vec3(
        stygian_from_linear_channel(output_linear.r, uOutputDstIsSRGB != 0, uOutputDstGamma),
        stygian_from_linear_channel(output_linear.g, uOutputDstIsSRGB != 0, uOutputDstGamma),
        stygian_from_linear_channel(output_linear.b, uOutputDstIsSRGB != 0, uOutputDstGamma)
    );
    return vec4(clamp(output_rgb, 0.0, 1.0), color_in.a);























}


float render_metaball_group(vec2 p, uint id, vec4 reserved0, float k) {
    SoAHot h = soa_hot[id];


    uint start = uint(reserved0.x);
    uint count = uint(reserved0.y);

    vec2 container_pos = vec2(h.x, h.y);
    vec2 container_size = vec2(h.w, h.h);
    vec2 container_center = container_pos + container_size * 0.5;

    vec2 frag_screen_pos = container_center + p;

    float d = 1000.0;

    for (uint i = 0; i < count; i ++) {
        uint child_idx = start + i;
        SoAHot ch = soa_hot[child_idx];
        SoAAppearance ca = soa_appearance[child_idx];

        vec2 child_size = vec2(ch.w, ch.h);
        vec2 child_center = vec2(ch.x, ch.y) + child_size * 0.5;
        vec2 child_p = frag_screen_pos - child_center;

        float child_d = sdRoundedBox(child_p, child_size * 0.5, ca.radius);

        if (i == 0u) d = child_d;
        else d = smoothUnion(d, child_d, k);
    }

    return d;
}

void main() {
    vec2 center = vSize * 0.5;
    vec2 p = vLocalPos - center;
    vec4 r = vRadius;
    uint type = vType;


    SoAHot h_clip = soa_hot[vInstanceID];
    uint clip_id = (h_clip.flags & 0x0000FF00u) >> 8u;
    if (clip_id != 0u) {
        vec4 clip_rect = clip_rects[clip_id];
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        if (worldP.x < clip_rect.x || worldP.y < clip_rect.y ||
            worldP.x > clip_rect.x + clip_rect.z ||
            worldP.y > clip_rect.y + clip_rect.w) {
            discard;
        }
    }

    vec4 col = vColor;
    float d = 1000.0;
    float aa = 1.5;



    if (type == 0u) {
        d = render_rect(p, center, r);
        aa = fwidth(d) * 1.5;
    }

    else if (type == 1u) {
        d = render_rect_outline(p, center, r);
        aa = fwidth(d) * 1.5;
    }

    else if (type == 2u) {
        d = render_circle(p, center);
        aa = fwidth(d) * 1.5;
    }

    else if (type == 3u) {
        d = render_metaball(p, center, r);
        aa = fwidth(d) * 1.5;
    }

    else if (type == 4u) {
        d = render_metaball(p, center, r);
        aa = fwidth(d) * 1.5;
    }

    else if (type == 5u) {
        d = render_window_body(p, center, r, vBorderColor, col);
        aa = fwidth(d) * 1.5;
    }

    else if (type == 6u) {
        fragColor = apply_output_color_transform(render_text(vLocalPos, vSize, vUV, col, vBlend));
        if (fragColor.a < 0.01) discard;
        return;
    }

    else if (type == 7u) {
        d = render_icon_close(p, center);
        aa = 1.5;
    }

    else if (type == 8u) {
        d = render_icon_maximize(p, center);
        aa = 1.5;
    }

    else if (type == 9u) {
        d = render_icon_minimize(p, center);
        aa = 1.5;
    }

    else if (type == 10u) {
        fragColor = apply_output_color_transform(render_texture(vLocalPos, vSize, vUV, col, vTextureID));
        return;
    }

    else if (type == 11u) {
        d = render_separator(p);
        aa = 1.5;
    }

    else if (type == 12u) {

        float k = vBlend;
        if (k < 0.1) k = 10.0;

        d = render_metaball_group(p, vInstanceID, vReserved0, k);
        aa = fwidth(d) * 1.5;
    }

    else if (type == 15u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 a = vUV.xy;
        vec2 b = vUV.zw;
        float half_thick = vRadius.x;
        d = sdSegment(worldP, a, b) - half_thick;
        aa = fwidth(d) * 1.5;
    }

    else if (type == 16u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 A = vUV.xy;
        vec2 C = vUV.zw;
        vec2 B = vReserved0.xy;
        float half_thick = vRadius.x;
        d = sdBezier(worldP, A, B, C) - half_thick;
        aa = fwidth(d) * 1.5;
    }

    else if (type == 17u) {
        vec2 worldP = vec2(h_clip.x, h_clip.y) + vLocalPos;
        vec2 A = vUV.xy;
        vec2 D = vUV.zw;
        vec2 B = vReserved0.xy;
        vec2 C = vReserved0.zw;
        float half_thick = vRadius.x;
        d = sdCubicBezierApprox(worldP, A, B, C, D) - half_thick;
        aa = fwidth(d) * 1.5;
    }
    else {

        d = sdBox(p, center - 1.0);
        aa = 1.5;
    }


    if (vHover > 0.0) {
        col.rgb = mix(col.rgb, col.rgb * 1.3, vHover);
    }

    float alpha = 1.0 - smoothstep(- aa, aa, d);
    col.a *= alpha * vBlend;

    if (col.a < 0.01) discard;
    fragColor = apply_output_color_transform(col);
}


# stygian.vert.glsl
#version 430 core
layout(location = 0) in vec2 aPos;


struct SoAHot {
    float x, y, w, h;
    vec4 color;
    uint texture_id;
    uint type;
    uint flags;
    float z;
};

struct SoAAppearance {
    vec4 border_color;
    vec4 radius;
    vec4 uv;
    vec4 control_points;
};

struct SoAEffects {
    vec2 shadow_offset;
    float shadow_blur;
    float shadow_spread;
    vec4 shadow_color;
    vec4 gradient_start;
    vec4 gradient_end;
    float hover;
    float blend;
    float gradient_angle;
    float blur_radius;
    float glow_intensity;
    uint parent_id;
    vec2 _pad;
};

layout(std430, binding = 4) readonly buffer SoAHotBuffer {
    SoAHot soa_hot[];
};

layout(std430, binding = 5) readonly buffer SoAAppearanceBuffer {
    SoAAppearance soa_appearance[];
};

layout(std430, binding = 6) readonly buffer SoAEffectsBuffer {
    SoAEffects soa_effects[];
};



uniform vec2 uScreenSize;
















layout(location = 0) flat out vec4 vColor;
layout(location = 1) flat out vec4 vBorderColor;
layout(location = 2) flat out vec4 vRadius;
layout(location = 3) flat out vec4 vUV;
layout(location = 4) flat out uint vType;
layout(location = 5) flat out float vBlend;
layout(location = 6) flat out float vHover;
layout(location = 7) out vec2 vLocalPos;
layout(location = 8) out vec2 vSize;
layout(location = 9) flat out uint vInstanceID;
layout(location = 10) flat out uint vTextureID;
layout(location = 11) flat out vec4 vReserved0;

void main() {

    SoAHot h = soa_hot[gl_InstanceID];

    if ( (h.flags & 1u) == 0u) {
        gl_Position = vec4(- 2.0, - 2.0, 0.0, 1.0);
        return;
    }

    vec2 uv01 = aPos * 0.5 + 0.5;
    vec2 size = vec2(h.w, h.h);


    vec2 pixelPos = vec2(h.x, h.y) + vec2(uv01.x, 1.0 - uv01.y) * size;
    vec2 ndc = (pixelPos / uScreenSize) * 2.0 - 1.0;



    ndc.y = - ndc.y;


    gl_Position = vec4(ndc, h.z, 1.0);


    vColor = h.color;
    vType = h.type;
    vTextureID = h.texture_id;


    SoAAppearance a = soa_appearance[gl_InstanceID];
    vBorderColor = a.border_color;
    vRadius = a.radius;
    vUV = a.uv;


    SoAEffects fx = soa_effects[gl_InstanceID];
    vBlend = fx.blend;
    vHover = fx.hover;


    vLocalPos = vec2(uv01.x, 1.0 - uv01.y) * size;
    vSize = size;
    vInstanceID = gl_InstanceID;


    vReserved0 = a.control_points;
}


# stygian_ap.h
// stygian_ap.h - Graphics Access Point Interface
// Part of Stygian UI Library
// This is the ONLY layer that touches GPU APIs
//
// DISCIPLINE: AP does context, frame, submit, texture, shader.
//             AP does NOT do layout, fonts, hit testing, animation.
#ifndef STYGIAN_AP_H
#define STYGIAN_AP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct StygianWindow StygianWindow;

typedef struct StygianAllocator StygianAllocator;
typedef struct StygianSoAHot StygianSoAHot;
typedef struct StygianSoAAppearance StygianSoAAppearance;
typedef struct StygianSoAEffects StygianSoAEffects;
typedef struct StygianBufferChunk StygianBufferChunk;

// ============================================================================
// Access Point Handle
// ============================================================================

typedef struct StygianAP StygianAP;

typedef enum StygianAPAdapterClass {
  STYGIAN_AP_ADAPTER_UNKNOWN = 0,
  STYGIAN_AP_ADAPTER_IGPU = 1,
  STYGIAN_AP_ADAPTER_DGPU = 2,
} StygianAPAdapterClass;

typedef enum StygianAPType {
  STYGIAN_AP_OPENGL,
  STYGIAN_AP_VULKAN,
  STYGIAN_AP_DX12,
  STYGIAN_AP_METAL,
} StygianAPType;

typedef struct StygianAPConfig {
  StygianAPType type;
  StygianWindow *window;       // Required: window for context creation
  uint32_t max_elements;       // Max elements in SSBO/UBO
  uint32_t max_textures;       // Max texture slots
  const char *shader_dir;      // Path to shader files (for hot reload)
  StygianAllocator *allocator; // Optional: defaults to CRT allocator
} StygianAPConfig;

// ============================================================================
// Lifecycle
// ============================================================================

// Create graphics access point
// Returns NULL on failure
StygianAP *stygian_ap_create(const StygianAPConfig *config);

// Destroy graphics access point and release GPU resources
void stygian_ap_destroy(StygianAP *ap);

// Query adapter class selected by backend (for policy decisions in core).
StygianAPAdapterClass stygian_ap_get_adapter_class(const StygianAP *ap);
uint32_t stygian_ap_get_last_upload_bytes(const StygianAP *ap);
uint32_t stygian_ap_get_last_upload_ranges(const StygianAP *ap);
float stygian_ap_get_last_gpu_ms(const StygianAP *ap);

// ============================================================================
// Multi-Surface (for floating windows, additional viewports)
// ============================================================================

// Opaque handle for per-window render surface
typedef struct StygianAPSurface StygianAPSurface;

// Create surface for an additional window (shares device/pipeline with main)
// Returns NULL on failure
StygianAPSurface *stygian_ap_surface_create(StygianAP *ap,
                                            StygianWindow *window);

// Destroy surface
void stygian_ap_surface_destroy(StygianAP *ap, StygianAPSurface *surface);

// Begin rendering to a surface
void stygian_ap_surface_begin(StygianAP *ap, StygianAPSurface *surface,
                              int width, int height);

// Submit elements to a surface (updates SSBO and issues draw call)
void stygian_ap_surface_submit(StygianAP *ap, StygianAPSurface *surface,
                               const StygianSoAHot *soa_hot, uint32_t count);

// End rendering to a surface (records commands)
void stygian_ap_surface_end(StygianAP *ap, StygianAPSurface *surface);

// Present surface (swap buffers)
void stygian_ap_surface_swap(StygianAP *ap, StygianAPSurface *surface);

// Get main window's surface (for consistency with multi-surface API)
StygianAPSurface *stygian_ap_get_main_surface(StygianAP *ap);

// ============================================================================
// Frame Management
// ============================================================================

// Begin frame - sets up viewport, clears, binds program
void stygian_ap_begin_frame(StygianAP *ap, int width, int height);

// Texture handle→sampler remapping + bind for the active batch.
void stygian_ap_submit(StygianAP *ap, const StygianSoAHot *soa_hot,
                       uint32_t count);

// Submit SoA buffers with versioned chunk upload
// Compares CPU chunk versions against GPU versions; uploads only dirty ranges
void stygian_ap_submit_soa(StygianAP *ap, const StygianSoAHot *hot,
                           const StygianSoAAppearance *appearance,
                           const StygianSoAEffects *effects,
                           uint32_t element_count,
                           const StygianBufferChunk *chunks,
                           uint32_t chunk_count, uint32_t chunk_size);

// Issue draw call for the most recently submitted batch
void stygian_ap_draw(StygianAP *ap);
void stygian_ap_draw_range(StygianAP *ap, uint32_t first_instance,
                           uint32_t instance_count);

// Optional GPU timing (backend-dependent).
// If unsupported, begin/end are no-ops and last_gpu_ms returns 0.
void stygian_ap_gpu_timer_begin(StygianAP *ap);
void stygian_ap_gpu_timer_end(StygianAP *ap);

// End frame - finalize frame (no draw for GL; ends command buffer for VK)
void stygian_ap_end_frame(StygianAP *ap);

// Swap buffers (present)
void stygian_ap_swap(StygianAP *ap);

// Make this AP's context current (restores main window context)
void stygian_ap_make_current(StygianAP *ap);

// Set viewport (useful after restoring context)
void stygian_ap_set_viewport(StygianAP *ap, int width, int height);

// ============================================================================
// Textures (GPU memory owned by AP)
// ============================================================================

typedef uint32_t StygianAPTexture;

// Create texture from RGBA data
// Returns 0 on failure
StygianAPTexture stygian_ap_texture_create(StygianAP *ap, int w, int h,
                                           const void *rgba);
bool stygian_ap_texture_update(StygianAP *ap, StygianAPTexture tex, int x,
                               int y, int w, int h, const void *rgba);

// Destroy texture
void stygian_ap_texture_destroy(StygianAP *ap, StygianAPTexture tex);

// Bind texture to slot for rendering
void stygian_ap_texture_bind(StygianAP *ap, StygianAPTexture tex,
                             uint32_t slot);

// ============================================================================
// Shader Management
// ============================================================================

// Reload shaders from disk (for hot reload)
// Returns true on success
bool stygian_ap_reload_shaders(StygianAP *ap);

// Check if shader files have been modified since last load
// Call this once per frame, reload if returns true
bool stygian_ap_shaders_need_reload(StygianAP *ap);

// ============================================================================
// Uniforms (per-frame data)
// ============================================================================

// Set font atlas texture for MTSDF rendering
void stygian_ap_set_font_texture(StygianAP *ap, StygianAPTexture tex,
                                 int atlas_w, int atlas_h, float px_range);

// Set output color transform applied in fragment shader.
// rgb3x3 is row-major source-linear RGB -> destination-linear RGB matrix.
void stygian_ap_set_output_color_transform(
    StygianAP *ap, bool enabled, const float *rgb3x3, bool src_srgb_transfer,
    float src_gamma, bool dst_srgb_transfer, float dst_gamma);

// ============================================================================
// Clip Regions
// ============================================================================

// Update clip regions array (up to 256)
void stygian_ap_set_clips(StygianAP *ap, const float *clips, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_AP_H


# stygian_ap_gl.c
// stygian_ap_gl.c - OpenGL 4.3+ Access Point Implementation
// Part of Stygian UI Library
// DISCIPLINE: Only GPU operations. No layout, no fonts, no hit testing.
#include "../include/stygian.h" // For StygianGPUElement
#include "../include/stygian_memory.h"
#include "../src/stygian_internal.h" // For SoA struct types
#include "../window/stygian_window.h"
#include "stygian_ap.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

// ============================================================================
// OpenGL Types & Constants
// ============================================================================

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef ptrdiff_t GLsizeiptr;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef float GLfloat;

// Use ifndef guards to avoid conflicts with system gl.h
#ifndef GL_FALSE
#define GL_FALSE 0
#endif
#ifndef GL_TRUE
#define GL_TRUE 1
#endif
#ifndef GL_TRIANGLES
#define GL_TRIANGLES 0x0004
#endif
#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_VALIDATE_STATUS
#define GL_VALIDATE_STATUS 0x8B83
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COLOR_BUFFER_BIT
#define GL_COLOR_BUFFER_BIT 0x00004000
#endif
#ifndef GL_BLEND
#define GL_BLEND 0x0BE2
#endif
#ifndef GL_SRC_ALPHA
#define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MIN_FILTER 0x2801
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAG_FILTER 0x2800
#endif
#ifndef GL_LINEAR
#define GL_LINEAR 0x2601
#endif
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#endif

// ============================================================================
// OpenGL Function Pointers
// ============================================================================

typedef void (*PFNGLGENBUFFERSPROC)(GLsizei, GLuint *);
typedef void (*PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void (*PFNGLBUFFERSUBDATAPROC)(GLenum, GLsizeiptr, GLsizeiptr,
                                       const void *);
typedef void (*PFNGLBINDBUFFERBASEPROC)(GLenum, GLuint, GLuint);
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar **,
                                      const GLint *);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint);
typedef void (*PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint *);
typedef void (*PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint);
typedef void (*PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint *);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei *,
                                           GLchar *);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar *);
typedef void (*PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void (*PFNGLUNIFORM1IVPROC)(GLint, GLsizei, const GLint *);
typedef void (*PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void (*PFNGLUNIFORM2FPROC)(GLint, GLfloat, GLfloat);
typedef void (*PFNGLUNIFORMMATRIX3FVPROC)(GLint, GLsizei, GLboolean,
                                          const GLfloat *);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean,
                                             GLsizei, const void *);
typedef void (*PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint *);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void (*PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum, GLint, GLsizei, GLsizei);
typedef void (*PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC)(GLenum, GLint, GLsizei,
                                                         GLsizei, GLuint);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint *);
typedef void (*PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void (*PFNGLACTIVETEXTUREPROC)(GLenum);

static PFNGLGENBUFFERSPROC glGenBuffers;
static PFNGLBINDBUFFERPROC glBindBuffer;
static PFNGLBUFFERDATAPROC glBufferData;
static PFNGLBUFFERSUBDATAPROC glBufferSubData;
static PFNGLBINDBUFFERBASEPROC glBindBufferBase;
static PFNGLCREATESHADERPROC glCreateShader;
static PFNGLSHADERSOURCEPROC glShaderSource;
static PFNGLCOMPILESHADERPROC glCompileShader;
static PFNGLGETSHADERIVPROC glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
static PFNGLCREATEPROGRAMPROC glCreateProgram;
static PFNGLATTACHSHADERPROC glAttachShader;
static PFNGLLINKPROGRAMPROC glLinkProgram;
static PFNGLUSEPROGRAMPROC glUseProgram;
static PFNGLGETPROGRAMIVPROC glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
static PFNGLUNIFORM1IPROC glUniform1i;
static PFNGLUNIFORM1IVPROC glUniform1iv;
static PFNGLUNIFORM1FPROC glUniform1f;
static PFNGLUNIFORM2FPROC glUniform2f;
static PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
static PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
static PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC
    glDrawArraysInstancedBaseInstance;
static PFNGLDELETEBUFFERSPROC glDeleteBuffers;
static PFNGLDELETEPROGRAMPROC glDeleteProgram;
static PFNGLACTIVETEXTUREPROC glActiveTexture;

// Shader cleanup and validation
typedef void (*PFNGLDELETESHADERPROC)(GLuint);
typedef void (*PFNGLVALIDATEPROGRAMPROC)(GLuint);
static PFNGLDELETESHADERPROC glDeleteShader;
static PFNGLVALIDATEPROGRAMPROC glValidateProgram;
typedef void (*PFNGLGENQUERIESPROC)(GLsizei, GLuint *);
typedef void (*PFNGLDELETEQUERIESPROC)(GLsizei, const GLuint *);
typedef void (*PFNGLBEGINQUERYPROC)(GLenum, GLuint);
typedef void (*PFNGLENDQUERYPROC)(GLenum);
typedef void (*PFNGLGETQUERYOBJECTIVPROC)(GLuint, GLenum, GLint *);
typedef void (*PFNGLGETQUERYOBJECTUI64VPROC)(GLuint, GLenum, uint64_t *);
static PFNGLGENQUERIESPROC glGenQueries;
static PFNGLDELETEQUERIESPROC glDeleteQueries;
static PFNGLBEGINQUERYPROC glBeginQuery;
static PFNGLENDQUERYPROC glEndQuery;
static PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;
static PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v;

static void load_gl(void **ptr, const char *name) {
  *ptr = stygian_window_gl_get_proc_address(name);
}
#define LOAD_GL(fn) load_gl((void **)&fn, #fn)

// ============================================================================
// Access Point Structure
// ============================================================================

struct StygianAP {
  StygianWindow *window;
  uint32_t max_elements;
  StygianAllocator *allocator;

  void *gl_context;

  // GPU resources
  GLuint clip_ssbo;
  GLuint vao;
  GLuint vbo;
  GLuint program;

  // Uniform locations
  GLint loc_screen_size;
  GLint loc_font_tex;
  GLint loc_image_tex;
  GLint loc_atlas_size;
  GLint loc_px_range;
  GLint loc_output_transform_enabled;
  GLint loc_output_matrix;
  GLint loc_output_src_srgb;
  GLint loc_output_src_gamma;
  GLint loc_output_dst_srgb;
  GLint loc_output_dst_gamma;

  // State
  uint32_t element_count;
  bool initialized;
  StygianAPAdapterClass adapter_class;
  bool output_color_transform_enabled;
  float output_color_matrix[9];
  bool output_src_srgb_transfer;
  float output_src_gamma;
  bool output_dst_srgb_transfer;
  float output_dst_gamma;

  // Shader paths for hot reload
  char shader_dir[256];

  // Shader file modification times for auto-reload
  uint64_t shader_load_time; // Time when shaders were last loaded

  uint32_t last_upload_bytes;
  uint32_t last_upload_ranges;
  float last_gpu_ms;
  GLuint gpu_queries[2];
  uint8_t gpu_query_index;
  bool gpu_query_initialized;
  bool gpu_query_in_flight;

  // SoA SSBOs (3 buffers: hot, appearance, effects)
  GLuint soa_ssbo_hot;
  GLuint soa_ssbo_appearance;
  GLuint soa_ssbo_effects;
  // GPU-side version tracking per chunk
  uint32_t *gpu_hot_versions;
  uint32_t *gpu_appearance_versions;
  uint32_t *gpu_effects_versions;
  uint32_t soa_chunk_count;

  // Remapped hot stream submitted to GPU (texture handles -> sampler slots).
  StygianSoAHot *submit_hot;
};

#define STYGIAN_GL_IMAGE_SAMPLERS 16
#define STYGIAN_GL_IMAGE_UNIT_BASE 2 // units 0,1 reserved for font atlas etc.

// Safe string copy: deterministic, no printf overhead, always NUL-terminates.
// copy_cstr removed — use stygian_cpystr from stygian_internal.h

// Allocator helpers: use AP allocator when set, else CRT (bootstrap/fallback)
static void *ap_alloc(StygianAP *ap, size_t size, size_t alignment) {
  if (ap->allocator && ap->allocator->alloc)
    return ap->allocator->alloc(ap->allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void ap_free(StygianAP *ap, void *ptr) {
  if (!ptr)
    return;
  if (ap->allocator && ap->allocator->free)
    ap->allocator->free(ap->allocator, ptr);
  else
    free(ptr);
}

// Config-based allocator helpers for bootstrap (before AP struct exists)
static void *cfg_alloc(StygianAllocator *allocator, size_t size,
                       size_t alignment) {
  if (allocator && allocator->alloc)
    return allocator->alloc(allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void cfg_free(StygianAllocator *allocator, void *ptr) {
  if (!ptr)
    return;
  if (allocator && allocator->free)
    allocator->free(allocator, ptr);
  else
    free(ptr);
}

static bool contains_nocase(const char *haystack, const char *needle) {
  size_t nlen;
  const char *p;
  if (!haystack || !needle)
    return false;
  nlen = strlen(needle);
  if (nlen == 0)
    return false;
  for (p = haystack; *p; p++) {
#ifdef _WIN32
    if (_strnicmp(p, needle, nlen) == 0)
      return true;
#else
    if (strncasecmp(p, needle, nlen) == 0)
      return true;
#endif
  }
  return false;
}

static StygianAPAdapterClass classify_renderer(const char *renderer) {
  if (!renderer || !renderer[0])
    return STYGIAN_AP_ADAPTER_UNKNOWN;
  if (contains_nocase(renderer, "intel") || contains_nocase(renderer, "iris") ||
      contains_nocase(renderer, "uhd")) {
    return STYGIAN_AP_ADAPTER_IGPU;
  }
  if (contains_nocase(renderer, "nvidia") ||
      contains_nocase(renderer, "geforce") ||
      contains_nocase(renderer, "radeon") || contains_nocase(renderer, "rtx") ||
      contains_nocase(renderer, "gtx")) {
    return STYGIAN_AP_ADAPTER_DGPU;
  }
  return STYGIAN_AP_ADAPTER_UNKNOWN;
}

// ============================================================================
// File Modification Time (for shader hot-reload)
// ============================================================================

// Portable file modification time
#ifdef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// Get file modification time as uint64 (0 on error)
static uint64_t get_file_mod_time(const char *path) {
#ifdef _WIN32
  struct _stat st;
  if (_stat(path, &st) == 0) {
    return (uint64_t)st.st_mtime;
  }
#else
  struct stat st;
  if (stat(path, &st) == 0) {
    return (uint64_t)st.st_mtime;
  }
#endif
  return 0;
}

// Get newest modification time of all shader files
static uint64_t get_shader_newest_mod_time(const char *shader_dir) {
  static const char *shader_files[] = {"stygian.vert",    "stygian.frag",
                                       "sdf_common.glsl", "window.glsl",
                                       "ui.glsl",         "text.glsl"};

  uint64_t newest = 0;
  char path[512];

  for (int i = 0; i < 6; i++) {
    snprintf(path, sizeof(path), "%s/%s", shader_dir, shader_files[i]);
    uint64_t mod_time = get_file_mod_time(path);
    if (mod_time > newest)
      newest = mod_time;
  }

  return newest;
}

// ============================================================================
// Shader Compilation
// ============================================================================

static GLuint compile_shader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    printf("[Stygian AP] Shader compile error:\n%s\n", log);
    return 0;
  }
  return shader;
}

// Load preprocessed shader file from build/ subdirectory
// Shaders are preprocessed by shaderc (glslc -E) to resolve #includes
static char *load_shader_file(StygianAP *ap, const char *filename) {
  char path[512];
  snprintf(path, sizeof(path), "%s/build/%s.glsl", ap->shader_dir, filename);

  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("[Stygian AP] Shader not found at '%s', trying fallback...\n", path);
    snprintf(path, sizeof(path), "%s/%s", ap->shader_dir, filename);
    f = fopen(path, "rb");
  }
  if (!f) {
    printf("[Stygian AP] Failed to load shader '%s'\n", path);
    return NULL;
  }
  printf("[Stygian AP] Loaded shader: %s\n", path);

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *source = (char *)ap_alloc(ap, (size_t)size + 1u, 1);
  if (!source) {
    fclose(f);
    return NULL;
  }

  fread(source, 1, (size_t)size, f);
  source[size] = '\0';
  fclose(f);

  return source;
}

// Compile and link shader program, returns program handle or 0 on failure
// Does NOT modify ap->program - caller decides what to do with result
static GLuint compile_program_internal(
    StygianAP *ap, GLint *out_loc_screen_size, GLint *out_loc_font_tex,
    GLint *out_loc_image_tex, GLint *out_loc_atlas_size,
    GLint *out_loc_px_range, GLint *out_loc_output_transform_enabled,
    GLint *out_loc_output_matrix, GLint *out_loc_output_src_srgb,
    GLint *out_loc_output_src_gamma, GLint *out_loc_output_dst_srgb,
    GLint *out_loc_output_dst_gamma) {
  char *vert_src = load_shader_file(ap, "stygian.vert");
  if (!vert_src)
    return 0;

  char *frag_src = load_shader_file(ap, "stygian.frag");
  if (!frag_src) {
    ap_free(ap, vert_src);
    return 0;
  }

  GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);

  ap_free(ap, vert_src);
  ap_free(ap, frag_src);

  if (!vs || !fs) {
    if (vs)
      glDeleteShader(vs);
    if (fs)
      glDeleteShader(fs);
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  // Shaders can be deleted after linking
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint status;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (!status) {
    char log[4096];
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    printf("[Stygian AP] Program link error:\n%s\n", log);
    glDeleteProgram(program);
    return 0;
  }

  // Validate program
  glValidateProgram(program);
  glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
  if (!status) {
    char log[4096];
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    printf("[Stygian AP] Program validation warning:\n%s\n", log);
    // Don't fail on validation - some drivers are picky
  }

  // Get uniform locations
  if (out_loc_screen_size)
    *out_loc_screen_size = glGetUniformLocation(program, "uScreenSize");
  if (out_loc_font_tex)
    *out_loc_font_tex = glGetUniformLocation(program, "uFontTex");
  if (out_loc_image_tex)
    *out_loc_image_tex = glGetUniformLocation(program, "uImageTex[0]");
  if (out_loc_atlas_size)
    *out_loc_atlas_size = glGetUniformLocation(program, "uAtlasSize");
  if (out_loc_px_range)
    *out_loc_px_range = glGetUniformLocation(program, "uPxRange");
  if (out_loc_output_transform_enabled)
    *out_loc_output_transform_enabled =
        glGetUniformLocation(program, "uOutputColorTransformEnabled");
  if (out_loc_output_matrix)
    *out_loc_output_matrix =
        glGetUniformLocation(program, "uOutputColorMatrix");
  if (out_loc_output_src_srgb)
    *out_loc_output_src_srgb =
        glGetUniformLocation(program, "uOutputSrcIsSRGB");
  if (out_loc_output_src_gamma)
    *out_loc_output_src_gamma =
        glGetUniformLocation(program, "uOutputSrcGamma");
  if (out_loc_output_dst_srgb)
    *out_loc_output_dst_srgb =
        glGetUniformLocation(program, "uOutputDstIsSRGB");
  if (out_loc_output_dst_gamma)
    *out_loc_output_dst_gamma =
        glGetUniformLocation(program, "uOutputDstGamma");

  return program;
}

static bool create_program(StygianAP *ap) {
  GLuint program = compile_program_internal(
      ap, &ap->loc_screen_size, &ap->loc_font_tex, &ap->loc_image_tex,
      &ap->loc_atlas_size, &ap->loc_px_range, &ap->loc_output_transform_enabled,
      &ap->loc_output_matrix, &ap->loc_output_src_srgb,
      &ap->loc_output_src_gamma, &ap->loc_output_dst_srgb,
      &ap->loc_output_dst_gamma);

  if (!program)
    return false;

  ap->program = program;
  ap->shader_load_time = get_shader_newest_mod_time(ap->shader_dir);
  printf("[Stygian AP] Shaders loaded from: %s\n", ap->shader_dir);
  return true;
}

static void upload_output_color_transform_uniforms(StygianAP *ap) {
  if (!ap || !ap->program)
    return;
  if (ap->loc_output_transform_enabled >= 0) {
    glUniform1i(ap->loc_output_transform_enabled,
                ap->output_color_transform_enabled ? 1 : 0);
  }
  if (ap->loc_output_matrix >= 0 && glUniformMatrix3fv) {
    glUniformMatrix3fv(ap->loc_output_matrix, 1, GL_TRUE,
                       ap->output_color_matrix);
  }
  if (ap->loc_output_src_srgb >= 0) {
    glUniform1i(ap->loc_output_src_srgb, ap->output_src_srgb_transfer ? 1 : 0);
  }
  if (ap->loc_output_src_gamma >= 0) {
    glUniform1f(ap->loc_output_src_gamma, ap->output_src_gamma);
  }
  if (ap->loc_output_dst_srgb >= 0) {
    glUniform1i(ap->loc_output_dst_srgb, ap->output_dst_srgb_transfer ? 1 : 0);
  }
  if (ap->loc_output_dst_gamma >= 0) {
    glUniform1f(ap->loc_output_dst_gamma, ap->output_dst_gamma);
  }
}

// ============================================================================
// Lifecycle
// ============================================================================

StygianAP *stygian_ap_create(const StygianAPConfig *config) {
  if (!config || !config->window) {
    printf("[Stygian AP] Error: window required\n");
    return NULL;
  }

  StygianAP *ap = (StygianAP *)cfg_alloc(config->allocator, sizeof(StygianAP),
                                         _Alignof(StygianAP));
  if (!ap)
    return NULL;
  memset(ap, 0, sizeof(StygianAP));
  ap->allocator = config->allocator;
  ap->adapter_class = STYGIAN_AP_ADAPTER_UNKNOWN;

  ap->window = config->window;
  ap->max_elements = config->max_elements > 0 ? config->max_elements : 16384;
  ap->output_color_transform_enabled = false;
  ap->output_src_srgb_transfer = true;
  ap->output_dst_srgb_transfer = true;
  ap->output_src_gamma = 2.4f;
  ap->output_dst_gamma = 2.4f;
  memset(ap->output_color_matrix, 0, sizeof(ap->output_color_matrix));
  ap->output_color_matrix[0] = 1.0f;
  ap->output_color_matrix[4] = 1.0f;
  ap->output_color_matrix[8] = 1.0f;

  // Copy shader directory (already resolved by core)
  stygian_cpystr(ap->shader_dir, sizeof(ap->shader_dir),
                 (config->shader_dir && config->shader_dir[0])
                     ? config->shader_dir
                     : "shaders");

  if (!stygian_window_gl_set_pixel_format(config->window)) {
    printf("[Stygian AP] Failed to set pixel format\n");
    cfg_free(config->allocator, ap);
    return NULL;
  }

  ap->gl_context = stygian_window_gl_create_context(config->window, NULL);
  if (!ap->gl_context) {
    printf("[Stygian AP] Failed to create OpenGL context\n");
    cfg_free(config->allocator, ap);
    return NULL;
  }

  if (!stygian_window_gl_make_current(config->window, ap->gl_context)) {
    printf("[Stygian AP] Failed to make OpenGL context current\n");
    stygian_window_gl_destroy_context(ap->gl_context);
    cfg_free(config->allocator, ap);
    return NULL;
  }

  stygian_window_gl_set_vsync(config->window, true);
  printf("[Stygian AP] VSync enabled\n");

  // Load GL extensions
  LOAD_GL(glGenBuffers);
  LOAD_GL(glBindBuffer);
  LOAD_GL(glBufferData);
  LOAD_GL(glBufferSubData);
  LOAD_GL(glBindBufferBase);
  LOAD_GL(glCreateShader);
  LOAD_GL(glShaderSource);
  LOAD_GL(glCompileShader);
  LOAD_GL(glGetShaderiv);
  LOAD_GL(glGetShaderInfoLog);
  LOAD_GL(glCreateProgram);
  LOAD_GL(glAttachShader);
  LOAD_GL(glLinkProgram);
  LOAD_GL(glUseProgram);
  LOAD_GL(glGetProgramiv);
  LOAD_GL(glGetProgramInfoLog);
  LOAD_GL(glGetUniformLocation);
  LOAD_GL(glUniform1i);
  LOAD_GL(glUniform1iv);
  LOAD_GL(glUniform1f);
  LOAD_GL(glUniform2f);
  LOAD_GL(glUniformMatrix3fv);
  LOAD_GL(glEnableVertexAttribArray);
  LOAD_GL(glVertexAttribPointer);
  LOAD_GL(glGenVertexArrays);
  LOAD_GL(glBindVertexArray);
  LOAD_GL(glDrawArraysInstanced);
  LOAD_GL(glDrawArraysInstancedBaseInstance);
  LOAD_GL(glDeleteBuffers);
  LOAD_GL(glDeleteProgram);
  LOAD_GL(glActiveTexture);
  LOAD_GL(glDeleteShader);
  LOAD_GL(glValidateProgram);
  LOAD_GL(glGenQueries);
  LOAD_GL(glDeleteQueries);
  LOAD_GL(glBeginQuery);
  LOAD_GL(glEndQuery);
  LOAD_GL(glGetQueryObjectiv);
  LOAD_GL(glGetQueryObjectui64v);

  // Check GL version
  const char *version = (const char *)glGetString(GL_VERSION);
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  ap->adapter_class = classify_renderer(renderer);
  if (renderer && renderer[0]) {
    printf("[Stygian AP] Renderer: %s\n", renderer);
  }
  if (version) {
    int major = version[0] - '0';
    int minor = version[2] - '0';
    printf("[Stygian AP] OpenGL %d.%d detected\n", major, minor);
    if (major < 4 || (major == 4 && minor < 3)) {
      printf("[Stygian AP] Warning: OpenGL 4.3+ required for SSBO\n");
    }
  } else {
    printf("[Stygian AP] Warning: Could not get GL version\n");
  }

  // Create shader program
  if (!create_program(ap)) {
    stygian_ap_destroy(ap);
    return NULL;
  }

  // Create SSBO for clip rects (binding 3)
  glGenBuffers(1, &ap->clip_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->clip_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, STYGIAN_MAX_CLIPS * sizeof(float) * 4,
               NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ap->clip_ssbo);

  // Create SoA SSBOs (bindings 4, 5, 6)
  glGenBuffers(1, &ap->soa_ssbo_hot);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_hot);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ap->max_elements * sizeof(StygianSoAHot), NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ap->soa_ssbo_hot);

  glGenBuffers(1, &ap->soa_ssbo_appearance);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_appearance);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ap->max_elements * sizeof(StygianSoAAppearance), NULL,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ap->soa_ssbo_appearance);

  glGenBuffers(1, &ap->soa_ssbo_effects);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_effects);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               ap->max_elements * sizeof(StygianSoAEffects), NULL,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ap->soa_ssbo_effects);

  // Optional GPU timing queries (GL_TIME_ELAPSED).
  ap->gpu_query_initialized =
      (glGenQueries && glDeleteQueries && glBeginQuery && glEndQuery &&
       glGetQueryObjectiv && glGetQueryObjectui64v);
  ap->gpu_query_in_flight = false;
  ap->gpu_query_index = 0u;
  ap->last_gpu_ms = 0.0f;
  ap->gpu_queries[0] = 0u;
  ap->gpu_queries[1] = 0u;
  if (ap->gpu_query_initialized) {
    glGenQueries(2, ap->gpu_queries);
  }

  // Allocate GPU-side version tracking for SoA chunk upload
  // Default chunk_size 256 → chunk_count = ceil(max_elements / 256)
  {
    uint32_t cs = 256u;
    uint32_t cc = (ap->max_elements + cs - 1u) / cs;
    ap->soa_chunk_count = cc;
    ap->gpu_hot_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_appearance_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_effects_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    if (ap->gpu_hot_versions)
      memset(ap->gpu_hot_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_appearance_versions)
      memset(ap->gpu_appearance_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_effects_versions)
      memset(ap->gpu_effects_versions, 0, cc * sizeof(uint32_t));
  }

  ap->submit_hot = (StygianSoAHot *)ap_alloc(
      ap, (size_t)ap->max_elements * sizeof(StygianSoAHot),
      _Alignof(StygianSoAHot));
  if (!ap->submit_hot) {
    printf("[Stygian AP] Failed to allocate submit hot buffer\n");
    stygian_ap_destroy(ap);
    return NULL;
  }

  // Create VAO/VBO for quad vertices [-1, +1] range (shader uses aPos * 0.5 +
  // 0.5)
  float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
  glGenVertexArrays(1, &ap->vao);
  glBindVertexArray(ap->vao);

  glGenBuffers(1, &ap->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, ap->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  ap->initialized = true;
  return ap;
}

void stygian_ap_destroy(StygianAP *ap) {
  if (!ap)
    return;

  if (ap->clip_ssbo)
    glDeleteBuffers(1, &ap->clip_ssbo);
  if (ap->soa_ssbo_hot)
    glDeleteBuffers(1, &ap->soa_ssbo_hot);
  if (ap->soa_ssbo_appearance)
    glDeleteBuffers(1, &ap->soa_ssbo_appearance);
  if (ap->soa_ssbo_effects)
    glDeleteBuffers(1, &ap->soa_ssbo_effects);
  if (ap->gpu_query_initialized) {
    glDeleteQueries(2, ap->gpu_queries);
    ap->gpu_queries[0] = 0u;
    ap->gpu_queries[1] = 0u;
    ap->gpu_query_initialized = false;
    ap->gpu_query_in_flight = false;
  }
  if (ap->vbo)
    glDeleteBuffers(1, &ap->vbo);
  if (ap->program)
    glDeleteProgram(ap->program);
  ap_free(ap, ap->gpu_hot_versions);
  ap_free(ap, ap->gpu_appearance_versions);
  ap_free(ap, ap->gpu_effects_versions);
  ap_free(ap, ap->submit_hot);
  ap->gpu_hot_versions = NULL;
  ap->gpu_appearance_versions = NULL;
  ap->gpu_effects_versions = NULL;
  ap->submit_hot = NULL;

  if (ap->gl_context) {
    stygian_window_gl_destroy_context(ap->gl_context);
    ap->gl_context = NULL;
  }

  // Free AP struct via its own allocator
  StygianAllocator *allocator = ap->allocator;
  cfg_free(allocator, ap);
}

StygianAPAdapterClass stygian_ap_get_adapter_class(const StygianAP *ap) {
  if (!ap)
    return STYGIAN_AP_ADAPTER_UNKNOWN;
  return ap->adapter_class;
}

uint32_t stygian_ap_get_last_upload_bytes(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_bytes;
}

uint32_t stygian_ap_get_last_upload_ranges(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_ranges;
}

float stygian_ap_get_last_gpu_ms(const StygianAP *ap) {
  if (!ap)
    return 0.0f;
  return ap->last_gpu_ms;
}

void stygian_ap_gpu_timer_begin(StygianAP *ap) {
  if (!ap || !ap->gpu_query_initialized)
    return;
  if (ap->gpu_query_in_flight)
    return;
  glBeginQuery(GL_TIME_ELAPSED, ap->gpu_queries[ap->gpu_query_index]);
  ap->gpu_query_in_flight = true;
}

void stygian_ap_gpu_timer_end(StygianAP *ap) {
  uint8_t prev_index;
  GLint available = 0;
  uint64_t ns = 0;
  if (!ap || !ap->gpu_query_initialized)
    return;
  if (!ap->gpu_query_in_flight)
    return;
  glEndQuery(GL_TIME_ELAPSED);
  ap->gpu_query_in_flight = false;

  // Rotate and poll the previous query to avoid stalling the CPU on the GPU.
  prev_index = ap->gpu_query_index;
  ap->gpu_query_index = (uint8_t)((ap->gpu_query_index + 1u) & 1u);

  glGetQueryObjectiv(ap->gpu_queries[prev_index], GL_QUERY_RESULT_AVAILABLE,
                     &available);
  if (available) {
    glGetQueryObjectui64v(ap->gpu_queries[prev_index], GL_QUERY_RESULT, &ns);
    ap->last_gpu_ms = (float)((double)ns / 1000000.0);
  }
}

// ============================================================================
// Shader Hot Reload
// ============================================================================

bool stygian_ap_reload_shaders(StygianAP *ap) {
  if (!ap)
    return false;

  // Compile new program FIRST (do not touch ap->program yet)
  GLint new_loc_screen_size, new_loc_font_tex, new_loc_image_tex,
      new_loc_atlas_size, new_loc_px_range, new_loc_output_transform_enabled,
      new_loc_output_matrix, new_loc_output_src_srgb, new_loc_output_src_gamma,
      new_loc_output_dst_srgb, new_loc_output_dst_gamma;
  GLuint new_program = compile_program_internal(
      ap, &new_loc_screen_size, &new_loc_font_tex, &new_loc_image_tex,
      &new_loc_atlas_size, &new_loc_px_range, &new_loc_output_transform_enabled,
      &new_loc_output_matrix, &new_loc_output_src_srgb,
      &new_loc_output_src_gamma, &new_loc_output_dst_srgb,
      &new_loc_output_dst_gamma);

  if (!new_program) {
    // Compilation failed - keep old shader, no black screen!
    printf("[Stygian AP] Shader reload FAILED - keeping previous shader\n");
    return false;
  }

  // Success! Now safe to delete old program
  if (ap->program) {
    glDeleteProgram(ap->program);
  }

  // Swap in new program and uniform locations
  ap->program = new_program;
  ap->loc_screen_size = new_loc_screen_size;
  ap->loc_font_tex = new_loc_font_tex;
  ap->loc_image_tex = new_loc_image_tex;
  ap->loc_atlas_size = new_loc_atlas_size;
  ap->loc_px_range = new_loc_px_range;
  ap->loc_output_transform_enabled = new_loc_output_transform_enabled;
  ap->loc_output_matrix = new_loc_output_matrix;
  ap->loc_output_src_srgb = new_loc_output_src_srgb;
  ap->loc_output_src_gamma = new_loc_output_src_gamma;
  ap->loc_output_dst_srgb = new_loc_output_dst_srgb;
  ap->loc_output_dst_gamma = new_loc_output_dst_gamma;

  // Update load timestamp for hot-reload tracking
  ap->shader_load_time = get_shader_newest_mod_time(ap->shader_dir);
  glUseProgram(ap->program);
  upload_output_color_transform_uniforms(ap);

  printf("[Stygian AP] Shaders reloaded successfully\n");
  return true;
}

// Check if shader files have been modified since last load
bool stygian_ap_shaders_need_reload(StygianAP *ap) {
  if (!ap || !ap->shader_dir[0])
    return false;

  uint64_t newest = get_shader_newest_mod_time(ap->shader_dir);
  return newest > ap->shader_load_time;
}

// ============================================================================
// Frame Management
// ============================================================================

void stygian_ap_begin_frame(StygianAP *ap, int width, int height) {
  if (!ap)
    return;

  // Ensure the correct GL context is current for this frame.
  stygian_ap_make_current(ap);

  glViewport(0, 0, width, height);
  glClearColor(0.235f, 0.259f, 0.294f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(ap->program);
  glUniform2f(ap->loc_screen_size, (float)width, (float)height);
  glUniform1i(ap->loc_font_tex, 1);
  if (ap->loc_image_tex >= 0 && glUniform1iv) {
    GLint units[STYGIAN_GL_IMAGE_SAMPLERS];
    for (int i = 0; i < STYGIAN_GL_IMAGE_SAMPLERS; ++i) {
      units[i] = STYGIAN_GL_IMAGE_UNIT_BASE + i;
    }
    glUniform1iv(ap->loc_image_tex, STYGIAN_GL_IMAGE_SAMPLERS, units);
  }
  upload_output_color_transform_uniforms(ap);

  glBindVertexArray(ap->vao);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ap->clip_ssbo);
}

void stygian_ap_submit(StygianAP *ap, const StygianSoAHot *soa_hot,
                       uint32_t count) {
  if (!ap || !soa_hot || !ap->submit_hot || count == 0)
    return;

  if (count > ap->max_elements) {
    count = ap->max_elements;
  }

  ap->element_count = count;

  // Map GL texture handles to compact sampler indices [0..N-1].
  // Texture unit routing:
  //   unit 1: font atlas
  //   units 2..(2+N-1): image textures (STYGIAN_TEXTURE)
  uint32_t mapped_handles[STYGIAN_GL_IMAGE_SAMPLERS];
  uint32_t mapped_count = 0;

  for (uint32_t i = 0; i < count; ++i) {
    ap->submit_hot[i] = soa_hot[i];

    // Read type and texture_id directly from SoA hot
    // Note: type is packed with render_mode in upper 16 bits, but for checking
    // STYGIAN_TEXTURE we only care about the lower 16 bits (element type).
    uint32_t type = ap->submit_hot[i].type & 0xFFFF;
    uint32_t tex_id = ap->submit_hot[i].texture_id;

    if (type == STYGIAN_TEXTURE && tex_id != 0) {
      uint32_t slot = UINT32_MAX;
      for (uint32_t j = 0; j < mapped_count; ++j) {
        if (mapped_handles[j] == tex_id) {
          slot = j;
          break;
        }
      }

      if (slot == UINT32_MAX) {
        if (mapped_count < STYGIAN_GL_IMAGE_SAMPLERS) {
          slot = mapped_count;
          mapped_handles[mapped_count++] = tex_id;
        } else {
          slot = STYGIAN_GL_IMAGE_SAMPLERS;
        }
      }
      // Keep CPU source immutable; write remapped slot to submit stream only.
      ap->submit_hot[i].texture_id = slot;
    }
  }

  // Bind image textures to configured image units.
  for (uint32_t i = 0; i < mapped_count; ++i) {
    glActiveTexture(GL_TEXTURE0 + STYGIAN_GL_IMAGE_UNIT_BASE + i);
    glBindTexture(GL_TEXTURE_2D, (GLuint)mapped_handles[i]);
  }
}

// ============================================================================
// SoA Versioned Chunk Upload
// ============================================================================

void stygian_ap_submit_soa(StygianAP *ap, const StygianSoAHot *hot,
                           const StygianSoAAppearance *appearance,
                           const StygianSoAEffects *effects,
                           uint32_t element_count,
                           const StygianBufferChunk *chunks,
                           uint32_t chunk_count, uint32_t chunk_size) {
  if (!ap || !hot || !appearance || !effects || !chunks || element_count == 0)
    return;
  const StygianSoAHot *hot_src = ap->submit_hot ? ap->submit_hot : hot;

  ap->last_upload_bytes = 0u;
  ap->last_upload_ranges = 0u;

  // Ensure GPU version arrays are large enough
  if (chunk_count > ap->soa_chunk_count) {
    // Should not happen if config is consistent, but guard anyway
    chunk_count = ap->soa_chunk_count;
  }

  for (uint32_t ci = 0; ci < chunk_count; ci++) {
    const StygianBufferChunk *c = &chunks[ci];
    uint32_t base = ci * chunk_size;

    // --- Hot buffer ---
    if (ap->gpu_hot_versions && c->hot_version != ap->gpu_hot_versions[ci]) {
      // Determine upload range
      uint32_t dmin = c->hot_dirty_min;
      uint32_t dmax = c->hot_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count) {
          uint32_t range_count = abs_max - abs_min + 1;
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_hot);
          glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                          (intptr_t)abs_min * (intptr_t)sizeof(StygianSoAHot),
                          (intptr_t)range_count *
                              (intptr_t)sizeof(StygianSoAHot),
                          &hot_src[abs_min]);
          ap->last_upload_bytes += range_count * (uint32_t)sizeof(StygianSoAHot);
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_hot_versions[ci] = c->hot_version;
    }

    // --- Appearance buffer ---
    if (ap->gpu_appearance_versions &&
        c->appearance_version != ap->gpu_appearance_versions[ci]) {
      uint32_t dmin = c->appearance_dirty_min;
      uint32_t dmax = c->appearance_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count) {
          uint32_t range_count = abs_max - abs_min + 1;
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_appearance);
          glBufferSubData(
              GL_SHADER_STORAGE_BUFFER,
              (intptr_t)abs_min * (intptr_t)sizeof(StygianSoAAppearance),
              (intptr_t)range_count * (intptr_t)sizeof(StygianSoAAppearance),
              &appearance[abs_min]);
          ap->last_upload_bytes +=
              range_count * (uint32_t)sizeof(StygianSoAAppearance);
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_appearance_versions[ci] = c->appearance_version;
    }

    // --- Effects buffer ---
    if (ap->gpu_effects_versions &&
        c->effects_version != ap->gpu_effects_versions[ci]) {
      uint32_t dmin = c->effects_dirty_min;
      uint32_t dmax = c->effects_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count) {
          uint32_t range_count = abs_max - abs_min + 1;
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->soa_ssbo_effects);
          glBufferSubData(
              GL_SHADER_STORAGE_BUFFER,
              (intptr_t)abs_min * (intptr_t)sizeof(StygianSoAEffects),
              (intptr_t)range_count * (intptr_t)sizeof(StygianSoAEffects),
              &effects[abs_min]);
          ap->last_upload_bytes +=
              range_count * (uint32_t)sizeof(StygianSoAEffects);
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_effects_versions[ci] = c->effects_version;
    }
  }
}

void stygian_ap_draw(StygianAP *ap) {
  if (!ap || ap->element_count == 0)
    return;
  stygian_ap_draw_range(ap, 0u, ap->element_count);
}

void stygian_ap_draw_range(StygianAP *ap, uint32_t first_instance,
                           uint32_t instance_count) {
  if (!ap || instance_count == 0)
    return;
  if (glDrawArraysInstancedBaseInstance) {
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, instance_count,
                                      first_instance);
    return;
  }
  if (first_instance != 0u) {
    // This should be unavailable only on very old GL drivers.
    // Fall back to full draw to preserve visibility over perfect layering.
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, ap->element_count);
    return;
  }
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instance_count);
}

void stygian_ap_end_frame(StygianAP *ap) {
  if (!ap)
    return;
}

void stygian_ap_set_clips(StygianAP *ap, const float *clips, uint32_t count) {
  if (!ap || !ap->clip_ssbo || !clips || count == 0)
    return;
  if (count > STYGIAN_MAX_CLIPS)
    count = STYGIAN_MAX_CLIPS;

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ap->clip_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(float) * 4,
                  clips);
}

void stygian_ap_swap(StygianAP *ap) {
  if (!ap)
    return;
  stygian_window_gl_swap_buffers(ap->window);
}

// ============================================================================
// Textures
// ============================================================================

StygianAPTexture stygian_ap_texture_create(StygianAP *ap, int w, int h,
                                           const void *rgba) {
  if (!ap)
    return 0;

  GLuint tex;
  // Keep font sampler binding (unit 1) intact by creating textures on unit 0.
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               rgba);

  return (StygianAPTexture)tex;
}

bool stygian_ap_texture_update(StygianAP *ap, StygianAPTexture tex, int x,
                               int y, int w, int h, const void *rgba) {
  if (!ap || !tex || !rgba || w <= 0 || h <= 0)
    return false;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                  rgba);
  return true;
}

void stygian_ap_texture_destroy(StygianAP *ap, StygianAPTexture tex) {
  if (!ap || !tex)
    return;
  GLuint id = (GLuint)tex;
  glDeleteTextures(1, &id);
}

void stygian_ap_texture_bind(StygianAP *ap, StygianAPTexture tex,
                             uint32_t slot) {
  if (!ap)
    return;
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
}

// ============================================================================
// Uniforms
// ============================================================================

void stygian_ap_set_font_texture(StygianAP *ap, StygianAPTexture tex,
                                 int atlas_w, int atlas_h, float px_range) {
  if (!ap)
    return;

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);

  glUseProgram(ap->program);
  glUniform2f(ap->loc_atlas_size, (float)atlas_w, (float)atlas_h);
  glUniform1f(ap->loc_px_range, px_range);
}

void stygian_ap_set_output_color_transform(
    StygianAP *ap, bool enabled, const float *rgb3x3, bool src_srgb_transfer,
    float src_gamma, bool dst_srgb_transfer, float dst_gamma) {
  static const float identity[9] = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  if (!ap)
    return;
  ap->output_color_transform_enabled = enabled;
  memcpy(ap->output_color_matrix, rgb3x3 ? rgb3x3 : identity,
         sizeof(ap->output_color_matrix));
  ap->output_src_srgb_transfer = src_srgb_transfer;
  ap->output_dst_srgb_transfer = dst_srgb_transfer;
  ap->output_src_gamma = (src_gamma > 0.0f) ? src_gamma : 2.2f;
  ap->output_dst_gamma = (dst_gamma > 0.0f) ? dst_gamma : 2.2f;
  if (!ap->program)
    return;
  glUseProgram(ap->program);
  upload_output_color_transform_uniforms(ap);
}

// ============================================================================
// Multi-Surface Support (Floating Windows)
// ============================================================================

struct StygianAPSurface {
  StygianWindow *window;
  int width;
  int height;
};

StygianAPSurface *stygian_ap_surface_create(StygianAP *ap,
                                            StygianWindow *window) {
  if (!ap || !window)
    return NULL;

  StygianAPSurface *surf = (StygianAPSurface *)ap_alloc(
      ap, sizeof(StygianAPSurface), _Alignof(StygianAPSurface));
  if (!surf)
    return NULL;
  memset(surf, 0, sizeof(StygianAPSurface));

  surf->window = window;

  if (!stygian_window_gl_set_pixel_format(window)) {
    printf("[Stygian AP GL] Failed to set pixel format for surface\n");
    ap_free(ap, surf);
    return NULL;
  }

  printf("[Stygian AP GL] Surface created\n");
  return surf;
}

void stygian_ap_surface_destroy(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface)
    return;

  // HDC is released automatically when window is destroyed?
  // Actually ReleaseDC is needed if GetDC was called.
  // stygian_window_native_context might return a persistent DC or temp.
  // Assuming persistent for now or handled by window class.

  ap_free(ap, surface);
}

void stygian_ap_surface_begin(StygianAP *ap, StygianAPSurface *surface,
                              int width, int height) {
  if (!ap || !surface)
    return;

  surface->width = width;
  surface->height = height;

  if (!stygian_window_gl_make_current(surface->window, ap->gl_context)) {
    printf("[Stygian AP GL] Failed to make surface current\n");
    return;
  }

  glViewport(0, 0, width, height);
  glClearColor(0.235f, 0.259f, 0.294f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(ap->program);

  // Use LOGICAL size for projection if we want to match layout
  // But surface_begin receives 'width' which might be Physical or Logical
  // depending on caller. In dock_impl we pass Physical.
  // So we should query logical size from window to be safe?
  int log_w, log_h;
  if (surface->window) {
    stygian_window_get_size(surface->window, &log_w, &log_h);
  } else {
    log_w = width;
    log_h = height;
  }

  glUniform2f(ap->loc_screen_size, (float)log_w, (float)log_h);
  glUniform1i(ap->loc_font_tex, 1);
  if (ap->loc_image_tex >= 0 && glUniform1iv) {
    GLint units[STYGIAN_GL_IMAGE_SAMPLERS];
    for (int i = 0; i < STYGIAN_GL_IMAGE_SAMPLERS; ++i) {
      units[i] = STYGIAN_GL_IMAGE_UNIT_BASE + i;
    }
    glUniform1iv(ap->loc_image_tex, STYGIAN_GL_IMAGE_SAMPLERS, units);
  }
  upload_output_color_transform_uniforms(ap);

  glBindVertexArray(ap->vao);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ap->soa_ssbo_hot);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ap->soa_ssbo_appearance);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ap->soa_ssbo_effects);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ap->clip_ssbo);
}

void stygian_ap_surface_submit(StygianAP *ap, StygianAPSurface *surface,
                               const StygianSoAHot *soa_hot, uint32_t count) {
  // Reuse main submit logic but targeting current context
  // We just need to upload to SSBO (shared context!)
  stygian_ap_submit(ap, soa_hot, count);
  stygian_ap_draw(ap);
  stygian_ap_end_frame(ap);
}

void stygian_ap_surface_end(StygianAP *ap, StygianAPSurface *surface) {
  // Nothing specific needed for GL if submitted
  (void)ap;
  (void)surface;
}

void stygian_ap_surface_swap(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface)
    return;

  stygian_window_gl_swap_buffers(surface->window);

  // Restore main context? Not strictly necessary if next begin switches it
  // back.
}

void stygian_ap_make_current(StygianAP *ap) {
  if (!ap)
    return;

  if (!stygian_window_gl_make_current(ap->window, ap->gl_context)) {
    printf("[Stygian AP GL] Failed to restore main context\n");
  }
}

void stygian_ap_set_viewport(StygianAP *ap, int width, int height) {
  if (!ap)
    return;
  glViewport(0, 0, width, height);

  // Restore projection uniform to match main window's logical size
  // This is critical when switching back from a floating window (which changed
  // the uniform)
  if (ap->window) {
    int log_w, log_h;
    stygian_window_get_size(ap->window, &log_w, &log_h);
    glUniform2f(ap->loc_screen_size, (float)log_w, (float)log_h);
  } else {
    glUniform2f(ap->loc_screen_size, (float)width, (float)height);
  }
}


# stygian_ap_vk.c
// stygian_ap_vk.c - Vulkan 1.0+ Access Point Implementation
// Part of Stygian UI Library
// DISCIPLINE: Only GPU operations. No layout, no fonts, no hit testing.

#include "../include/stygian.h"
#include "../include/stygian_memory.h"
#include "../src/stygian_internal.h" // stygian_cpystr
#include "../window/stygian_window.h"
#include "stygian_ap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vulkan/vulkan.h>

#define STYGIAN_VK_IMAGE_SAMPLERS 16
#define STYGIAN_VK_MAX_SWAPCHAIN_IMAGES 3
#define STYGIAN_VK_FRAMES_IN_FLIGHT 3

// ============================================================================
// Vulkan Access Point Structure
// ============================================================================

struct StygianAP {
  // Vulkan core
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  uint32_t graphics_family;

  // Swapchain
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkImage swapchain_images[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer framebuffers[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  uint32_t swapchain_image_count;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;

  // Render pass & pipeline
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline graphics_pipeline;

  // Resources
  VkBuffer clip_ssbo;
  VkDeviceMemory clip_ssbo_memory;
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_memory;

  // Descriptors
  VkDescriptorSetLayout descriptor_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;

  // Command buffers
  VkCommandPool command_pool;
  VkCommandBuffer command_buffers[STYGIAN_VK_FRAMES_IN_FLIGHT];

  // Synchronization
  VkSemaphore image_available[STYGIAN_VK_FRAMES_IN_FLIGHT];
  VkSemaphore render_finished[STYGIAN_VK_FRAMES_IN_FLIGHT];
  VkFence in_flight[STYGIAN_VK_FRAMES_IN_FLIGHT];
  VkFence image_in_flight[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  uint32_t current_frame;
  uint32_t current_image;
  bool frame_active;
  bool swapchain_needs_recreate;
  int resize_pending_w;
  int resize_pending_h;
  uint32_t resize_stable_count;
  uint32_t resize_debounce_frames;
  uint32_t resize_suboptimal_count;
  uint32_t resize_suboptimal_threshold;
  bool resize_telemetry_enabled;
  uint32_t resize_telemetry_period;
  uint32_t resize_telemetry_frames;
  uint32_t resize_telemetry_recreate_count;
  double resize_telemetry_acquire_ms;
  double resize_telemetry_submit_ms;
  double resize_telemetry_present_ms;
  double resize_telemetry_recreate_ms;

  // Shader modules
  VkShaderModule vert_module;
  VkShaderModule frag_module;

  // Font texture (placeholder for now)
  VkImage font_image;
  VkDeviceMemory font_memory;
  VkImageView font_view;
  VkSampler font_sampler;

  // Config
  char shader_dir[256];
  uint32_t max_elements;
  uint32_t element_count;
  StygianAllocator *allocator;
  uint32_t last_upload_bytes;
  uint32_t last_upload_ranges;
  StygianWindow *window;
  bool initialized;
  StygianAPAdapterClass adapter_class;
  float atlas_width;
  float atlas_height;
  float px_range;
  bool output_color_transform_enabled;
  float output_color_matrix[9];
  bool output_src_srgb_transfer;
  float output_src_gamma;
  bool output_dst_srgb_transfer;
  float output_dst_gamma;

  // SoA SSBOs (bindings 4/5/6 — mirrors GL backend)
  VkBuffer soa_hot_buf;
  VkDeviceMemory soa_hot_mem;
  VkBuffer soa_appearance_buf;
  VkDeviceMemory soa_appearance_mem;
  VkBuffer soa_effects_buf;
  VkDeviceMemory soa_effects_mem;

  // Per-chunk GPU version tracking (for dirty range upload)
  uint32_t *gpu_hot_versions;
  uint32_t *gpu_appearance_versions;
  uint32_t *gpu_effects_versions;
  uint32_t soa_chunk_count;

  // Main surface (embedded for the primary window)
  struct StygianAPSurface *main_surface;
};

// ============================================================================
// Per-Window Surface (for multi-window support)
// ============================================================================

struct StygianAPSurface {
  StygianAP *ap;         // Parent AP (shared resources)
  StygianWindow *window; // Associated window

  // Vulkan surface resources
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkImage swapchain_images[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer framebuffers[STYGIAN_VK_MAX_SWAPCHAIN_IMAGES];
  uint32_t image_count;
  VkFormat format;
  VkExtent2D extent;

  // Frame state
  uint32_t current_image;
  VkSemaphore image_available;
  VkSemaphore render_finished;
  VkFence in_flight;
  VkCommandBuffer command_buffer;
  bool frame_active;
  bool swapchain_needs_recreate;
  int resize_pending_w;
  int resize_pending_h;
  uint32_t resize_stable_count;
  uint32_t resize_debounce_frames;
  uint32_t resize_suboptimal_count;
};

typedef struct StygianVKPushConstants {
  float screen_atlas[4];   // x=screen w, y=screen h, z=atlas w, w=atlas h
  float px_range_flags[4]; // x=px range, y=enabled, z=src sRGB, w=dst sRGB
  float output_row0[4];
  float output_row1[4];
  float output_row2[4];
  float gamma[4]; // x=src gamma, y=dst gamma
} StygianVKPushConstants;

// Forward declaration (used by create() error path).
void stygian_ap_destroy(StygianAP *ap);

// Allocator helpers: use AP allocator when set, else CRT (bootstrap/fallback)
static void *ap_alloc(StygianAP *ap, size_t size, size_t alignment) {
  if (ap->allocator && ap->allocator->alloc)
    return ap->allocator->alloc(ap->allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void ap_free(StygianAP *ap, void *ptr) {
  if (!ptr)
    return;
  if (ap->allocator && ap->allocator->free)
    ap->allocator->free(ap->allocator, ptr);
  else
    free(ptr);
}

// Config-based allocator helpers for bootstrap (before AP struct exists)
static void *cfg_alloc(StygianAllocator *allocator, size_t size,
                       size_t alignment) {
  if (allocator && allocator->alloc)
    return allocator->alloc(allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void cfg_free(StygianAllocator *allocator, void *ptr) {
  if (!ptr)
    return;
  if (allocator && allocator->free)
    allocator->free(allocator, ptr);
  else
    free(ptr);
}

// copy_cstr removed — use stygian_cpystr from stygian_internal.h

static double stygian_vk_now_ms(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void fill_push_constants(const StygianAP *ap, float screen_w,
                                float screen_h,
                                StygianVKPushConstants *out_pc) {
  if (!ap || !out_pc)
    return;
  memset(out_pc, 0, sizeof(*out_pc));
  out_pc->screen_atlas[0] = screen_w;
  out_pc->screen_atlas[1] = screen_h;
  out_pc->screen_atlas[2] = ap->atlas_width;
  out_pc->screen_atlas[3] = ap->atlas_height;
  out_pc->px_range_flags[0] = ap->px_range;
  out_pc->px_range_flags[1] = ap->output_color_transform_enabled ? 1.0f : 0.0f;
  out_pc->px_range_flags[2] = ap->output_src_srgb_transfer ? 1.0f : 0.0f;
  out_pc->px_range_flags[3] = ap->output_dst_srgb_transfer ? 1.0f : 0.0f;

  out_pc->output_row0[0] = ap->output_color_matrix[0];
  out_pc->output_row0[1] = ap->output_color_matrix[1];
  out_pc->output_row0[2] = ap->output_color_matrix[2];
  out_pc->output_row1[0] = ap->output_color_matrix[3];
  out_pc->output_row1[1] = ap->output_color_matrix[4];
  out_pc->output_row1[2] = ap->output_color_matrix[5];
  out_pc->output_row2[0] = ap->output_color_matrix[6];
  out_pc->output_row2[1] = ap->output_color_matrix[7];
  out_pc->output_row2[2] = ap->output_color_matrix[8];
  out_pc->gamma[0] = ap->output_src_gamma;
  out_pc->gamma[1] = ap->output_dst_gamma;
}

static void update_image_sampler_array(StygianAP *ap) {
  if (!ap || ap->font_sampler == VK_NULL_HANDLE ||
      ap->font_view == VK_NULL_HANDLE)
    return;

  VkDescriptorImageInfo image_infos[STYGIAN_VK_IMAGE_SAMPLERS];
  for (uint32_t i = 0; i < STYGIAN_VK_IMAGE_SAMPLERS; ++i) {
    image_infos[i].sampler = ap->font_sampler;
    image_infos[i].imageView = ap->font_view;
    image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  VkWriteDescriptorSet image_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = ap->descriptor_set,
      .dstBinding = 2,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = STYGIAN_VK_IMAGE_SAMPLERS,
      .pImageInfo = image_infos,
  };
  vkUpdateDescriptorSets(ap->device, 1, &image_write, 0, NULL);
}

// ============================================================================
// Helper Functions
// ============================================================================

static VkShaderModule load_shader_module(StygianAP *ap, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("[Stygian AP VK] Failed to open shader: %s\n", path);
    return VK_NULL_HANDLE;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint32_t *code = (uint32_t *)ap_alloc(ap, size, _Alignof(uint32_t));
  fread(code, 1, size, f);
  fclose(f);

  VkShaderModuleCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = code,
  };

  VkShaderModule module;
  VkResult result =
      vkCreateShaderModule(ap->device, &create_info, NULL, &module);
  ap_free(ap, code);

  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create shader module: %d\n", result);
    return VK_NULL_HANDLE;
  }

  return module;
}

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  return UINT32_MAX;
}

// ============================================================================
// Vulkan Initialization
// ============================================================================

static bool create_instance(StygianAP *ap) {
  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Stygian UI",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Stygian",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  const char *extensions[8] = {0};
  uint32_t ext_count = stygian_window_vk_get_instance_extensions(extensions, 8);

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = ext_count,
      .ppEnabledExtensionNames = extensions,
  };

  VkResult result = vkCreateInstance(&create_info, NULL, &ap->instance);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create instance: %d\n", result);
    return false;
  }

  printf("[Stygian AP VK] Instance created\n");
  return true;
}

static bool pick_physical_device(StygianAP *ap) {
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(ap->instance, &device_count, NULL);

  if (device_count == 0) {
    printf("[Stygian AP VK] No Vulkan devices found\n");
    return false;
  }

  VkPhysicalDevice devices[8];
  if (device_count > 8)
    device_count = 8;
  vkEnumeratePhysicalDevices(ap->instance, &device_count, devices);

  // Pick first discrete GPU, or fallback to first device
  ap->physical_device = devices[0];
  ap->adapter_class = STYGIAN_AP_ADAPTER_UNKNOWN;
  for (uint32_t i = 0; i < device_count; i++) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(devices[i], &props);

    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      ap->physical_device = devices[i];
      ap->adapter_class = STYGIAN_AP_ADAPTER_DGPU;
      printf("[Stygian AP VK] Selected GPU: %s\n", props.deviceName);
      break;
    }
  }

  if (ap->adapter_class == STYGIAN_AP_ADAPTER_UNKNOWN) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ap->physical_device, &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      ap->adapter_class = STYGIAN_AP_ADAPTER_IGPU;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      ap->adapter_class = STYGIAN_AP_ADAPTER_DGPU;
    }
  }

  return true;
}

static bool find_queue_families(StygianAP *ap) {
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(ap->physical_device,
                                           &queue_family_count, NULL);

  VkQueueFamilyProperties queue_families[16];
  if (queue_family_count > 16)
    queue_family_count = 16;
  vkGetPhysicalDeviceQueueFamilyProperties(ap->physical_device,
                                           &queue_family_count, queue_families);

  // Find graphics queue family
  for (uint32_t i = 0; i < queue_family_count; i++) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      ap->graphics_family = i;
      return true;
    }
  }

  printf("[Stygian AP VK] No graphics queue family found\n");
  return false;
}

static bool create_logical_device(StygianAP *ap) {
  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = ap->graphics_family,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
  };

  const char *device_extensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  VkPhysicalDeviceFeatures device_features = {0};

  VkDeviceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_create_info,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = device_extensions,
      .pEnabledFeatures = &device_features,
  };

  VkResult result =
      vkCreateDevice(ap->physical_device, &create_info, NULL, &ap->device);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create logical device: %d\n", result);
    return false;
  }

  vkGetDeviceQueue(ap->device, ap->graphics_family, 0, &ap->graphics_queue);
  printf("[Stygian AP VK] Logical device created\n");
  return true;
}

static bool create_surface(StygianAP *ap) {
  if (!stygian_window_vk_create_surface(ap->window, ap->instance,
                                        (void **)&ap->surface)) {
    printf("[Stygian AP VK] Failed to create surface\n");
    return false;
  }

  // Verify surface support for our queue family
  VkBool32 supported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(ap->physical_device, ap->graphics_family,
                                       ap->surface, &supported);
  if (!supported) {
    printf("[Stygian AP VK] Surface not supported by queue family\n");
    return false;
  }

  printf("[Stygian AP VK] Surface created\n");
  return true;
}

static bool create_swapchain(StygianAP *ap, int width, int height,
                             VkSwapchainKHR old_swapchain) {
  // Query surface capabilities
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ap->physical_device, ap->surface,
                                            &capabilities);

  // Query surface formats
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, ap->surface,
                                       &format_count, NULL);
  VkSurfaceFormatKHR formats[16];
  if (format_count > 16)
    format_count = 16;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, ap->surface,
                                       &format_count, formats);

  // Pick format (prefer BGRA8 UNORM for linear colors like OpenGL)
  VkSurfaceFormatKHR surface_format = formats[0];
  for (uint32_t i = 0; i < format_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = formats[i];
      break;
    }
  }

  // Query present modes
  uint32_t present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(ap->physical_device, ap->surface,
                                            &present_mode_count, NULL);
  VkPresentModeKHR present_modes[8];
  if (present_mode_count > 8)
    present_mode_count = 8;
  vkGetPhysicalDeviceSurfacePresentModesKHR(ap->physical_device, ap->surface,
                                            &present_mode_count, present_modes);

  // Pick present mode (prefer FIFO for vsync)
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

  // Determine extent
  VkExtent2D extent;
  if (capabilities.currentExtent.width != UINT32_MAX) {
    extent = capabilities.currentExtent;
  } else {
    extent.width = width;
    extent.height = height;

    // Clamp to min/max
    if (extent.width < capabilities.minImageExtent.width)
      extent.width = capabilities.minImageExtent.width;
    if (extent.width > capabilities.maxImageExtent.width)
      extent.width = capabilities.maxImageExtent.width;
    if (extent.height < capabilities.minImageExtent.height)
      extent.height = capabilities.minImageExtent.height;
    if (extent.height > capabilities.maxImageExtent.height)
      extent.height = capabilities.maxImageExtent.height;
  }

  // Determine image count (triple buffering)
  uint32_t image_count = 3;
  if (capabilities.maxImageCount > 0 &&
      image_count > capabilities.maxImageCount)
    image_count = capabilities.maxImageCount;
  if (image_count < capabilities.minImageCount)
    image_count = capabilities.minImageCount;

  VkSwapchainCreateInfoKHR create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = ap->surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = old_swapchain,
  };

  VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
  VkResult result =
      vkCreateSwapchainKHR(ap->device, &create_info, NULL, &new_swapchain);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create swapchain: %d\n", result);
    return false;
  }

  // Get swapchain images
  vkGetSwapchainImagesKHR(ap->device, new_swapchain, &ap->swapchain_image_count,
                          NULL);
  if (ap->swapchain_image_count > STYGIAN_VK_MAX_SWAPCHAIN_IMAGES)
    ap->swapchain_image_count = STYGIAN_VK_MAX_SWAPCHAIN_IMAGES;
  vkGetSwapchainImagesKHR(ap->device, new_swapchain, &ap->swapchain_image_count,
                          ap->swapchain_images);

  ap->swapchain_format = surface_format.format;
  ap->swapchain_extent = extent;
  ap->swapchain = new_swapchain;

  printf("[Stygian AP VK] Swapchain created: %dx%d, %d images\n", extent.width,
         extent.height, ap->swapchain_image_count);
  return true;
}

static bool create_image_views(StygianAP *ap) {
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ap->swapchain_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = ap->swapchain_format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkResult result = vkCreateImageView(ap->device, &create_info, NULL,
                                        &ap->swapchain_views[i]);
    if (result != VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create image view %d: %d\n", i, result);
      return false;
    }
  }

  printf("[Stygian AP VK] Image views created\n");
  return true;
}

static bool create_render_pass(StygianAP *ap) {
  VkAttachmentDescription color_attachment = {
      .format = ap->swapchain_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency,
  };

  VkResult result =
      vkCreateRenderPass(ap->device, &render_pass_info, NULL, &ap->render_pass);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create render pass: %d\n", result);
    return false;
  }

  printf("[Stygian AP VK] Render pass created\n");
  return true;
}

static bool create_framebuffers(StygianAP *ap) {
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    VkImageView attachments[] = {ap->swapchain_views[i]};

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ap->render_pass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = ap->swapchain_extent.width,
        .height = ap->swapchain_extent.height,
        .layers = 1,
    };

    VkResult result = vkCreateFramebuffer(ap->device, &framebuffer_info, NULL,
                                          &ap->framebuffers[i]);
    if (result != VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create framebuffer %d: %d\n", i,
             result);
      return false;
    }
  }

  printf("[Stygian AP VK] Framebuffers created\n");
  return true;
}

static void cleanup_main_swapchain_attachments(StygianAP *ap) {
  if (!ap || !ap->device)
    return;

  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    if (ap->framebuffers[i]) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
      ap->framebuffers[i] = VK_NULL_HANDLE;
    }
    if (ap->swapchain_views[i]) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
      ap->swapchain_views[i] = VK_NULL_HANDLE;
    }
  }

  ap->swapchain_image_count = 0;
}

static void cleanup_main_swapchain_resources(StygianAP *ap) {
  if (!ap || !ap->device)
    return;
  cleanup_main_swapchain_attachments(ap);
  if (ap->swapchain) {
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    ap->swapchain = VK_NULL_HANDLE;
  }
}

static bool recreate_main_swapchain(StygianAP *ap, int width, int height) {
  if (!ap || !ap->device)
    return false;
  double t0 = stygian_vk_now_ms();

  if (width <= 0 || height <= 0) {
    stygian_window_get_framebuffer_size(ap->window, &width, &height);
    if (width <= 0 || height <= 0) {
      return false; // Minimized window, skip recreate for now.
    }
  }

  // Wait for in-flight frames instead of idling the whole queue.
  vkWaitForFences(ap->device, STYGIAN_VK_FRAMES_IN_FLIGHT, ap->in_flight,
                  VK_TRUE, UINT64_MAX);
  VkSwapchainKHR old_swapchain = ap->swapchain;
  cleanup_main_swapchain_attachments(ap);

  if (!create_swapchain(ap, width, height, old_swapchain)) {
    return false;
  }
  if (!create_image_views(ap)) {
    cleanup_main_swapchain_resources(ap);
    return false;
  }
  if (!create_framebuffers(ap)) {
    cleanup_main_swapchain_resources(ap);
    return false;
  }

  for (uint32_t i = 0; i < STYGIAN_VK_MAX_SWAPCHAIN_IMAGES; ++i) {
    ap->image_in_flight[i] = VK_NULL_HANDLE;
  }

  if (old_swapchain) {
    vkDestroySwapchainKHR(ap->device, old_swapchain, NULL);
  }

  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_recreate_ms += stygian_vk_now_ms() - t0;
    ap->resize_telemetry_recreate_count++;
  }

  return true;
}

static bool create_command_pool(StygianAP *ap) {
  VkCommandPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = ap->graphics_family,
  };

  VkResult result =
      vkCreateCommandPool(ap->device, &pool_info, NULL, &ap->command_pool);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create command pool: %d\n", result);
    return false;
  }

  // Allocate command buffers (2 for double buffering)
  VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = ap->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = STYGIAN_VK_FRAMES_IN_FLIGHT,
  };

  result =
      vkAllocateCommandBuffers(ap->device, &alloc_info, ap->command_buffers);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate command buffers: %d\n", result);
    return false;
  }

  printf("[Stygian AP VK] Command pool and buffers created\n");
  return true;
}

static bool create_sync_objects(StygianAP *ap) {
  VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT, // Start signaled
  };

  for (int i = 0; i < STYGIAN_VK_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(ap->device, &semaphore_info, NULL,
                          &ap->image_available[i]) != VK_SUCCESS ||
        vkCreateSemaphore(ap->device, &semaphore_info, NULL,
                          &ap->render_finished[i]) != VK_SUCCESS ||
        vkCreateFence(ap->device, &fence_info, NULL, &ap->in_flight[i]) !=
            VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create sync objects\n");
      return false;
    }
  }

  ap->current_frame = 0;
  printf("[Stygian AP VK] Sync objects created\n");
  return true;
}

static bool create_buffers(StygianAP *ap) {
  VkMemoryRequirements mem_requirements;
  VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
  };

  // Create clip SSBO (vec4 clip rects)
  VkDeviceSize clip_size = STYGIAN_MAX_CLIPS * sizeof(float) * 4;
  buffer_info.size = clip_size;
  buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if (vkCreateBuffer(ap->device, &buffer_info, NULL, &ap->clip_ssbo) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create clip SSBO\n");
    return false;
  }

  vkGetBufferMemoryRequirements(ap->device, ap->clip_ssbo, &mem_requirements);
  alloc_info.allocationSize = mem_requirements.size;
  alloc_info.memoryTypeIndex =
      find_memory_type(ap->physical_device, mem_requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (vkAllocateMemory(ap->device, &alloc_info, NULL, &ap->clip_ssbo_memory) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate clip SSBO memory\n");
    return false;
  }
  vkBindBufferMemory(ap->device, ap->clip_ssbo, ap->clip_ssbo_memory, 0);

  // Create SoA SSBOs (hot=binding 4, appearance=binding 5, effects=binding 6)
  struct {
    VkBuffer *buf;
    VkDeviceMemory *mem;
    VkDeviceSize size;
    const char *name;
  } soa_bufs[3] = {
      {&ap->soa_hot_buf, &ap->soa_hot_mem,
       ap->max_elements * sizeof(StygianSoAHot), "hot"},
      {&ap->soa_appearance_buf, &ap->soa_appearance_mem,
       ap->max_elements * sizeof(StygianSoAAppearance), "appearance"},
      {&ap->soa_effects_buf, &ap->soa_effects_mem,
       ap->max_elements * sizeof(StygianSoAEffects), "effects"},
  };

  for (int si = 0; si < 3; si++) {
    buffer_info.size = soa_bufs[si].size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (vkCreateBuffer(ap->device, &buffer_info, NULL, soa_bufs[si].buf) !=
        VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to create SoA %s SSBO\n",
             soa_bufs[si].name);
      return false;
    }

    vkGetBufferMemoryRequirements(ap->device, *soa_bufs[si].buf,
                                  &mem_requirements);
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex =
        find_memory_type(ap->physical_device, mem_requirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(ap->device, &alloc_info, NULL, soa_bufs[si].mem) !=
        VK_SUCCESS) {
      printf("[Stygian AP VK] Failed to allocate SoA %s memory\n",
             soa_bufs[si].name);
      return false;
    }

    vkBindBufferMemory(ap->device, *soa_bufs[si].buf, *soa_bufs[si].mem, 0);
  }

  printf("[Stygian AP VK] SoA SSBOs created (hot: %zu, appearance: %zu, "
         "effects: %zu bytes)\n",
         (size_t)soa_bufs[0].size, (size_t)soa_bufs[1].size,
         (size_t)soa_bufs[2].size);

  // Create vertex buffer (quad: 6 vertices)
  float quad_vertices[] = {
      -1.0f, -1.0f, 1.0f, -1.0f, 1.0f,  1.0f,
      -1.0f, -1.0f, 1.0f, 1.0f,  -1.0f, 1.0f,
  };

  VkDeviceSize vb_size = sizeof(quad_vertices);

  buffer_info.size = vb_size;
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if (vkCreateBuffer(ap->device, &buffer_info, NULL, &ap->vertex_buffer) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create vertex buffer\n");
    return false;
  }

  vkGetBufferMemoryRequirements(ap->device, ap->vertex_buffer,
                                &mem_requirements);

  alloc_info.allocationSize = mem_requirements.size;
  alloc_info.memoryTypeIndex =
      find_memory_type(ap->physical_device, mem_requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (vkAllocateMemory(ap->device, &alloc_info, NULL, &ap->vertex_memory) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate vertex buffer memory\n");
    return false;
  }

  vkBindBufferMemory(ap->device, ap->vertex_buffer, ap->vertex_memory, 0);

  // Upload vertex data
  void *data;
  vkMapMemory(ap->device, ap->vertex_memory, 0, vb_size, 0, &data);
  memcpy(data, quad_vertices, vb_size);
  vkUnmapMemory(ap->device, ap->vertex_memory);

  printf("[Stygian AP VK] Buffers created (SoA hot/app/fx: %zu/%zu/%zu, "
         "Clip: %zu, VB: %zu bytes)\n",
         (size_t)soa_bufs[0].size, (size_t)soa_bufs[1].size,
         (size_t)soa_bufs[2].size, (size_t)clip_size, (size_t)vb_size);
  return true;
}

static bool create_descriptor_sets(StygianAP *ap) {
  // Descriptor set layout:
  // 1 = font sampler, 2 = image sampler array, 3 = clip SSBO,
  // 4 = SoA hot, 5 = SoA appearance, 6 = SoA effects
  VkDescriptorSetLayoutBinding bindings[6] = {
      {
          .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 2,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = STYGIAN_VK_IMAGE_SAMPLERS,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 3,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 4,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags =
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 5,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags =
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 6,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags =
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 6,
      .pBindings = bindings,
  };

  if (vkCreateDescriptorSetLayout(ap->device, &layout_info, NULL,
                                  &ap->descriptor_layout) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create descriptor set layout\n");
    return false;
  }

  // Descriptor pool (4 storage buffers: clip + hot + appearance + effects)
  VkDescriptorPoolSize pool_sizes[2] = {
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 4},
      {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = 1 + STYGIAN_VK_IMAGE_SAMPLERS},
  };

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = 2,
      .pPoolSizes = pool_sizes,
      .maxSets = 1,
  };

  if (vkCreateDescriptorPool(ap->device, &pool_info, NULL,
                             &ap->descriptor_pool) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create descriptor pool\n");
    return false;
  }

  // Allocate descriptor set
  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = ap->descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &ap->descriptor_layout,
  };

  if (vkAllocateDescriptorSets(ap->device, &alloc_info, &ap->descriptor_set) !=
      VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to allocate descriptor set\n");
    return false;
  }

  // Update descriptor set: clip + SoA hot/appearance/effects
  VkDescriptorBufferInfo clip_buffer_info = {
      .buffer = ap->clip_ssbo,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  VkDescriptorBufferInfo soa_hot_info = {
      .buffer = ap->soa_hot_buf,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  VkDescriptorBufferInfo soa_appearance_info = {
      .buffer = ap->soa_appearance_buf,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  VkDescriptorBufferInfo soa_effects_info = {
      .buffer = ap->soa_effects_buf,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };

  VkWriteDescriptorSet descriptor_writes[4] = {
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &clip_buffer_info,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 4,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &soa_hot_info,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 5,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &soa_appearance_info,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ap->descriptor_set,
          .dstBinding = 6,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &soa_effects_info,
      },
  };

  vkUpdateDescriptorSets(ap->device, 4, descriptor_writes, 0, NULL);

  printf("[Stygian AP VK] Descriptor sets created (6 bindings, SoA-only)\n");
  return true;
}

static bool load_shaders_and_create_pipeline(StygianAP *ap) {
  // Load SPIR-V shaders
  char vert_path[512], frag_path[512];
  snprintf(vert_path, sizeof(vert_path), "%s/build/stygian.vert.spv",
           ap->shader_dir);
  snprintf(frag_path, sizeof(frag_path), "%s/build/stygian.frag.spv",
           ap->shader_dir);

  ap->vert_module = load_shader_module(ap, vert_path);
  ap->frag_module = load_shader_module(ap, frag_path);

  if (!ap->vert_module || !ap->frag_module) {
    printf("[Stygian AP VK] Failed to load shaders\n");
    return false;
  }

  VkPipelineShaderStageCreateInfo shader_stages[] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = ap->vert_module,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = ap->frag_module,
          .pName = "main",
      },
  };

  // Vertex input: single vec2 attribute
  VkVertexInputBindingDescription binding_desc = {
      .binding = 0,
      .stride = 2 * sizeof(float),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  VkVertexInputAttributeDescription attribute_desc = {
      .binding = 0,
      .location = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = 0,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_desc,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &attribute_desc,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)ap->swapchain_extent.width,
      .height = (float)ap->swapchain_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = ap->swapchain_extent,
  };

  VkPipelineViewportStateCreateInfo viewport_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  VkDynamicState dynamic_states[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamic_states,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
  };

  VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
  };

  VkPipelineColorBlendStateCreateInfo color_blending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
  };

  // Push constants for screen/atlas + output color transform.
  VkPushConstantRange push_constant = {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(StygianVKPushConstants),
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &ap->descriptor_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant,
  };

  if (vkCreatePipelineLayout(ap->device, &pipeline_layout_info, NULL,
                             &ap->pipeline_layout) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create pipeline layout\n");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &color_blending,
      .pDynamicState = &dynamic_state,
      .layout = ap->pipeline_layout,
      .renderPass = ap->render_pass,
      .subpass = 0,
  };

  if (vkCreateGraphicsPipelines(ap->device, VK_NULL_HANDLE, 1, &pipeline_info,
                                NULL, &ap->graphics_pipeline) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create graphics pipeline\n");
    return false;
  }

  printf("[Stygian AP VK] Shaders loaded and pipeline created\n");
  return true;
}

// ============================================================================
// Lifecycle
// ============================================================================

StygianAP *stygian_ap_create(const StygianAPConfig *config) {
  if (!config || !config->window) {
    printf("[Stygian AP VK] Error: window required\n");
    return NULL;
  }

  StygianAP *ap = (StygianAP *)cfg_alloc(config->allocator, sizeof(StygianAP),
                                         _Alignof(StygianAP));
  if (!ap)
    return NULL;
  memset(ap, 0, sizeof(StygianAP));
  ap->allocator = config->allocator;
  ap->window = config->window;
  ap->max_elements = config->max_elements > 0 ? config->max_elements : 16384;
  ap->atlas_width = 1.0f;
  ap->atlas_height = 1.0f;
  ap->px_range = 4.0f;
  ap->output_color_transform_enabled = false;
  ap->output_src_srgb_transfer = true;
  ap->output_dst_srgb_transfer = true;
  ap->output_src_gamma = 2.4f;
  ap->output_dst_gamma = 2.4f;
  ap->resize_debounce_frames = 2;
  ap->resize_suboptimal_threshold = 3;
  ap->resize_telemetry_enabled = false;
  ap->resize_telemetry_period = 120;
  memset(ap->output_color_matrix, 0, sizeof(ap->output_color_matrix));
  ap->output_color_matrix[0] = 1.0f;
  ap->output_color_matrix[4] = 1.0f;
  ap->output_color_matrix[8] = 1.0f;

  // Copy shader directory
  stygian_cpystr(ap->shader_dir, sizeof(ap->shader_dir),
                 (config->shader_dir && config->shader_dir[0])
                     ? config->shader_dir
                     : "shaders");

  {
    const char *debounce_env = getenv("STYGIAN_VK_RESIZE_DEBOUNCE");
    if (debounce_env && debounce_env[0]) {
      long v = strtol(debounce_env, NULL, 10);
      if (v >= 0 && v <= 30) {
        ap->resize_debounce_frames = (uint32_t)v;
      }
    }
  }
  {
    const char *suboptimal_env = getenv("STYGIAN_VK_SUBOPTIMAL_THRESHOLD");
    if (suboptimal_env && suboptimal_env[0]) {
      long v = strtol(suboptimal_env, NULL, 10);
      if (v >= 1 && v <= 120) {
        ap->resize_suboptimal_threshold = (uint32_t)v;
      }
    }
  }
  {
    const char *telemetry_env = getenv("STYGIAN_VK_RESIZE_TELEMETRY");
    ap->resize_telemetry_enabled = (telemetry_env && telemetry_env[0] &&
                                    strtol(telemetry_env, NULL, 10) != 0);
  }
  {
    const char *period_env = getenv("STYGIAN_VK_RESIZE_TELEMETRY_PERIOD");
    if (period_env && period_env[0]) {
      long v = strtol(period_env, NULL, 10);
      if (v >= 1 && v <= 10000) {
        ap->resize_telemetry_period = (uint32_t)v;
      }
    }
  }

  printf("[Stygian AP VK] Initializing Vulkan backend...\n");

  // Create Vulkan instance
  if (!create_instance(ap)) {
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Pick physical device
  if (!pick_physical_device(ap)) {
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Find queue families
  if (!find_queue_families(ap)) {
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create logical device
  if (!create_logical_device(ap)) {
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create surface
  if (!create_surface(ap)) {
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Get window size for swapchain
  int width, height;
  stygian_window_get_size(ap->window, &width, &height);

  // Create swapchain
  if (!create_swapchain(ap, width, height, VK_NULL_HANDLE)) {
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create image views
  if (!create_image_views(ap)) {
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create render pass
  if (!create_render_pass(ap)) {
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create framebuffers
  if (!create_framebuffers(ap)) {
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create command pool and buffers
  if (!create_command_pool(ap)) {
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
    }
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create synchronization objects
  if (!create_sync_objects(ap)) {
    vkDestroyCommandPool(ap->device, ap->command_pool, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
    }
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create buffers
  if (!create_buffers(ap)) {
    for (int i = 0; i < STYGIAN_VK_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore(ap->device, ap->render_finished[i], NULL);
      vkDestroySemaphore(ap->device, ap->image_available[i], NULL);
      vkDestroyFence(ap->device, ap->in_flight[i], NULL);
    }
    vkDestroyCommandPool(ap->device, ap->command_pool, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
    }
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);
    for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);
    vkDestroyDevice(ap->device, NULL);
    vkDestroyInstance(ap->instance, NULL);
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Create descriptor sets
  if (!create_descriptor_sets(ap)) {
    vkDestroyBuffer(ap->device, ap->vertex_buffer, NULL);
    vkFreeMemory(ap->device, ap->vertex_memory, NULL);
    if (ap->clip_ssbo)
      vkDestroyBuffer(ap->device, ap->clip_ssbo, NULL);
    if (ap->clip_ssbo_memory)
      vkFreeMemory(ap->device, ap->clip_ssbo_memory, NULL);
    if (ap->soa_hot_buf)
      vkDestroyBuffer(ap->device, ap->soa_hot_buf, NULL);
    if (ap->soa_hot_mem)
      vkFreeMemory(ap->device, ap->soa_hot_mem, NULL);
    if (ap->soa_appearance_buf)
      vkDestroyBuffer(ap->device, ap->soa_appearance_buf, NULL);
    if (ap->soa_appearance_mem)
      vkFreeMemory(ap->device, ap->soa_appearance_mem, NULL);
    if (ap->soa_effects_buf)
      vkDestroyBuffer(ap->device, ap->soa_effects_buf, NULL);
    if (ap->soa_effects_mem)
      vkFreeMemory(ap->device, ap->soa_effects_mem, NULL);
    // ... (cleanup sync, command pool, framebuffers, etc.)
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Load shaders and create pipeline
  if (!load_shaders_and_create_pipeline(ap)) {
    vkDestroyDescriptorPool(ap->device, ap->descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(ap->device, ap->descriptor_layout, NULL);
    vkDestroyBuffer(ap->device, ap->vertex_buffer, NULL);
    vkFreeMemory(ap->device, ap->vertex_memory, NULL);
    // ... (cleanup sync, command pool, framebuffers, etc.)
    cfg_free(ap->allocator, ap);
    return NULL;
  }

  // Allocate GPU-side version tracking for SoA chunk upload
  // Default chunk_size 256 → chunk_count = ceil(max_elements / 256)
  {
    uint32_t cs = 256u;
    uint32_t cc = (ap->max_elements + cs - 1u) / cs;
    ap->soa_chunk_count = cc;
    ap->gpu_hot_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_appearance_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    ap->gpu_effects_versions =
        (uint32_t *)ap_alloc(ap, cc * sizeof(uint32_t), _Alignof(uint32_t));
    if (ap->gpu_hot_versions)
      memset(ap->gpu_hot_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_appearance_versions)
      memset(ap->gpu_appearance_versions, 0, cc * sizeof(uint32_t));
    if (ap->gpu_effects_versions)
      memset(ap->gpu_effects_versions, 0, cc * sizeof(uint32_t));
  }

  printf("[Stygian AP VK] Vulkan backend initialized successfully\n");
  ap->initialized = true;
  return ap;
}

void stygian_ap_destroy(StygianAP *ap) {
  if (!ap)
    return;

  // Wait for device to finish all operations
  if (ap->device) {
    vkDeviceWaitIdle(ap->device);
  }

  // Destroy sync objects
  for (int i = 0; i < STYGIAN_VK_FRAMES_IN_FLIGHT; i++) {
    if (ap->render_finished[i])
      vkDestroySemaphore(ap->device, ap->render_finished[i], NULL);
    if (ap->image_available[i])
      vkDestroySemaphore(ap->device, ap->image_available[i], NULL);
    if (ap->in_flight[i])
      vkDestroyFence(ap->device, ap->in_flight[i], NULL);
  }

  // Destroy command pool (frees command buffers automatically)
  if (ap->command_pool)
    vkDestroyCommandPool(ap->device, ap->command_pool, NULL);

  if (ap->graphics_pipeline)
    vkDestroyPipeline(ap->device, ap->graphics_pipeline, NULL);
  if (ap->pipeline_layout)
    vkDestroyPipelineLayout(ap->device, ap->pipeline_layout, NULL);
  if (ap->vert_module)
    vkDestroyShaderModule(ap->device, ap->vert_module, NULL);
  if (ap->frag_module)
    vkDestroyShaderModule(ap->device, ap->frag_module, NULL);
  if (ap->descriptor_pool)
    vkDestroyDescriptorPool(ap->device, ap->descriptor_pool, NULL);
  if (ap->descriptor_layout)
    vkDestroyDescriptorSetLayout(ap->device, ap->descriptor_layout, NULL);
  if (ap->vertex_buffer)
    vkDestroyBuffer(ap->device, ap->vertex_buffer, NULL);
  if (ap->vertex_memory)
    vkFreeMemory(ap->device, ap->vertex_memory, NULL);
  if (ap->clip_ssbo)
    vkDestroyBuffer(ap->device, ap->clip_ssbo, NULL);
  if (ap->clip_ssbo_memory)
    vkFreeMemory(ap->device, ap->clip_ssbo_memory, NULL);
  // SoA buffer cleanup
  if (ap->soa_hot_buf)
    vkDestroyBuffer(ap->device, ap->soa_hot_buf, NULL);
  if (ap->soa_hot_mem)
    vkFreeMemory(ap->device, ap->soa_hot_mem, NULL);
  if (ap->soa_appearance_buf)
    vkDestroyBuffer(ap->device, ap->soa_appearance_buf, NULL);
  if (ap->soa_appearance_mem)
    vkFreeMemory(ap->device, ap->soa_appearance_mem, NULL);
  if (ap->soa_effects_buf)
    vkDestroyBuffer(ap->device, ap->soa_effects_buf, NULL);
  if (ap->soa_effects_mem)
    vkFreeMemory(ap->device, ap->soa_effects_mem, NULL);
  if (ap->font_sampler)
    vkDestroySampler(ap->device, ap->font_sampler, NULL);
  if (ap->font_view)
    vkDestroyImageView(ap->device, ap->font_view, NULL);
  if (ap->font_image)
    vkDestroyImage(ap->device, ap->font_image, NULL);
  if (ap->font_memory)
    vkFreeMemory(ap->device, ap->font_memory, NULL);

  // Destroy framebuffers
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    if (ap->framebuffers[i])
      vkDestroyFramebuffer(ap->device, ap->framebuffers[i], NULL);
  }

  // Destroy render pass
  if (ap->render_pass)
    vkDestroyRenderPass(ap->device, ap->render_pass, NULL);

  // Destroy image views
  for (uint32_t i = 0; i < ap->swapchain_image_count; i++) {
    if (ap->swapchain_views[i])
      vkDestroyImageView(ap->device, ap->swapchain_views[i], NULL);
  }

  // Destroy swapchain
  if (ap->swapchain)
    vkDestroySwapchainKHR(ap->device, ap->swapchain, NULL);

  // Destroy surface
  if (ap->surface)
    vkDestroySurfaceKHR(ap->instance, ap->surface, NULL);

  // Destroy device
  if (ap->device)
    vkDestroyDevice(ap->device, NULL);

  // Destroy instance
  if (ap->instance)
    vkDestroyInstance(ap->instance, NULL);

  // Free AP struct via its own allocator
  StygianAllocator *allocator = ap->allocator;
  cfg_free(allocator, ap);
}

StygianAPAdapterClass stygian_ap_get_adapter_class(const StygianAP *ap) {
  if (!ap)
    return STYGIAN_AP_ADAPTER_UNKNOWN;
  return ap->adapter_class;
}

uint32_t stygian_ap_get_last_upload_bytes(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_bytes;
}

uint32_t stygian_ap_get_last_upload_ranges(const StygianAP *ap) {
  if (!ap)
    return 0u;
  return ap->last_upload_ranges;
}

float stygian_ap_get_last_gpu_ms(const StygianAP *ap) {
  (void)ap;
  return 0.0f;
}

void stygian_ap_gpu_timer_begin(StygianAP *ap) { (void)ap; }

void stygian_ap_gpu_timer_end(StygianAP *ap) { (void)ap; }

void stygian_ap_begin_frame(StygianAP *ap, int width, int height) {
  if (!ap)
    return;

  ap->frame_active = false;

  if (ap->window) {
    int fb_w = 0, fb_h = 0;
    stygian_window_get_framebuffer_size(ap->window, &fb_w, &fb_h);
    if (fb_w > 0 && fb_h > 0) {
      width = fb_w;
      height = fb_h;
    }
  }

  if (width <= 0 || height <= 0) {
    return; // Minimized window
  }

  if (ap->swapchain_needs_recreate) {
    if (!recreate_main_swapchain(ap, width, height)) {
      return;
    }
    ap->swapchain_needs_recreate = false;
  }

  // Coalesce resize churn: only recreate after size is stable for N frames.
  if (!ap->swapchain_needs_recreate &&
      ((uint32_t)width != ap->swapchain_extent.width ||
       (uint32_t)height != ap->swapchain_extent.height)) {
    if (ap->resize_pending_w != width || ap->resize_pending_h != height) {
      ap->resize_pending_w = width;
      ap->resize_pending_h = height;
      ap->resize_stable_count = 0;
      return;
    }

    ap->resize_stable_count++;
    if (ap->resize_stable_count < ap->resize_debounce_frames) {
      return;
    }

    ap->swapchain_needs_recreate = true;
    if (!recreate_main_swapchain(ap, width, height)) {
      return;
    }
    ap->swapchain_needs_recreate = false;
    ap->resize_stable_count = 0;
  } else {
    ap->resize_pending_w = width;
    ap->resize_pending_h = height;
    ap->resize_stable_count = 0;
  }

  // Wait for previous frame to finish
  vkWaitForFences(ap->device, 1, &ap->in_flight[ap->current_frame], VK_TRUE,
                  UINT64_MAX);

  // Acquire next swapchain image
  double acquire_t0 = stygian_vk_now_ms();
  VkResult result =
      vkAcquireNextImageKHR(ap->device, ap->swapchain, UINT64_MAX,
                            ap->image_available[ap->current_frame],
                            VK_NULL_HANDLE, &ap->current_image);
  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_acquire_ms += (stygian_vk_now_ms() - acquire_t0);
  }

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    ap->swapchain_needs_recreate = true;
    return;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    printf("[Stygian AP VK] Failed to acquire swapchain image: %d\n", result);
    return;
  }
  if (result == VK_SUBOPTIMAL_KHR) {
    ap->resize_suboptimal_count++;
    if (ap->resize_suboptimal_count >= ap->resize_suboptimal_threshold) {
      ap->swapchain_needs_recreate = true;
    }
  } else {
    ap->resize_suboptimal_count = 0;
  }

  // If this image is still in use by a previous frame, wait only for that one.
  if (ap->current_image < STYGIAN_VK_MAX_SWAPCHAIN_IMAGES &&
      ap->image_in_flight[ap->current_image] != VK_NULL_HANDLE) {
    vkWaitForFences(ap->device, 1, &ap->image_in_flight[ap->current_image],
                    VK_TRUE, UINT64_MAX);
  }
  if (ap->current_image < STYGIAN_VK_MAX_SWAPCHAIN_IMAGES) {
    ap->image_in_flight[ap->current_image] = ap->in_flight[ap->current_frame];
  }

  // Reset fence for this frame
  vkResetFences(ap->device, 1, &ap->in_flight[ap->current_frame]);

  // Reset and begin command buffer
  VkCommandBuffer cmd = ap->command_buffers[ap->current_frame];
  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &begin_info);

  // Begin render pass
  VkClearValue clear_color = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
  VkRenderPassBeginInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = ap->render_pass,
      .framebuffer = ap->framebuffers[ap->current_image],
      .renderArea = {.offset = {0, 0}, .extent = ap->swapchain_extent},
      .clearValueCount = 1,
      .pClearValues = &clear_color,
  };

  vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  ap->frame_active = true;
}

void stygian_ap_submit(StygianAP *ap, const StygianSoAHot *soa_hot,
                       uint32_t count) {
  // Vulkan path uses explicit descriptor slots; texture IDs are already compact.
  (void)soa_hot;
  if (!ap || count == 0)
    return;
  if (count > ap->max_elements)
    count = ap->max_elements;
  ap->element_count = count;
}

void stygian_ap_submit_soa(StygianAP *ap, const StygianSoAHot *hot,
                           const StygianSoAAppearance *appearance,
                           const StygianSoAEffects *effects,
                           uint32_t element_count,
                           const StygianBufferChunk *chunks,
                           uint32_t chunk_count, uint32_t chunk_size) {
  if (!ap || !hot || !appearance || !effects || !chunks || element_count == 0)
    return;

  // Clamp to tracked chunk count
  if (chunk_count > ap->soa_chunk_count) {
    chunk_count = ap->soa_chunk_count;
  }

  ap->last_upload_bytes = 0u;
  ap->last_upload_ranges = 0u;

  // Map all three SoA buffers once for the entire upload pass
  void *hot_mapped = NULL;
  void *app_mapped = NULL;
  void *eff_mapped = NULL;

  vkMapMemory(ap->device, ap->soa_hot_mem, 0, VK_WHOLE_SIZE, 0, &hot_mapped);
  vkMapMemory(ap->device, ap->soa_appearance_mem, 0, VK_WHOLE_SIZE, 0,
              &app_mapped);
  vkMapMemory(ap->device, ap->soa_effects_mem, 0, VK_WHOLE_SIZE, 0,
              &eff_mapped);

  for (uint32_t ci = 0; ci < chunk_count; ci++) {
    const StygianBufferChunk *c = &chunks[ci];
    uint32_t base = ci * chunk_size;

    // --- Hot buffer ---
    if (ap->gpu_hot_versions && c->hot_version != ap->gpu_hot_versions[ci]) {
      uint32_t dmin = c->hot_dirty_min;
      uint32_t dmax = c->hot_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count && hot_mapped) {
          uint32_t range_count = abs_max - abs_min + 1;
          size_t offset = (size_t)abs_min * sizeof(StygianSoAHot);
          size_t bytes = (size_t)range_count * sizeof(StygianSoAHot);
          memcpy((char *)hot_mapped + offset, &hot[abs_min], bytes);
          ap->last_upload_bytes += (uint32_t)bytes;
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_hot_versions[ci] = c->hot_version;
    }

    // --- Appearance buffer ---
    if (ap->gpu_appearance_versions &&
        c->appearance_version != ap->gpu_appearance_versions[ci]) {
      uint32_t dmin = c->appearance_dirty_min;
      uint32_t dmax = c->appearance_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count && app_mapped) {
          uint32_t range_count = abs_max - abs_min + 1;
          size_t offset = (size_t)abs_min * sizeof(StygianSoAAppearance);
          size_t bytes = (size_t)range_count * sizeof(StygianSoAAppearance);
          memcpy((char *)app_mapped + offset, &appearance[abs_min], bytes);
          ap->last_upload_bytes += (uint32_t)bytes;
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_appearance_versions[ci] = c->appearance_version;
    }

    // --- Effects buffer ---
    if (ap->gpu_effects_versions &&
        c->effects_version != ap->gpu_effects_versions[ci]) {
      uint32_t dmin = c->effects_dirty_min;
      uint32_t dmax = c->effects_dirty_max;
      if (dmin <= dmax) {
        uint32_t abs_min = base + dmin;
        uint32_t abs_max = base + dmax;
        if (abs_max >= element_count)
          abs_max = element_count - 1;
        if (abs_min < element_count && eff_mapped) {
          uint32_t range_count = abs_max - abs_min + 1;
          size_t offset = (size_t)abs_min * sizeof(StygianSoAEffects);
          size_t bytes = (size_t)range_count * sizeof(StygianSoAEffects);
          memcpy((char *)eff_mapped + offset, &effects[abs_min], bytes);
          ap->last_upload_bytes += (uint32_t)bytes;
          ap->last_upload_ranges++;
        }
      }
      ap->gpu_effects_versions[ci] = c->effects_version;
    }
  }

  // Unmap all three buffers
  if (hot_mapped)
    vkUnmapMemory(ap->device, ap->soa_hot_mem);
  if (app_mapped)
    vkUnmapMemory(ap->device, ap->soa_appearance_mem);
  if (eff_mapped)
    vkUnmapMemory(ap->device, ap->soa_effects_mem);
}

void stygian_ap_draw(StygianAP *ap) {
  if (!ap || !ap->frame_active || ap->element_count == 0)
    return;
  stygian_ap_draw_range(ap, 0u, ap->element_count);
}

void stygian_ap_draw_range(StygianAP *ap, uint32_t first_instance,
                           uint32_t instance_count) {
  if (!ap || !ap->frame_active || instance_count == 0)
    return;

  VkCommandBuffer cmd = ap->command_buffers[ap->current_frame];

  // Bind pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ap->graphics_pipeline);

  // Bind descriptor set (SSBO)
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          ap->pipeline_layout, 0, 1, &ap->descriptor_set, 0,
                          NULL);

  // Push constants (shared vertex/fragment constants)
  {
    StygianVKPushConstants pc;
    fill_push_constants(ap, (float)ap->swapchain_extent.width,
                        (float)ap->swapchain_extent.height, &pc);
    vkCmdPushConstants(cmd, ap->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);
  }

  // Bind vertex buffer
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &ap->vertex_buffer, &offset);

  // Dynamic viewport/scissor to match current swapchain extent after resize.
  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)ap->swapchain_extent.width,
      .height = (float)ap->swapchain_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = ap->swapchain_extent,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Draw instanced (6 vertices per quad, count instances)
  vkCmdDraw(cmd, 6, instance_count, 0, first_instance);
}

void stygian_ap_end_frame(StygianAP *ap) {
  if (!ap || !ap->frame_active)
    return;

  // End render pass
  VkCommandBuffer cmd = ap->command_buffers[ap->current_frame];
  vkCmdEndRenderPass(cmd);

  // End command buffer
  VkResult result = vkEndCommandBuffer(cmd);
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to end command buffer: %d\n", result);
    return;
  }

  // Submit command buffer
  VkSemaphore wait_semaphores[] = {ap->image_available[ap->current_frame]};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signal_semaphores[] = {ap->render_finished[ap->current_frame]};

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = wait_semaphores,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signal_semaphores,
  };

  double submit_t0 = stygian_vk_now_ms();
  result = vkQueueSubmit(ap->graphics_queue, 1, &submit_info,
                         ap->in_flight[ap->current_frame]);
  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_submit_ms += (stygian_vk_now_ms() - submit_t0);
  }
  if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to submit command buffer: %d\n", result);
  }
}

void stygian_ap_swap(StygianAP *ap) {
  if (!ap || !ap->frame_active)
    return;

  // Present
  VkSemaphore signal_semaphores[] = {ap->render_finished[ap->current_frame]};
  VkSwapchainKHR swapchains[] = {ap->swapchain};

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signal_semaphores,
      .swapchainCount = 1,
      .pSwapchains = swapchains,
      .pImageIndices = &ap->current_image,
  };

  double present_t0 = stygian_vk_now_ms();
  VkResult result = vkQueuePresentKHR(ap->graphics_queue, &present_info);
  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_present_ms += (stygian_vk_now_ms() - present_t0);
  }
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    ap->swapchain_needs_recreate = true;
  } else if (result == VK_SUBOPTIMAL_KHR) {
    ap->resize_suboptimal_count++;
    if (ap->resize_suboptimal_count >= ap->resize_suboptimal_threshold) {
      ap->swapchain_needs_recreate = true;
    }
  } else if (result != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to present: %d\n", result);
  } else {
    ap->resize_suboptimal_count = 0;
  }

  if (ap->resize_telemetry_enabled) {
    ap->resize_telemetry_frames++;
    if (ap->resize_telemetry_frames >= ap->resize_telemetry_period) {
      double frames = (double)ap->resize_telemetry_frames;
      printf("[Stygian AP VK] resize telemetry: acquire=%.3fms submit=%.3fms "
             "present=%.3fms recreate_total=%.3fms recreates=%u/%u frames\n",
             ap->resize_telemetry_acquire_ms / frames,
             ap->resize_telemetry_submit_ms / frames,
             ap->resize_telemetry_present_ms / frames,
             ap->resize_telemetry_recreate_ms,
             ap->resize_telemetry_recreate_count, ap->resize_telemetry_frames);
      ap->resize_telemetry_frames = 0;
      ap->resize_telemetry_recreate_count = 0;
      ap->resize_telemetry_acquire_ms = 0.0;
      ap->resize_telemetry_submit_ms = 0.0;
      ap->resize_telemetry_present_ms = 0.0;
      ap->resize_telemetry_recreate_ms = 0.0;
    }
  }

  // Advance to next frame
  ap->current_frame = (ap->current_frame + 1) % STYGIAN_VK_FRAMES_IN_FLIGHT;
  ap->frame_active = false;
}

StygianAPTexture stygian_ap_texture_create(StygianAP *ap, int w, int h,
                                           const void *rgba) {
  if (!ap || !rgba)
    return 0;

  // Create image
  VkImageCreateInfo image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {.width = w, .height = h, .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage image;
  if (vkCreateImage(ap->device, &image_info, NULL, &image) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create texture image\n");
    return 0;
  }

  // Allocate memory
  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(ap->device, image, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = mem_reqs.size,
      .memoryTypeIndex =
          find_memory_type(ap->physical_device, mem_reqs.memoryTypeBits,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  VkDeviceMemory memory;
  if (vkAllocateMemory(ap->device, &alloc_info, NULL, &memory) != VK_SUCCESS) {
    vkDestroyImage(ap->device, image, NULL);
    printf("[Stygian AP VK] Failed to allocate texture memory\n");
    return 0;
  }

  vkBindImageMemory(ap->device, image, memory, 0);

  // Create staging buffer
  VkDeviceSize image_size = w * h * 4;
  VkBuffer staging_buffer;
  VkDeviceMemory staging_memory;

  VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = image_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  vkCreateBuffer(ap->device, &buffer_info, NULL, &staging_buffer);

  VkMemoryRequirements buf_mem_reqs;
  vkGetBufferMemoryRequirements(ap->device, staging_buffer, &buf_mem_reqs);

  VkMemoryAllocateInfo buf_alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = buf_mem_reqs.size,
      .memoryTypeIndex =
          find_memory_type(ap->physical_device, buf_mem_reqs.memoryTypeBits,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };

  vkAllocateMemory(ap->device, &buf_alloc, NULL, &staging_memory);
  vkBindBufferMemory(ap->device, staging_buffer, staging_memory, 0);

  // Upload to staging buffer
  void *data;
  vkMapMemory(ap->device, staging_memory, 0, image_size, 0, &data);
  memcpy(data, rgba, image_size);
  vkUnmapMemory(ap->device, staging_memory);

  // Create one-time command buffer for copy
  VkCommandBufferAllocateInfo cmd_alloc = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = ap->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(ap->device, &cmd_alloc, &cmd);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Transition image to TRANSFER_DST_OPTIMAL
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                       &barrier);

  // Copy buffer to image
  VkBufferImageCopy region = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {w, h, 1},
  };

  vkCmdCopyBufferToImage(cmd, staging_buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // Transition to SHADER_READ_ONLY_OPTIMAL
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);

  vkEndCommandBuffer(cmd);

  // Submit and wait
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
  };

  vkQueueSubmit(ap->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(ap->graphics_queue);

  vkFreeCommandBuffers(ap->device, ap->command_pool, 1, &cmd);
  vkDestroyBuffer(ap->device, staging_buffer, NULL);
  vkFreeMemory(ap->device, staging_memory, NULL);

  // Create image view
  VkImageViewCreateInfo view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
  };

  VkImageView view;
  if (vkCreateImageView(ap->device, &view_info, NULL, &view) != VK_SUCCESS) {
    vkDestroyImage(ap->device, image, NULL);
    vkFreeMemory(ap->device, memory, NULL);
    printf("[Stygian AP VK] Failed to create image view\n");
    return 0;
  }

  // Create sampler
  VkSamplerCreateInfo sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_FALSE,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_FALSE,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
  };

  VkSampler sampler;
  if (vkCreateSampler(ap->device, &sampler_info, NULL, &sampler) !=
      VK_SUCCESS) {
    vkDestroyImageView(ap->device, view, NULL);
    vkDestroyImage(ap->device, image, NULL);
    vkFreeMemory(ap->device, memory, NULL);
    printf("[Stygian AP VK] Failed to create sampler\n");
    return 0;
  }

  // Store font texture
  ap->font_image = image;
  ap->font_memory = memory;
  ap->font_view = view;
  ap->font_sampler = sampler;

  // Update descriptor set with font texture (binding 1)
  VkDescriptorImageInfo desc_image_info = {
      .sampler = sampler,
      .imageView = view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  VkWriteDescriptorSet descriptor_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = ap->descriptor_set,
      .dstBinding = 1,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .pImageInfo = &desc_image_info,
  };

  vkUpdateDescriptorSets(ap->device, 1, &descriptor_write, 0, NULL);
  update_image_sampler_array(ap);

  printf("[Stygian AP VK] Texture created: %dx%d\n", w, h);
  return (StygianAPTexture)1;
}

bool stygian_ap_texture_update(StygianAP *ap, StygianAPTexture tex, int x,
                               int y, int w, int h, const void *rgba) {
  (void)ap;
  (void)tex;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)rgba;
  return false;
}

void stygian_ap_texture_destroy(StygianAP *ap, StygianAPTexture tex) {
  if (!ap || !tex)
    return;

  // TODO: Destroy VkImage and free memory
}

void stygian_ap_texture_bind(StygianAP *ap, StygianAPTexture tex,
                             uint32_t slot) {
  if (!ap)
    return;
  (void)tex;
  (void)slot;

  // TODO: Update descriptor set with texture
}

void stygian_ap_set_font_texture(StygianAP *ap, StygianAPTexture tex,
                                 int atlas_w, int atlas_h, float px_range) {
  if (!ap || !tex)
    return;
  ap->atlas_width = atlas_w > 0 ? (float)atlas_w : 1.0f;
  ap->atlas_height = atlas_h > 0 ? (float)atlas_h : 1.0f;
  ap->px_range = px_range > 0.0f ? px_range : 4.0f;

  // Update descriptor set with font texture (binding 1)
  VkDescriptorImageInfo image_info = {
      .sampler = ap->font_sampler,
      .imageView = ap->font_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  VkWriteDescriptorSet descriptor_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = ap->descriptor_set,
      .dstBinding = 1,
      .dstArrayElement = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .pImageInfo = &image_info,
  };

  vkUpdateDescriptorSets(ap->device, 1, &descriptor_write, 0, NULL);
  update_image_sampler_array(ap);
  printf("[Stygian AP VK] Font texture bound: %dx%d, px_range=%.1f\n", atlas_w,
         atlas_h, px_range);
}

void stygian_ap_set_output_color_transform(
    StygianAP *ap, bool enabled, const float *rgb3x3, bool src_srgb_transfer,
    float src_gamma, bool dst_srgb_transfer, float dst_gamma) {
  static const float identity[9] = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  if (!ap)
    return;
  ap->output_color_transform_enabled = enabled;
  memcpy(ap->output_color_matrix, rgb3x3 ? rgb3x3 : identity,
         sizeof(ap->output_color_matrix));
  ap->output_src_srgb_transfer = src_srgb_transfer;
  ap->output_dst_srgb_transfer = dst_srgb_transfer;
  ap->output_src_gamma = src_gamma > 0.0f ? src_gamma : 2.2f;
  ap->output_dst_gamma = dst_gamma > 0.0f ? dst_gamma : 2.2f;
}

void stygian_ap_set_clips(StygianAP *ap, const float *clips, uint32_t count) {
  if (!ap || ap->clip_ssbo_memory == VK_NULL_HANDLE)
    return;
  if (!clips || count == 0)
    return;
  if (count > STYGIAN_MAX_CLIPS)
    count = STYGIAN_MAX_CLIPS;

  void *data = NULL;
  if (vkMapMemory(ap->device, ap->clip_ssbo_memory, 0, VK_WHOLE_SIZE, 0,
                  &data) != VK_SUCCESS) {
    return;
  }
  memcpy(data, clips, count * sizeof(float) * 4);
  vkUnmapMemory(ap->device, ap->clip_ssbo_memory);
}

bool stygian_ap_reload_shaders(StygianAP *ap) {
  if (!ap)
    return false;

  printf("[Stygian AP VK] Shader reload not yet implemented\n");
  return false;
}

bool stygian_ap_shaders_need_reload(StygianAP *ap) {
  return false; // TODO: Implement file watching
}

// ============================================================================
// Multi-Surface API
// ============================================================================

static bool create_surface_swapchain(StygianAPSurface *surf, int width,
                                     int height) {
  StygianAP *ap = surf->ap;

  if (!surf->surface) {
    if (!stygian_window_vk_create_surface(surf->window, ap->instance,
                                          (void **)&surf->surface)) {
      printf("[Stygian AP VK] Failed to create surface for window\n");
      return false;
    }
  }

  // Verify surface support
  VkBool32 supported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(ap->physical_device, ap->graphics_family,
                                       surf->surface, &supported);
  if (!supported) {
    printf("[Stygian AP VK] Surface not supported by queue family\n");
    return false;
  }

  // Query surface capabilities
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ap->physical_device, surf->surface,
                                            &capabilities);

  // Query formats
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, surf->surface,
                                       &format_count, NULL);
  VkSurfaceFormatKHR formats[16];
  if (format_count > 16)
    format_count = 16;
  vkGetPhysicalDeviceSurfaceFormatsKHR(ap->physical_device, surf->surface,
                                       &format_count, formats);

  VkSurfaceFormatKHR surface_format = formats[0];
  for (uint32_t i = 0; i < format_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
      surface_format = formats[i];
      break;
    }
  }

  // Determine extent
  VkExtent2D extent;
  if (capabilities.currentExtent.width != UINT32_MAX) {
    extent = capabilities.currentExtent;
  } else {
    extent.width = width;
    extent.height = height;
  }

  // Create swapchain
  uint32_t image_count = 2; // Double buffering for secondary windows
  if (image_count < capabilities.minImageCount)
    image_count = capabilities.minImageCount;
  if (capabilities.maxImageCount > 0 &&
      image_count > capabilities.maxImageCount)
    image_count = capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR swapchain_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surf->surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
  };

  if (vkCreateSwapchainKHR(ap->device, &swapchain_info, NULL,
                           &surf->swapchain) != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to create swapchain for surface\n");
    return false;
  }

  // Get swapchain images
  vkGetSwapchainImagesKHR(ap->device, surf->swapchain, &surf->image_count,
                          NULL);
  if (surf->image_count > STYGIAN_VK_MAX_SWAPCHAIN_IMAGES)
    surf->image_count = STYGIAN_VK_MAX_SWAPCHAIN_IMAGES;
  vkGetSwapchainImagesKHR(ap->device, surf->swapchain, &surf->image_count,
                          surf->swapchain_images);

  surf->format = surface_format.format;
  surf->extent = extent;

  // Create image views
  for (uint32_t i = 0; i < surf->image_count; i++) {
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = surf->swapchain_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = surf->format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    if (vkCreateImageView(ap->device, &view_info, NULL,
                          &surf->swapchain_views[i]) != VK_SUCCESS) {
      return false;
    }
  }

  // Create framebuffers
  for (uint32_t i = 0; i < surf->image_count; i++) {
    VkImageView attachments[] = {surf->swapchain_views[i]};

    VkFramebufferCreateInfo fb_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ap->render_pass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = surf->extent.width,
        .height = surf->extent.height,
        .layers = 1,
    };

    if (vkCreateFramebuffer(ap->device, &fb_info, NULL,
                            &surf->framebuffers[i]) != VK_SUCCESS) {
      return false;
    }
  }

  // Create sync objects and command buffer (ONLY IF MISSING)
  if (!surf->image_available) {
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {.sType =
                                        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                    .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    vkCreateSemaphore(ap->device, &sem_info, NULL, &surf->image_available);
    vkCreateSemaphore(ap->device, &sem_info, NULL, &surf->render_finished);
    vkCreateFence(ap->device, &fence_info, NULL, &surf->in_flight);
  }

  if (!surf->command_buffer) {
    VkCommandBufferAllocateInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ap->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(ap->device, &cmd_info, &surf->command_buffer);
  }

  printf("[Stygian AP VK] Surface created: %dx%d\n", extent.width,
         extent.height);
  surf->resize_suboptimal_count = 0;
  return true;
}

// Helper to clean up swapchain resources (for resize or destroy)
static void cleanup_surface_swapchain(StygianAP *ap,
                                      StygianAPSurface *surface) {
  if (surface->in_flight) {
    vkWaitForFences(ap->device, 1, &surface->in_flight, VK_TRUE, UINT64_MAX);
  }

  // Destroy framebuffers and image views
  for (uint32_t i = 0; i < surface->image_count; i++) {
    if (surface->framebuffers[i]) {
      vkDestroyFramebuffer(ap->device, surface->framebuffers[i], NULL);
      surface->framebuffers[i] = VK_NULL_HANDLE;
    }
    if (surface->swapchain_views[i]) {
      vkDestroyImageView(ap->device, surface->swapchain_views[i], NULL);
      surface->swapchain_views[i] = VK_NULL_HANDLE;
    }
  }

  if (surface->swapchain) {
    vkDestroySwapchainKHR(ap->device, surface->swapchain, NULL);
    surface->swapchain = VK_NULL_HANDLE;
  }
}

StygianAPSurface *stygian_ap_surface_create(StygianAP *ap,
                                            StygianWindow *window) {
  if (!ap || !window)
    return NULL;

  StygianAPSurface *surf = (StygianAPSurface *)ap_alloc(
      ap, sizeof(StygianAPSurface), _Alignof(StygianAPSurface));
  if (!surf)
    return NULL;
  memset(surf, 0, sizeof(StygianAPSurface));

  surf->ap = ap;
  surf->window = window;
  surf->resize_debounce_frames = ap->resize_debounce_frames;

  int width, height;
  stygian_window_get_framebuffer_size(window, &width, &height);

  if (!create_surface_swapchain(surf, width, height)) {
    ap_free(ap, surf);
    return NULL;
  }

  return surf;
}

void stygian_ap_surface_destroy(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface)
    return;

  vkDeviceWaitIdle(ap->device);

  // Destroy sync objects
  if (surface->image_available)
    vkDestroySemaphore(ap->device, surface->image_available, NULL);
  if (surface->render_finished)
    vkDestroySemaphore(ap->device, surface->render_finished, NULL);
  if (surface->in_flight)
    vkDestroyFence(ap->device, surface->in_flight, NULL);

  // Cleanup swapchain resources
  cleanup_surface_swapchain(ap, surface);

  if (surface->surface)
    vkDestroySurfaceKHR(ap->instance, surface->surface, NULL);

  ap_free(ap, surface);
  printf("[Stygian AP VK] Surface destroyed\n");
}

void stygian_ap_surface_begin(StygianAP *ap, StygianAPSurface *surface,
                              int width, int height) {
  if (!ap || !surface)
    return;
  surface->frame_active = false;

  if (width <= 0 || height <= 0) {
    return; // Window minimized or invalid size
  }

  // Coalesce resize churn for secondary surfaces too (same policy as main).
  if (surface->swapchain_needs_recreate ||
      surface->extent.width != (uint32_t)width ||
      surface->extent.height != (uint32_t)height) {
    if (surface->resize_pending_w != width ||
        surface->resize_pending_h != height) {
      surface->resize_pending_w = width;
      surface->resize_pending_h = height;
      surface->resize_stable_count = 0;
      return;
    }
    surface->resize_stable_count++;
    if (surface->resize_stable_count < surface->resize_debounce_frames) {
      return;
    }

    cleanup_surface_swapchain(ap, surface);
    if (!create_surface_swapchain(surface, width, height)) {
      printf("[Stygian AP VK] Failed to recreate swapchain during resize\n");
      return;
    }
    surface->swapchain_needs_recreate = false;
    surface->resize_stable_count = 0;
  } else {
    surface->resize_pending_w = width;
    surface->resize_pending_h = height;
    surface->resize_stable_count = 0;
  }

  // Wait for previous frame
  VkResult fence_res =
      vkWaitForFences(ap->device, 1, &surface->in_flight, VK_TRUE, UINT64_MAX);
  if (fence_res != VK_SUCCESS) {
    printf("[Stygian AP VK] Wait for fences failed: %d\n", fence_res);
    return;
  }
  vkResetFences(ap->device, 1, &surface->in_flight);

  // Acquire next image
  VkResult result = vkAcquireNextImageKHR(
      ap->device, surface->swapchain, UINT64_MAX, surface->image_available,
      VK_NULL_HANDLE, &surface->current_image);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    surface->swapchain_needs_recreate = true;
    return;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    printf("[Stygian AP VK] Failed to acquire image: %d\n", result);
    return;
  }
  if (result == VK_SUBOPTIMAL_KHR) {
    surface->resize_suboptimal_count++;
    if (surface->resize_suboptimal_count >= ap->resize_suboptimal_threshold) {
      surface->swapchain_needs_recreate = true;
    }
  } else {
    surface->resize_suboptimal_count = 0;
  }

  // Begin command buffer
  vkResetCommandBuffer(surface->command_buffer, 0);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  vkBeginCommandBuffer(surface->command_buffer, &begin_info);

  // Begin render pass
  VkClearValue clear_color = {{{0.08f, 0.08f, 0.08f, 1.0f}}};
  VkRenderPassBeginInfo rp_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = ap->render_pass,
      .framebuffer = surface->framebuffers[surface->current_image],
      .renderArea = {{0, 0}, surface->extent},
      .clearValueCount = 1,
      .pClearValues = &clear_color,
  };
  vkCmdBeginRenderPass(surface->command_buffer, &rp_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  // Set viewport and scissor (PHYSICAL)
  VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = (float)surface->extent.width,
      .height = (float)surface->extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  // DEBUG: Print surface extent periodically
  static int surf_debug = 0;
  if (surf_debug++ % 600 == 0) {
    if (surface != ap->main_surface) { // Only log floating windows
      printf("[Stygian VK] Surface Extent: %dx%d\n", surface->extent.width,
             surface->extent.height);
    }
  }
  VkRect2D scissor = {{0, 0}, surface->extent};
  vkCmdSetViewport(surface->command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(surface->command_buffer, 0, 1, &scissor);

  // Bind pipeline and descriptors
  vkCmdBindPipeline(surface->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ap->graphics_pipeline);
  vkCmdBindDescriptorSets(surface->command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, ap->pipeline_layout,
                          0, 1, &ap->descriptor_set, 0, NULL);
  surface->frame_active = true;
}

void stygian_ap_surface_submit(StygianAP *ap, StygianAPSurface *surface,
                               const StygianSoAHot *soa_hot, uint32_t count) {
  if (!ap || !surface || !surface->frame_active || count == 0)
    return;

  // SoA data is uploaded via stygian_ap_submit_soa(); no AoS memcpy needed.

  // Push viewport size constant (LOGICAL SIZE for correct projection!)
  // If physical != logical (DPI scaling), we must use logical size here
  // because the UI elements were laid out in logical coordinates.
  int log_w, log_h;
  if (surface->window) {
    stygian_window_get_size(surface->window, &log_w, &log_h);
  } else {
    // Fallback if no window attached (?)
    log_w = surface->extent.width;
    log_h = surface->extent.height;
  }

  {
    StygianVKPushConstants pc;
    fill_push_constants(ap, (float)log_w, (float)log_h, &pc);
    vkCmdPushConstants(surface->command_buffer, ap->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);
  }

  // Bind vertex buffer and draw
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(surface->command_buffer, 0, 1, &ap->vertex_buffer,
                         &offset);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)surface->extent.width,
      .height = (float)surface->extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = surface->extent,
  };
  vkCmdSetViewport(surface->command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(surface->command_buffer, 0, 1, &scissor);

  vkCmdDraw(surface->command_buffer, 6, count, 0, 0);
}

void stygian_ap_surface_end(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface || !surface->frame_active)
    return;

  vkCmdEndRenderPass(surface->command_buffer);
  vkEndCommandBuffer(surface->command_buffer);
}

void stygian_ap_surface_swap(StygianAP *ap, StygianAPSurface *surface) {
  if (!ap || !surface || !surface->frame_active)
    return;

  // Submit command buffer
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &surface->image_available,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &surface->command_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &surface->render_finished,
  };

  VkResult res =
      vkQueueSubmit(ap->graphics_queue, 1, &submit_info, surface->in_flight);
  if (res != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to submit draw command buffer: %d\n", res);
  }

  // Present
  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &surface->render_finished,
      .swapchainCount = 1,
      .pSwapchains = &surface->swapchain,
      .pImageIndices = &surface->current_image,
  };

  res = vkQueuePresentKHR(ap->graphics_queue, &present_info);
  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
    surface->swapchain_needs_recreate = true;
  } else if (res != VK_SUCCESS) {
    printf("[Stygian AP VK] Failed to present: %d\n", res);
  }

  // Wait for present to finish (simple sync for multi-window stability)
  // vkQueueWaitIdle(ap->graphics_queue);
  surface->frame_active = false;
}

StygianAPSurface *stygian_ap_get_main_surface(StygianAP *ap) {
  return ap ? ap->main_surface : NULL;
}


# stygian_widgets.c
// stygian_widgets.c - Widget Implementation for Stygian UI Library
// Immediate-mode widgets built on Stygian rendering primitives

#include "stygian_widgets.h"
#include "../include/stygian_clipboard.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// Internal State (Immediate-Mode)
// ============================================================================

// Queue for char events
#define MAX_CHAR_EVENTS 32
#define MAX_KEY_EVENTS 32
#define MAX_WIDGET_REGIONS 4096

typedef struct WidgetRegion {
  float x;
  float y;
  float w;
  float h;
  uint32_t flags;
} WidgetRegion;

typedef struct WidgetState {
  StygianContext *ctx;
  uint32_t hot_id;    // Widget under mouse
  uint32_t active_id; // Widget being interacted with
  uint32_t focus_id;  // Widget with keyboard focus
  int mouse_x, mouse_y;
  bool mouse_down;
  bool mouse_was_down;
  bool right_down;
  bool right_was_down;
  bool mouse_pressed;
  bool mouse_released;
  bool right_pressed;
  bool right_released;
  bool mouse_pressed_mutating;
  bool right_pressed_mutating;

  // Buffered Inputs (cleared each frame)
  uint32_t char_events[MAX_CHAR_EVENTS];
  int char_count;

  struct {
    StygianKey key;
    bool down;
    uint32_t mods;
  } key_events[MAX_KEY_EVENTS];
  int key_count;

  // Scroll
  float scroll_dx, scroll_dy;
  float mouse_dx, mouse_dy; // Calculated internally
  uint32_t repaint_hz_request;

  // Focus navigation (tab cycle)
  uint32_t focus_order_prev[1024];
  uint32_t focus_order_curr[1024];
  uint16_t focus_count_prev;
  uint16_t focus_count_curr;
  bool nav_prepared;
  bool nav_tab_pressed;
  bool nav_shift_pressed;
  bool nav_enter_pressed;
  bool nav_space_pressed;
  bool nav_left_pressed;
  bool nav_right_pressed;
  bool nav_up_pressed;
  bool nav_down_pressed;

  // Clipboard (simulated or real)
  char *clipboard_text;

  // Previous-frame interactive regions for strict input routing.
  WidgetRegion regions_prev[MAX_WIDGET_REGIONS];
  WidgetRegion regions_curr[MAX_WIDGET_REGIONS];
  uint16_t region_count_prev;
  uint16_t region_count_curr;
  bool has_region_snapshot;
  uint64_t impact_pointer_only_events;
  uint64_t impact_mutated_events;
  uint64_t impact_request_events;
} WidgetState;

static WidgetState g_widget_state = {0};

static double perf_now_seconds(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

// Widget ID hash: uses x,y coordinates (floats) and label string for stable IDs
// CRITICAL: DO NOT hash &x (stack address) - that causes ghost click
// collisions!
static uint32_t widget_id(float x, float y, const char *str) {
  uint32_t hash = 2166136261u;
  // Hash x coordinate as bytes
  const unsigned char *px = (const unsigned char *)&x;
  for (size_t i = 0; i < sizeof(float); i++) {
    hash ^= px[i];
    hash *= 16777619;
  }
  // Hash y coordinate as bytes
  const unsigned char *py = (const unsigned char *)&y;
  for (size_t i = 0; i < sizeof(float); i++) {
    hash ^= py[i];
    hash *= 16777619;
  }
  // Hash string if provided
  if (str) {
    while (*str) {
      hash ^= (unsigned char)*str++;
      hash *= 16777619;
    }
  }
  return hash;
}

static bool point_in_rect(float px, float py, float x, float y, float w,
                          float h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

static void widget_register_region_internal(float x, float y, float w, float h,
                                            uint32_t flags) {
  WidgetRegion *r;
  if (w <= 0.0f || h <= 0.0f || flags == 0u)
    return;
  if (g_widget_state.region_count_curr >= MAX_WIDGET_REGIONS)
    return;
  r = &g_widget_state.regions_curr[g_widget_state.region_count_curr++];
  r->x = x;
  r->y = y;
  r->w = w;
  r->h = h;
  r->flags = flags;
}

static bool widget_region_hit_prev(float px, float py, uint32_t need_flags) {
  int i;
  if (need_flags == 0u)
    return false;
  if (!g_widget_state.has_region_snapshot)
    return false;
  for (i = (int)g_widget_state.region_count_prev - 1; i >= 0; --i) {
    const WidgetRegion *r = &g_widget_state.regions_prev[i];
    if ((r->flags & need_flags) != 0u &&
        point_in_rect(px, py, r->x, r->y, r->w, r->h)) {
      return true;
    }
  }
  return false;
}

static bool widget_mouse_pressed(void) {
  return g_widget_state.mouse_pressed ||
         (g_widget_state.mouse_down && !g_widget_state.mouse_was_down);
}

static bool widget_mouse_released(void) {
  return g_widget_state.mouse_released ||
         (!g_widget_state.mouse_down && g_widget_state.mouse_was_down);
}

static bool widget_right_pressed(void) {
  return g_widget_state.right_pressed ||
         (g_widget_state.right_down && !g_widget_state.right_was_down);
}

static void widget_nav_prepare(void) {
  int i;
  if (g_widget_state.nav_prepared)
    return;

  g_widget_state.nav_prepared = true;
  g_widget_state.nav_tab_pressed = false;
  g_widget_state.nav_shift_pressed = false;
  g_widget_state.nav_enter_pressed = false;
  g_widget_state.nav_space_pressed = false;
  g_widget_state.nav_left_pressed = false;
  g_widget_state.nav_right_pressed = false;
  g_widget_state.nav_up_pressed = false;
  g_widget_state.nav_down_pressed = false;

  for (i = 0; i < g_widget_state.key_count; i++) {
    if (!g_widget_state.key_events[i].down)
      continue;
    switch (g_widget_state.key_events[i].key) {
    case STYGIAN_KEY_TAB:
      g_widget_state.nav_tab_pressed = true;
      if (g_widget_state.key_events[i].mods & STYGIAN_MOD_SHIFT)
        g_widget_state.nav_shift_pressed = true;
      break;
    case STYGIAN_KEY_ENTER:
      g_widget_state.nav_enter_pressed = true;
      break;
    case STYGIAN_KEY_SPACE:
      g_widget_state.nav_space_pressed = true;
      break;
    case STYGIAN_KEY_LEFT:
      g_widget_state.nav_left_pressed = true;
      break;
    case STYGIAN_KEY_RIGHT:
      g_widget_state.nav_right_pressed = true;
      break;
    case STYGIAN_KEY_UP:
      g_widget_state.nav_up_pressed = true;
      break;
    case STYGIAN_KEY_DOWN:
      g_widget_state.nav_down_pressed = true;
      break;
    default:
      break;
    }
  }

  if (g_widget_state.nav_tab_pressed && g_widget_state.focus_count_prev > 0u) {
    int i;
    int focused_index = -1;
    for (i = 0; i < (int)g_widget_state.focus_count_prev; i++) {
      if (g_widget_state.focus_order_prev[i] == g_widget_state.focus_id) {
        focused_index = i;
        break;
      }
    }
    if (focused_index < 0) {
      g_widget_state.focus_id = g_widget_state.focus_order_prev[0];
    } else {
      int next =
          g_widget_state.nav_shift_pressed
              ? (focused_index - 1 + (int)g_widget_state.focus_count_prev) %
                    (int)g_widget_state.focus_count_prev
              : (focused_index + 1) % (int)g_widget_state.focus_count_prev;
      g_widget_state.focus_id = g_widget_state.focus_order_prev[next];
    }
  }
}

static void widget_register_focusable(uint32_t id) {
  uint16_t i;
  if (id == 0)
    return;
  for (i = 0; i < g_widget_state.focus_count_curr; i++) {
    if (g_widget_state.focus_order_curr[i] == id)
      return;
  }
  if (g_widget_state.focus_count_curr <
      (uint16_t)(sizeof(g_widget_state.focus_order_curr) /
                 sizeof(g_widget_state.focus_order_curr[0]))) {
    g_widget_state.focus_order_curr[g_widget_state.focus_count_curr++] = id;
  }
}

void stygian_widgets_begin_frame(StygianContext *ctx) {
  StygianWindow *win = stygian_get_window(ctx);
  uint16_t i;

  g_widget_state.ctx = ctx;

  // Clear event buffers
  g_widget_state.char_count = 0;
  g_widget_state.key_count = 0;
  g_widget_state.scroll_dx = 0;
  g_widget_state.scroll_dy = 0;
  g_widget_state.repaint_hz_request = 0;
  g_widget_state.region_count_curr = 0;

  // Reset movement delta; only real mouse-move events set dx/dy.
  int nx, ny;
  g_widget_state.mouse_dx = 0.0f;
  g_widget_state.mouse_dy = 0.0f;
  stygian_mouse_pos(win, &nx, &ny);
  g_widget_state.mouse_x = nx;
  g_widget_state.mouse_y = ny;

  g_widget_state.mouse_was_down = g_widget_state.mouse_down;
  g_widget_state.mouse_down = stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
  g_widget_state.right_was_down = g_widget_state.right_down;
  g_widget_state.right_down = stygian_mouse_down(win, STYGIAN_MOUSE_RIGHT);
  g_widget_state.mouse_pressed = false;
  g_widget_state.mouse_released = false;
  g_widget_state.right_pressed = false;
  g_widget_state.right_released = false;
  g_widget_state.mouse_pressed_mutating = false;
  g_widget_state.right_pressed_mutating = false;

  // Carry focus traversal order from previous frame.
  g_widget_state.focus_count_prev = g_widget_state.focus_count_curr;
  for (i = 0; i < g_widget_state.focus_count_prev; i++) {
    g_widget_state.focus_order_prev[i] = g_widget_state.focus_order_curr[i];
  }
  g_widget_state.focus_count_curr = 0;
  g_widget_state.nav_prepared = false;

  // Reset hot widget each frame
  g_widget_state.hot_id = 0;

  // Drag interaction stays event-driven; avoid periodic repaint tails.
}

StygianWidgetEventImpact
stygian_widgets_process_event_ex(StygianContext *ctx, const StygianEvent *e) {
  StygianWidgetEventImpact impact = STYGIAN_IMPACT_NONE;
  bool should_repaint = false;
  bool hit_region = false;
  bool mutating_region = false;
  if (ctx)
    g_widget_state.ctx = ctx;
  if (!e)
    return impact;
  if (e->type == STYGIAN_EVENT_MOUSE_MOVE) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.mouse_dx = (float)e->mouse_move.dx;
    g_widget_state.mouse_dy = (float)e->mouse_move.dy;
    g_widget_state.mouse_x = e->mouse_move.x;
    g_widget_state.mouse_y = e->mouse_move.y;
  } else if (e->type == STYGIAN_EVENT_MOUSE_DOWN) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.mouse_x = e->mouse_button.x;
    g_widget_state.mouse_y = e->mouse_button.y;
    if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      hit_region = widget_region_hit_prev((float)e->mouse_button.x,
                                          (float)e->mouse_button.y,
                                          STYGIAN_WIDGET_REGION_POINTER_RIGHT);
      mutating_region = widget_region_hit_prev(
          (float)e->mouse_button.x, (float)e->mouse_button.y,
          STYGIAN_WIDGET_REGION_POINTER_RIGHT_MUTATES);
    } else {
      hit_region = widget_region_hit_prev((float)e->mouse_button.x,
                                          (float)e->mouse_button.y,
                                          STYGIAN_WIDGET_REGION_POINTER_LEFT);
      mutating_region = widget_region_hit_prev(
          (float)e->mouse_button.x, (float)e->mouse_button.y,
          STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
    }
    if (e->mouse_button.button == STYGIAN_MOUSE_LEFT) {
      g_widget_state.mouse_down = true;
      if (hit_region)
        g_widget_state.mouse_pressed = true;
      if (mutating_region)
        g_widget_state.mouse_pressed_mutating = true;
    } else if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      g_widget_state.right_down = true;
      if (hit_region)
        g_widget_state.right_pressed = true;
      if (mutating_region)
        g_widget_state.right_pressed_mutating = true;
    } else {
      // Middle/other buttons do not own widget state, but can still wake a
      // frame for region-bound behaviors (e.g. graph pan).
    }
    // Pointer down requests only evaluation for known interactive regions.
    // Mutation remains separately tracked and must be emitted by widget logic.
    should_repaint = hit_region || mutating_region;
    if (should_repaint) {
      impact |= STYGIAN_IMPACT_REQUEST_EVAL;
    }
  } else if (e->type == STYGIAN_EVENT_MOUSE_UP) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.mouse_x = e->mouse_button.x;
    g_widget_state.mouse_y = e->mouse_button.y;
    if (e->mouse_button.button == STYGIAN_MOUSE_LEFT) {
      g_widget_state.mouse_down = false;
    } else if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      g_widget_state.right_down = false;
    }
    // Release repaint is valid only when we had an in-flight interaction.
    if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
      should_repaint = (g_widget_state.active_id != 0u ||
                        g_widget_state.right_pressed_mutating);
    } else {
      should_repaint = (g_widget_state.active_id != 0u ||
                        g_widget_state.mouse_pressed_mutating);
    }
    if (should_repaint) {
      if (e->mouse_button.button == STYGIAN_MOUSE_LEFT) {
        g_widget_state.mouse_released = true;
      } else if (e->mouse_button.button == STYGIAN_MOUSE_RIGHT) {
        g_widget_state.right_released = true;
      }
    }
    if (should_repaint) {
      impact |= STYGIAN_IMPACT_REQUEST_EVAL;
    }
  } else if (e->type == STYGIAN_EVENT_CHAR) {
    // Character input is meaningful only when a widget currently owns focus.
    if (g_widget_state.focus_id != 0u) {
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
      impact |= STYGIAN_IMPACT_MUTATED_STATE;
      if (g_widget_state.char_count < MAX_CHAR_EVENTS) {
        g_widget_state.char_events[g_widget_state.char_count++] =
            e->chr.codepoint;
      }
      if (g_widget_state.ctx) {
        stygian_set_repaint_source(g_widget_state.ctx, "event-char");
        stygian_request_repaint_after_ms(g_widget_state.ctx, 0u);
      }
    }
  } else if (e->type == STYGIAN_EVENT_KEY_DOWN ||
             e->type == STYGIAN_EVENT_KEY_UP) {
    bool key_affects_ui =
        (g_widget_state.focus_id != 0u || e->key.key == STYGIAN_KEY_TAB ||
         e->key.key == STYGIAN_KEY_ENTER || e->key.key == STYGIAN_KEY_SPACE ||
         e->key.key == STYGIAN_KEY_ESCAPE);
    if (key_affects_ui) {
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
      if (e->type == STYGIAN_EVENT_KEY_DOWN) {
        impact |= STYGIAN_IMPACT_MUTATED_STATE;
      }
      if (g_widget_state.key_count < MAX_KEY_EVENTS) {
        g_widget_state.key_events[g_widget_state.key_count].key = e->key.key;
        g_widget_state.key_events[g_widget_state.key_count].down =
            (e->type == STYGIAN_EVENT_KEY_DOWN);
        g_widget_state.key_events[g_widget_state.key_count].mods = e->key.mods;
        g_widget_state.key_count++;
      }
      if (g_widget_state.ctx) {
        stygian_set_repaint_source(g_widget_state.ctx, "event-key");
        stygian_request_repaint_after_ms(g_widget_state.ctx, 0u);
      }
    }
  } else if (e->type == STYGIAN_EVENT_SCROLL) {
    impact |= STYGIAN_IMPACT_POINTER_ONLY;
    g_widget_state.scroll_dx += e->scroll.dx;
    g_widget_state.scroll_dy += e->scroll.dy;
    // Scroll drives redraw only when the pointer is over a registered
    // scroll-capable region from the previous frame.
    should_repaint = (e->scroll.dx != 0.0f || e->scroll.dy != 0.0f) &&
                     widget_region_hit_prev((float)g_widget_state.mouse_x,
                                            (float)g_widget_state.mouse_y,
                                            STYGIAN_WIDGET_REGION_SCROLL);
    if (should_repaint)
      impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
  } else if (e->type == STYGIAN_EVENT_RESIZE) {
    impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    impact |= STYGIAN_IMPACT_LAYOUT_CHANGED;
    if (g_widget_state.ctx) {
      stygian_set_repaint_source(g_widget_state.ctx, "event-resize");
      stygian_request_repaint_after_ms(g_widget_state.ctx, 0u);
    }
  } else if (e->type == STYGIAN_EVENT_TICK) {
    impact |= STYGIAN_IMPACT_REQUEST_REPAINT;
    if (g_widget_state.ctx) {
      stygian_set_repaint_source(g_widget_state.ctx, "event-tick");
    }
  }
  if (impact & STYGIAN_IMPACT_POINTER_ONLY)
    g_widget_state.impact_pointer_only_events++;
  if (impact & STYGIAN_IMPACT_MUTATED_STATE)
    g_widget_state.impact_mutated_events++;
  if ((impact & STYGIAN_IMPACT_REQUEST_REPAINT) ||
      (impact & STYGIAN_IMPACT_REQUEST_EVAL))
    g_widget_state.impact_request_events++;
  return impact;
}

// User must call this for every event in their loop
void stygian_widgets_process_event(StygianContext *ctx, StygianEvent *e) {
  (void)stygian_widgets_process_event_ex(ctx, e);
}

void stygian_widgets_register_region(float x, float y, float w, float h,
                                     StygianWidgetRegionFlags flags) {
  widget_register_region_internal(x, y, w, h, flags);
}

void stygian_widgets_commit_regions(void) {
  uint16_t i;
  g_widget_state.region_count_prev = g_widget_state.region_count_curr;
  if (g_widget_state.region_count_prev > MAX_WIDGET_REGIONS)
    g_widget_state.region_count_prev = MAX_WIDGET_REGIONS;
  for (i = 0; i < g_widget_state.region_count_prev; ++i) {
    g_widget_state.regions_prev[i] = g_widget_state.regions_curr[i];
  }
  g_widget_state.has_region_snapshot = true;
}

float stygian_widgets_scroll_dx(void) { return g_widget_state.scroll_dx; }

float stygian_widgets_scroll_dy(void) { return g_widget_state.scroll_dy; }

void stygian_widgets_request_repaint_hz(uint32_t hz) {
  if (hz == 0u)
    return;
  if (g_widget_state.ctx) {
    stygian_request_repaint_hz(g_widget_state.ctx, hz);
  }
  if (g_widget_state.repaint_hz_request < hz)
    g_widget_state.repaint_hz_request = hz;
}

uint32_t stygian_widgets_repaint_wait_ms(uint32_t idle_wait_ms) {
  if (g_widget_state.ctx) {
    return stygian_next_repaint_wait_ms(g_widget_state.ctx, idle_wait_ms);
  }
  uint32_t hz;
  uint32_t ms;
  if (idle_wait_ms == 0u)
    idle_wait_ms = 1u;
  hz = g_widget_state.repaint_hz_request;
  if (hz == 0u)
    return idle_wait_ms;
  ms = 1000u / hz;
  if (ms < 1u)
    ms = 1u;
  if (ms > idle_wait_ms)
    ms = idle_wait_ms;
  return ms;
}

bool stygian_widgets_wants_repaint(void) {
  if (g_widget_state.ctx) {
    return stygian_has_pending_repaint(g_widget_state.ctx);
  }
  return g_widget_state.repaint_hz_request > 0u;
}

static void perf_history_push(StygianPerfWidget *state, float frame_ms) {
  uint32_t idx;
  if (!state)
    return;
  idx = state->history_head % STYGIAN_PERF_HISTORY_MAX;
  state->history_ms[idx] = frame_ms;
  state->history_head = (state->history_head + 1u) % STYGIAN_PERF_HISTORY_MAX;
  if (state->history_count < STYGIAN_PERF_HISTORY_MAX)
    state->history_count++;
}

static struct {
  StygianPerfWidget *active;
  float drag_off_x;
  float drag_off_y;
} g_perf_drag = {0};

void stygian_perf_widget(StygianContext *ctx, StygianFont font,
                         StygianPerfWidget *state) {
  char line[256];
  const char *renderer = NULL;
  float x, y, w, h;
  float line_y;
  float line_h;
  float line_size;
  float header_h;
  float latest_ms = 0.0f;
  float frame_target_ms = 16.7f;
  uint32_t draw_calls;
  uint32_t elem_count;
  uint32_t dirty_count;
  uint32_t scope_replay_hits;
  uint32_t scope_replay_misses;
  uint32_t scope_replay_forced;
  uint32_t clip_count;
  uint32_t active_elem_count;
  uint32_t element_capacity;
  uint32_t free_elem_count;
  uint32_t font_count;
  uint32_t inline_emoji_count;
  uint16_t clip_capacity;
  uint32_t hot, active, focus;
  uint64_t pointer_only_events;
  uint64_t mutated_events;
  uint64_t request_events;
  uint32_t window_samples;
  bool repaint_pending;
  uint32_t repaint_wait_ms;
  uint32_t repaint_flags;
  const char *repaint_source;
  uint32_t upload_bytes;
  uint32_t upload_ranges;
  float build_ms;
  float submit_ms;
  float present_ms;
  float gpu_ms;
  bool triad_mounted;
  bool triad_info_ok;
  StygianTriadPackInfo triad_info;
  double now_s;
  int vp_w = 0;
  int vp_h = 0;
  StygianWindow *win = NULL;
  bool over_header;
  bool dragging;
  bool interacting;
  uint32_t idle_hz;
  uint32_t active_hz;
  uint32_t stress_hz;
  uint32_t target_idle_hz;
  uint32_t sample_hz;
  uint32_t frame_budget_hz;
  float budget_ms;
  float sample_ms;
  float wall_ms = 0.0f;
  double sample_dt_s;
  double elapsed_s;
  uint32_t sample_steps;
  float wall_fps;
  bool draw_memory;
  bool draw_glyphs;
  bool draw_triad;
  bool draw_input;
  int max_lines;
  int slots_left;

  if (!ctx || !state || !state->enabled)
    return;

  x = state->x;
  y = state->y;
  w = state->w;
  h = state->h;
  header_h = state->compact_mode ? 20.0f : 24.0f;
  widget_register_region_internal(x, y, w, header_h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  line_h = state->compact_mode ? 14.0f : 16.0f;
  line_size = state->compact_mode ? 11.0f : 12.0f;
  renderer = state->renderer_name ? state->renderer_name : "unknown";
  triad_info.version = 0u;
  triad_info.encoding = 0u;
  triad_info.tier = 0u;
  triad_info.entry_count = 0u;
  triad_info.data_offset = 0u;
  win = stygian_get_window(ctx);
  if (win)
    stygian_window_get_size(win, &vp_w, &vp_h);

  over_header = point_in_rect((float)g_widget_state.mouse_x,
                              (float)g_widget_state.mouse_y, x, y, w, header_h);
  if (over_header && widget_mouse_pressed()) {
    g_perf_drag.active = state;
    g_perf_drag.drag_off_x = (float)g_widget_state.mouse_x - x;
    g_perf_drag.drag_off_y = (float)g_widget_state.mouse_y - y;
  }
  if (g_perf_drag.active == state) {
    if (g_widget_state.mouse_down) {
      state->x = (float)g_widget_state.mouse_x - g_perf_drag.drag_off_x;
      state->y = (float)g_widget_state.mouse_y - g_perf_drag.drag_off_y;
      if (vp_w > 0 && vp_h > 0) {
        if (state->x < 0.0f)
          state->x = 0.0f;
        if (state->y < 0.0f)
          state->y = 0.0f;
        if (state->x + state->w > (float)vp_w)
          state->x = (float)vp_w - state->w;
        if (state->y + state->h > (float)vp_h)
          state->y = (float)vp_h - state->h;
      }
      x = state->x;
      y = state->y;
    } else {
      g_perf_drag.active = NULL;
    }
  }

  dragging = (g_perf_drag.active == state) && g_widget_state.mouse_down;
  // Keep high-rate only for active drag/manipulation, not passive clicks.
  interacting = dragging;
  idle_hz =
      state->idle_hz > 0u ? state->idle_hz : (state->show_graph ? 30u : 10u);
  active_hz = state->active_hz > 0u ? state->active_hz : 60u;
  stress_hz = state->max_stress_hz;
  target_idle_hz = idle_hz;
  if (state->stress_mode && stress_hz > 0u) {
    if (stress_hz < idle_hz)
      stress_hz = idle_hz;
    target_idle_hz = stress_hz;
  }
  if (state->history_count > 0u) {
    uint32_t last_idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - 1u) %
                        STYGIAN_PERF_HISTORY_MAX;
    latest_ms = state->history_ms[last_idx];
  }
  frame_budget_hz = target_idle_hz > 0u ? target_idle_hz : 1u;
  budget_ms = 1000.0f / (float)frame_budget_hz;
  if (latest_ms > budget_ms * 1.35f) {
    if (state->budget_miss_count < 100000u)
      state->budget_miss_count++;
  } else if (state->budget_miss_count > 0u) {
    state->budget_miss_count--;
  }
  if (target_idle_hz > idle_hz && state->budget_miss_count > 10u) {
    target_idle_hz = target_idle_hz / 2u;
    if (target_idle_hz < idle_hz)
      target_idle_hz = idle_hz;
  }
  if (ctx) {
    stygian_set_repaint_source(ctx, interacting ? "diag-active" : "diag-idle");
  }
  stygian_widgets_request_repaint_hz(interacting ? active_hz : target_idle_hz);

  draw_calls = stygian_get_last_frame_draw_calls(ctx);
  elem_count = stygian_get_last_frame_element_count(ctx);
  dirty_count = 0u;
  scope_replay_hits = stygian_get_last_frame_scope_replay_hits(ctx);
  scope_replay_misses = stygian_get_last_frame_scope_replay_misses(ctx);
  scope_replay_forced = stygian_get_last_frame_scope_forced_rebuilds(ctx);
  clip_count = stygian_get_last_frame_clip_count(ctx);
  active_elem_count = stygian_get_active_element_count(ctx);
  element_capacity = stygian_get_element_capacity(ctx);
  free_elem_count = stygian_get_free_element_count(ctx);
  font_count = stygian_get_font_count(ctx);
  inline_emoji_count = stygian_get_inline_emoji_cache_count(ctx);
  clip_capacity = stygian_get_clip_capacity(ctx);
  triad_mounted = stygian_triad_is_mounted(ctx);
  triad_info_ok =
      triad_mounted && stygian_triad_get_pack_info(ctx, &triad_info);
  repaint_pending = stygian_has_pending_repaint(ctx);
  repaint_wait_ms = stygian_next_repaint_wait_ms(ctx, 250u);
  repaint_flags = stygian_get_repaint_reason_flags(ctx);
  repaint_source = stygian_get_repaint_source(ctx);
  upload_bytes = stygian_get_last_frame_upload_bytes(ctx);
  upload_ranges = stygian_get_last_frame_upload_ranges(ctx);
  dirty_count = upload_ranges;
  build_ms = stygian_get_last_frame_build_ms(ctx);
  submit_ms = stygian_get_last_frame_submit_ms(ctx);
  present_ms = stygian_get_last_frame_present_ms(ctx);
  gpu_ms = stygian_get_last_frame_gpu_ms(ctx);

  if (state->history_count > 0u) {
    uint32_t last_idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - 1u) %
                        STYGIAN_PERF_HISTORY_MAX;
    latest_ms = state->history_ms[last_idx];
  }

  sample_hz = interacting ? active_hz : target_idle_hz;
  if (sample_hz == 0u)
    sample_hz = 30u;
  sample_dt_s = 1.0 / (double)sample_hz;
  sample_ms = (gpu_ms > 0.0f && gpu_ms < 1000.0f) ? gpu_ms
                                                  : (build_ms + submit_ms +
                                                     present_ms);
  if (sample_ms <= 0.0f || sample_ms >= 1000.0f) {
    sample_ms = latest_ms > 0.0f ? latest_ms : 16.7f;
  }
  // Quantize tiny timing noise before filtering to keep the polyline stable.
  sample_ms = floorf(sample_ms * 20.0f + 0.5f) * 0.05f;
  if (state->graph_filtered_ms <= 0.0f) {
    state->graph_filtered_ms = sample_ms;
  } else {
    float alpha = interacting ? 0.18f : 0.10f;
    float max_delta = interacting ? 8.0f : 2.5f;
    float delta = sample_ms - state->graph_filtered_ms;
    if (delta > max_delta)
      delta = max_delta;
    else if (delta < -max_delta)
      delta = -max_delta;
    state->graph_filtered_ms += delta * alpha;
  }
  sample_ms = state->graph_filtered_ms;
  now_s = perf_now_seconds();
  wall_ms = 0.0f;
  wall_fps = 0.0f;
  if (state->last_render_seconds > 0.0) {
    wall_ms = (float)((now_s - state->last_render_seconds) * 1000.0);
    if (wall_ms > 0.0f && wall_ms < 1000.0f) {
      wall_fps = 1000.0f / wall_ms;
      if (state->fps_wall_smoothed <= 0.0f) {
        state->fps_wall_smoothed = wall_fps;
      } else {
        state->fps_wall_smoothed +=
            (wall_fps - state->fps_wall_smoothed) * 0.1f;
      }
    }
  }
  state->last_render_seconds = now_s;
  if (state->last_sample_seconds <= 0.0) {
    state->last_sample_seconds = now_s;
    perf_history_push(state, sample_ms);
    state->fps_smoothed = 1000.0f / sample_ms;
    if (state->fps_wall_smoothed <= 0.0f)
      state->fps_wall_smoothed = state->fps_smoothed;
  } else {
    elapsed_s = now_s - state->last_sample_seconds;
    sample_steps = 0u;
    while (elapsed_s >= sample_dt_s && sample_steps < 8u) {
      float fps = 1000.0f / sample_ms;
      perf_history_push(state, sample_ms);
      if (state->fps_smoothed <= 0.0f) {
        state->fps_smoothed = fps;
      } else {
        state->fps_smoothed += (fps - state->fps_smoothed) * 0.1f;
      }
      state->last_sample_seconds += sample_dt_s;
      elapsed_s -= sample_dt_s;
      sample_steps++;
    }
    if (sample_steps == 8u) {
      state->last_sample_seconds = now_s;
    }
  }

  if (state->history_count > 0u) {
    uint32_t last_idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - 1u) %
                        STYGIAN_PERF_HISTORY_MAX;
    latest_ms = state->history_ms[last_idx];
  }

  window_samples = state->history_window;
  if (window_samples == 0u)
    window_samples = 120u;
  if (window_samples > STYGIAN_PERF_HISTORY_MAX)
    window_samples = STYGIAN_PERF_HISTORY_MAX;
  if (window_samples < 30u)
    window_samples = 30u;
  hot = g_widget_state.hot_id;
  active = g_widget_state.active_id;
  focus = g_widget_state.focus_id;
  pointer_only_events = g_widget_state.impact_pointer_only_events;
  mutated_events = g_widget_state.impact_mutated_events;
  request_events = g_widget_state.impact_request_events;

  // Ensure graph visibility in small widgets by dropping optional lines first.
  draw_memory = state->show_memory;
  draw_glyphs = state->show_glyphs;
  draw_triad = state->show_triad;
  draw_input = state->show_input;
  if (state->show_graph) {
    max_lines = (int)((h - 94.0f) / line_h); // reserve graph area
    if (max_lines < 6)
      max_lines = 6;
    slots_left = max_lines - 6; // after required lines
    if (slots_left <= 0) {
      draw_memory = false;
      draw_glyphs = false;
      draw_triad = false;
      draw_input = false;
    } else {
      if (draw_memory) {
        slots_left--;
      }
      if (draw_glyphs) {
        if (slots_left > 0) {
          slots_left--;
        } else {
          draw_glyphs = false;
        }
      }
      if (draw_triad) {
        if (slots_left > 0) {
          slots_left--;
        } else {
          draw_triad = false;
        }
      }
      if (draw_input) {
        if (slots_left > 0) {
          slots_left--;
        } else {
          draw_input = false;
        }
      }
    }
  }

  stygian_rect_rounded(ctx, x, y, w, h, 0.08f, 0.09f, 0.11f, 0.94f, 6.0f);
  stygian_rect_rounded(ctx, x, y, w, header_h, 0.13f, 0.15f, 0.19f, 0.96f,
                       6.0f);

  snprintf(line, sizeof(line), "Stygian Diagnostics (%s)", renderer);
  stygian_text(ctx, font, line, x + 8.0f, y + 4.0f,
               state->compact_mode ? 12.0f : 13.0f, 0.92f, 0.94f, 0.98f, 1.0f);

  line_y = y + header_h + 6.0f;
  snprintf(line, sizeof(line),
           "Frame: %.2f ms | FPS: %.1f", latest_ms, state->fps_wall_smoothed);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.85f, 0.9f, 0.95f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line),
           "Draw calls: %u | Elements: %u | Dirty ranges: %u",
           draw_calls, elem_count, dirty_count);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line), "Scope replay h/m/f: %u/%u/%u",
           scope_replay_hits, scope_replay_misses, scope_replay_forced);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line),
           "Repaint: %s flags=0x%X pending=%u next_wait=%ums",
           repaint_source ? repaint_source : "none", repaint_flags,
           repaint_pending ? 1u : 0u, repaint_wait_ms);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line),
           "CPU ms: build=%.2f submit=%.2f present=%.2f | GPU ms: %.2f",
           build_ms, submit_ms, present_ms, gpu_ms);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line), "Upload: %u bytes in %u range(s)", upload_bytes,
           upload_ranges);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.8f, 0.86f, 0.92f,
               1.0f);
  line_y += line_h;

  snprintf(line, sizeof(line), "Clip regions: %u / %u", clip_count,
           (uint32_t)clip_capacity);
  stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.82f,
               0.88f, 1.0f);
  line_y += line_h;

  if (draw_memory) {
    snprintf(line, sizeof(line), "Element pool: active=%u free=%u cap=%u",
             active_elem_count, free_elem_count, element_capacity);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.84f,
                 0.90f, 1.0f);
    line_y += line_h;
  }

  if (draw_glyphs) {
    snprintf(line, sizeof(line), "Fonts=%u | Inline emoji cache=%u", font_count,
             inline_emoji_count);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.84f,
                 0.90f, 1.0f);
    line_y += line_h;
  }

  if (draw_triad) {
    if (triad_info_ok) {
      snprintf(line, sizeof(line), "TRIAD mounted: tier=%u entries=%u enc=%u",
               triad_info.tier, triad_info.entry_count, triad_info.encoding);
    } else if (triad_mounted) {
      snprintf(line, sizeof(line), "TRIAD mounted (pack info unavailable)");
    } else {
      snprintf(line, sizeof(line), "TRIAD not mounted");
    }
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.78f, 0.84f,
                 0.90f, 1.0f);
    line_y += line_h;
  }

  if (draw_input) {
    snprintf(
        line, sizeof(line), "Input: mouse(%d,%d) hot=%u active=%u focus=%u",
        g_widget_state.mouse_x, g_widget_state.mouse_y, hot, active, focus);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.76f, 0.8f,
                 0.86f, 1.0f);
    line_y += line_h;
    snprintf(line, sizeof(line), "Event impact ptr/mut/req: %llu/%llu/%llu",
             (unsigned long long)pointer_only_events,
             (unsigned long long)mutated_events,
             (unsigned long long)request_events);
    stygian_text(ctx, font, line, x + 8.0f, line_y, line_size, 0.76f, 0.8f,
                 0.86f, 1.0f);
    line_y += line_h;
  }

  if (state->show_graph && state->history_count > 0u && h > 90.0f) {
    float graph_x = x + 8.0f;
    float graph_w = w - 16.0f;
    float graph_h = h - (line_y - y) - 8.0f;
    float max_ms = state->auto_scale_graph ? 16.7f : 33.3f;
    float prev_px = 0.0f;
    float prev_py = 0.0f;
    bool has_prev = false;
    uint32_t sample_count = state->history_count;
    uint32_t draw_points;
    uint32_t max_segments;
    uint32_t i;
    if (sample_count > window_samples)
      sample_count = window_samples;

    if (graph_h > 8.0f) {
      max_segments = state->graph_max_segments;
      if (max_segments == 0u)
        max_segments = 64u;
      draw_points = sample_count;
      if (draw_points > max_segments + 1u)
        draw_points = max_segments + 1u;

      if (state->auto_scale_graph) {
        float raw_max_ms = 16.7f;
        for (i = 0; i < sample_count; i++) {
          uint32_t idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX -
                          sample_count + i) %
                         STYGIAN_PERF_HISTORY_MAX;
          if (state->history_ms[idx] > raw_max_ms) {
            raw_max_ms = state->history_ms[idx];
          }
        }
        if (raw_max_ms < 8.0f)
          raw_max_ms = 8.0f;
        if (state->graph_scale_ms <= 0.0f) {
          state->graph_scale_ms = raw_max_ms;
        } else if (raw_max_ms > state->graph_scale_ms) {
          // Rise faster to preserve spikes.
          state->graph_scale_ms +=
              (raw_max_ms - state->graph_scale_ms) * 0.25f;
        } else {
          // Decay slower to reduce jitter.
          state->graph_scale_ms +=
              (raw_max_ms - state->graph_scale_ms) * 0.08f;
        }
        max_ms = state->graph_scale_ms;
        if (max_ms < 8.0f)
          max_ms = 8.0f;
        // Quantize to reduce tiny autoscale oscillations.
        max_ms = floorf((max_ms + 1.0f) / 2.0f) * 2.0f;
      }

      stygian_rect(ctx, graph_x, line_y, graph_w, graph_h, 0.05f, 0.06f, 0.08f,
                   0.9f);
      {
        float tt = frame_target_ms / max_ms;
        float ty;
        if (tt < 0.0f)
          tt = 0.0f;
        if (tt > 1.0f)
          tt = 1.0f;
        ty = line_y + graph_h - (tt * graph_h);
        ty = floorf(ty) + 0.5f;
        stygian_line(ctx, graph_x, ty, graph_x + graph_w, ty, 1.0f, 0.65f,
                     0.72f, 0.9f, 0.55f);
      }
      for (i = 0; i < draw_points; i++) {
        uint32_t src_i;
        uint32_t idx;
        float ms;
        float src_f;
        float t;
        float px;
        float py;
        float stress;
        float r = 0.28f, g = 0.90f, b = 0.52f;
        if (draw_points <= 1u) {
          src_i = sample_count - 1u;
        } else {
          src_f = ((float)i * (float)(sample_count - 1u)) /
                  (float)(draw_points - 1u);
          src_i = (uint32_t)(src_f + 0.5f);
          if (src_i >= sample_count)
            src_i = sample_count - 1u;
        }
        idx = (state->history_head + STYGIAN_PERF_HISTORY_MAX - sample_count +
               src_i) %
              STYGIAN_PERF_HISTORY_MAX;
        ms = state->history_ms[idx];
        t = ms / max_ms;
        if (t < 0.0f)
          t = 0.0f;
        if (t > 1.0f)
          t = 1.0f;
        if (draw_points <= 1u) {
          px = graph_x;
        } else {
          px = graph_x + (graph_w * ((float)i / (float)(draw_points - 1u)));
        }
        py = line_y + graph_h - (t * graph_h);
        // Pixel-snap the polyline to avoid shimmering/AA jitter on iGPU.
        px = floorf(px) + 0.5f;
        py = floorf(py) + 0.5f;

        // Smooth color ramp based on stress (no threshold flicker).
        stress = ms / frame_target_ms;
        {
          float u = stress * 0.5f; // 0..2 => 0..1
          if (u < 0.0f)
            u = 0.0f;
          if (u > 1.0f)
            u = 1.0f;
          // green -> yellow -> red
          r = 0.28f + (0.97f - 0.28f) * u;
          g = 0.90f + (0.30f - 0.90f) * u;
          b = 0.52f + (0.23f - 0.52f) * u;
        }
        if (has_prev) {
          stygian_line(ctx, prev_px, prev_py, px, py, 1.0f, r, g, b, 0.95f);
        }
        prev_px = px;
        prev_py = py;
        has_prev = true;
      }
    }
  }
}

void stygian_perf_widget_set_rates(StygianPerfWidget *state, uint32_t graph_hz,
                                   uint32_t text_hz) {
  if (!state)
    return;
  state->idle_hz = graph_hz;
  state->active_hz = graph_hz;
  state->text_hz = text_hz;
}

void stygian_perf_widget_set_enabled(StygianPerfWidget *state, bool enabled) {
  if (!state)
    return;
  state->enabled = enabled;
}

// ============================================================================
// Overlay Widgets
// ============================================================================

typedef struct {
  bool active;
  uint8_t clip_id;
} ModalRuntimeState;

typedef struct {
  bool active;
  StygianContextMenu *menu;
  float x, y, w;
  float item_h;
  float panel_h;
  int item_count;
  int item_cursor;
} ContextMenuRuntimeState;

static ModalRuntimeState g_modal_runtime = {0};
static ContextMenuRuntimeState g_context_menu_runtime = {0};

void stygian_tooltip(StygianContext *ctx, StygianFont font,
                     const StygianTooltip *tooltip) {
  float x;
  float y;
  float w;
  float h = 24.0f;
  float max_w;
  int vp_w = 2000;
  int vp_h = 1200;
  StygianWindow *win;
  if (!ctx || !tooltip || !tooltip->show || !tooltip->text || !tooltip->text[0])
    return;
  win = stygian_get_window(ctx);
  if (win)
    stygian_window_get_size(win, &vp_w, &vp_h);

  max_w = tooltip->max_w > 20.0f ? tooltip->max_w : 320.0f;
  w = stygian_text_width(ctx, font, tooltip->text, 14.0f) + 14.0f;
  if (w > max_w)
    w = max_w;

  x = tooltip->x + 12.0f;
  y = tooltip->y + 16.0f;
  if (x + w > (float)vp_w)
    x = tooltip->x - w - 6.0f;
  if (y + h > (float)vp_h)
    y = tooltip->y - h - 6.0f;
  if (x < 0.0f)
    x = 0.0f;
  if (y < 0.0f)
    y = 0.0f;

  stygian_rect_rounded(ctx, x, y, w, h, 0.08f, 0.09f, 0.12f, 0.96f, 4.0f);
  stygian_text(ctx, font, tooltip->text, x + 7.0f, y + 5.0f, 14.0f, 0.94f,
               0.96f, 1.0f, 1.0f);
}

bool stygian_context_menu_trigger_region(StygianContext *ctx,
                                         StygianContextMenu *state, float x,
                                         float y, float w, float h) {
  bool opened = false;
  (void)ctx;
  if (!state)
    return false;
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_RIGHT_MUTATES);

  if (point_in_rect((float)g_widget_state.mouse_x,
                    (float)g_widget_state.mouse_y, x, y, w, h) &&
      widget_right_pressed()) {
    state->open = true;
    state->x = (float)g_widget_state.mouse_x;
    state->y = (float)g_widget_state.mouse_y;
    opened = true;
  }
  return opened;
}

bool stygian_context_menu_begin(StygianContext *ctx, StygianFont font,
                                StygianContextMenu *state, int item_count) {
  float panel_x;
  float panel_y;
  float panel_w;
  float item_h;
  float panel_h;
  int vp_w = 2000;
  int vp_h = 1200;
  StygianWindow *win;
  if (!ctx || !state || !state->open || item_count <= 0)
    return false;
  win = stygian_get_window(ctx);
  if (win)
    stygian_window_get_size(win, &vp_w, &vp_h);

  panel_w = state->w > 60.0f ? state->w : 180.0f;
  item_h = state->item_h > 18.0f ? state->item_h : 28.0f;
  panel_h = item_h * (float)item_count + 8.0f;
  panel_x = state->x;
  panel_y = state->y;
  if (panel_x + panel_w > (float)vp_w)
    panel_x = (float)vp_w - panel_w;
  if (panel_y + panel_h > (float)vp_h)
    panel_y = (float)vp_h - panel_h;
  if (panel_x < 0.0f)
    panel_x = 0.0f;
  if (panel_y < 0.0f)
    panel_y = 0.0f;

  // While menu is open, allow pointer routing anywhere so outside-click close
  // works without input-driven global repaint leaks in normal mode.
  widget_register_region_internal(0.0f, 0.0f, (float)vp_w, (float)vp_h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  widget_register_region_internal(panel_x, panel_y, panel_w, panel_h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  stygian_rect_rounded(ctx, panel_x, panel_y, panel_w, panel_h, 0.11f, 0.12f,
                       0.14f, 0.97f, 6.0f);
  if (font) {
    stygian_text(ctx, font, "Menu", panel_x + 8.0f, panel_y + 4.0f, 12.0f,
                 0.82f, 0.86f, 0.92f, 1.0f);
  }

  g_context_menu_runtime.active = true;
  g_context_menu_runtime.menu = state;
  g_context_menu_runtime.x = panel_x + 4.0f;
  g_context_menu_runtime.y = panel_y + 4.0f;
  g_context_menu_runtime.w = panel_w - 8.0f;
  g_context_menu_runtime.item_h = item_h;
  g_context_menu_runtime.panel_h = panel_h;
  g_context_menu_runtime.item_count = item_count;
  g_context_menu_runtime.item_cursor = 0;
  return true;
}

bool stygian_context_menu_item(StygianContext *ctx, StygianFont font,
                               StygianContextMenu *state, const char *label,
                               int item_index) {
  float bx;
  float by;
  bool clicked;
  (void)item_index;
  if (!ctx || !state || !label || !g_context_menu_runtime.active ||
      g_context_menu_runtime.menu != state)
    return false;

  bx = g_context_menu_runtime.x;
  by = g_context_menu_runtime.y + (float)g_context_menu_runtime.item_cursor *
                                      g_context_menu_runtime.item_h;
  g_context_menu_runtime.item_cursor++;
  widget_register_region_internal(bx, by, g_context_menu_runtime.w,
                                  g_context_menu_runtime.item_h - 2.0f,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  clicked = stygian_button(ctx, font, label, bx, by, g_context_menu_runtime.w,
                           g_context_menu_runtime.item_h - 2.0f);
  if (clicked)
    state->open = false;
  return clicked;
}

void stygian_context_menu_end(StygianContext *ctx, StygianContextMenu *state) {
  bool inside;
  if (!ctx || !state || !g_context_menu_runtime.active ||
      g_context_menu_runtime.menu != state) {
    g_context_menu_runtime.active = false;
    return;
  }

  inside = point_in_rect(
      (float)g_widget_state.mouse_x, (float)g_widget_state.mouse_y,
      g_context_menu_runtime.x - 4.0f, g_context_menu_runtime.y - 4.0f,
      g_context_menu_runtime.w + 8.0f, g_context_menu_runtime.panel_h);
  if (!inside && widget_mouse_pressed()) {
    state->open = false;
  }

  g_context_menu_runtime.active = false;
}

bool stygian_modal_begin(StygianContext *ctx, StygianFont font,
                         StygianModal *state, float viewport_w,
                         float viewport_h) {
  float mw;
  float mh;
  float mx;
  float my;
  bool pressed;
  bool inside;
  if (!ctx || !state || !state->open)
    return false;

  mw = state->w > 40.0f ? state->w : 420.0f;
  mh = state->h > 40.0f ? state->h : 260.0f;
  mx = (viewport_w - mw) * 0.5f;
  my = (viewport_h - mh) * 0.5f;
  if (state->close_on_backdrop) {
    widget_register_region_internal(0.0f, 0.0f, viewport_w, viewport_h,
                                    STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  } else {
    widget_register_region_internal(mx, my, mw, mh,
                                    STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  }

  stygian_rect(ctx, 0.0f, 0.0f, viewport_w, viewport_h, 0.02f, 0.02f, 0.03f,
               0.70f);
  stygian_rect_rounded(ctx, mx, my, mw, mh, 0.11f, 0.12f, 0.14f, 0.98f, 8.0f);
  stygian_rect_rounded(ctx, mx, my, mw, 28.0f, 0.15f, 0.17f, 0.22f, 1.0f, 8.0f);
  if (font && state->title) {
    stygian_text(ctx, font, state->title, mx + 10.0f, my + 6.0f, 14.0f, 0.95f,
                 0.97f, 1.0f, 1.0f);
  }

  pressed = widget_mouse_pressed();
  inside = point_in_rect((float)g_widget_state.mouse_x,
                         (float)g_widget_state.mouse_y, mx, my, mw, mh);
  if (state->close_on_backdrop && pressed && !inside) {
    state->open = false;
    return false;
  }

  g_modal_runtime.active = true;
  g_modal_runtime.clip_id =
      stygian_clip_push(ctx, mx + 8.0f, my + 32.0f, mw - 16.0f, mh - 40.0f);
  return true;
}

void stygian_modal_end(StygianContext *ctx, StygianModal *state) {
  (void)state;
  if (!ctx || !g_modal_runtime.active)
    return;
  stygian_clip_pop(ctx);
  g_modal_runtime.active = false;
}

// ... (button, etc)

// ... INSIDE stygian_text_area ...
// (Need separate patch for that, this chunk is strictly beginning of file)

// ============================================================================
// Button Widget
// ============================================================================

bool stygian_button(StygianContext *ctx, StygianFont font, const char *label,
                    float x, float y, float w, float h) {
  uint32_t id = widget_id(x, y, label);
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool hovered =
      point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y, x, y, w, h);
  bool focused;
  bool clicked = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  // Update state
  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);

  // Check for click (mouse released on active button) and always clear active
  // state on release to avoid stale captures.
  if (active && widget_mouse_released()) {
    if (hovered) {
      clicked = true;
    }
    g_widget_state.active_id = 0;
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    clicked = true;
  }

  // Render button
  float bg_r = 0.25f, bg_g = 0.25f, bg_b = 0.25f, bg_a = 1.0f;
  if (active) {
    bg_r = 0.15f;
    bg_g = 0.15f;
    bg_b = 0.15f;
  } else if (focused) {
    bg_r = 0.22f;
    bg_g = 0.24f;
    bg_b = 0.30f;
  } else if (hovered) {
    bg_r = 0.35f;
    bg_g = 0.35f;
    bg_b = 0.35f;
  }

  stygian_rect_rounded(ctx, x, y, w, h, bg_r, bg_g, bg_b, bg_a, 4.0f);

  // Render text (centered)
  if (label) {
    float text_w = stygian_text_width(ctx, font, label, 16.0f);
    float text_x = x + (w - text_w) * 0.5f;
    float text_y = y + (h - 16.0f) * 0.5f;
    stygian_text(ctx, font, label, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return clicked;
}

bool stygian_button_ex(StygianContext *ctx, StygianFont font,
                       StygianButton *state, const StygianWidgetStyle *style) {
  uint32_t id = widget_id(state->x, state->y, state->label);
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool focused;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  state->hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                                 state->x, state->y, state->w, state->h);
  state->clicked = false;

  if (state->hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
      state->pressed = true;
    }
  }

  if (widget_mouse_released()) {
    if (state->pressed && state->hovered) {
      state->clicked = true;
    }
    state->pressed = false;
    if (g_widget_state.active_id == id) {
      g_widget_state.active_id = 0;
    }
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    state->clicked = true;
  }

  // Render with custom style
  const float *color = style->bg_color;
  if (state->pressed)
    color = style->active_color;
  else if (state->hovered)
    color = style->hover_color;

  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, color[0],
                       color[1], color[2], color[3], style->border_radius);

  if (state->label) {
    float text_w = stygian_text_width(ctx, font, state->label, 16.0f);
    float text_x = state->x + (state->w - text_w) * 0.5f;
    float text_y = state->y + (state->h - 16.0f) * 0.5f;
    stygian_text(ctx, font, state->label, text_x, text_y, 16.0f,
                 style->text_color[0], style->text_color[1],
                 style->text_color[2], style->text_color[3]);
  }

  return state->clicked;
}

// ============================================================================
// Slider Widget
// ============================================================================

bool stygian_slider(StygianContext *ctx, float x, float y, float w, float h,
                    float *value, float min, float max) {
  uint32_t id = widget_id(x, y, NULL);
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool focused;
  bool hovered =
      point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y, x, y, w, h);
  bool changed = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);

  if (active && widget_mouse_released()) {
    g_widget_state.active_id = 0;
    active = false;
  }

  if (focused &&
      (g_widget_state.nav_left_pressed || g_widget_state.nav_right_pressed ||
       g_widget_state.nav_up_pressed || g_widget_state.nav_down_pressed)) {
    float span = max - min;
    float step = span * 0.01f;
    if (step <= 0.0f)
      step = 0.01f;
    if (g_widget_state.nav_left_pressed || g_widget_state.nav_down_pressed)
      *value -= step;
    if (g_widget_state.nav_right_pressed || g_widget_state.nav_up_pressed)
      *value += step;
    if (*value < min)
      *value = min;
    if (*value > max)
      *value = max;
    changed = true;
  }

  // Update value if dragging
  if (active && g_widget_state.mouse_down) {
    float t = (g_widget_state.mouse_x - x) / w;
    if (t < 0.0f)
      t = 0.0f;
    if (t > 1.0f)
      t = 1.0f;
    float new_value = min + t * (max - min);
    if (new_value != *value) {
      *value = new_value;
      changed = true;
    }
  }

  // Render track
  stygian_rect_rounded(ctx, x, y, w, h, 0.15f, 0.15f, 0.15f, 1.0f, h * 0.5f);

  // Render filled portion
  float t = (*value - min) / (max - min);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  float fill_w = w * t;
  if (fill_w > 0.0f) {
    stygian_rect_rounded(ctx, x, y, fill_w, h, 0.4f, 0.6f, 0.9f, 1.0f,
                         h * 0.5f);
  }

  // Render thumb
  float thumb_size = h * 1.5f;
  float thumb_x = x + fill_w - thumb_size * 0.5f;
  float thumb_y = y + h * 0.5f - thumb_size * 0.5f;

  float thumb_r = 0.5f, thumb_g = 0.7f, thumb_b = 1.0f;
  if (active) {
    thumb_r = 0.3f;
    thumb_g = 0.5f;
    thumb_b = 0.8f;
  } else if (focused) {
    thumb_r = 0.55f;
    thumb_g = 0.78f;
    thumb_b = 1.0f;
  } else if (hovered) {
    thumb_r = 0.6f;
    thumb_g = 0.8f;
    thumb_b = 1.0f;
  }

  stygian_rect_rounded(ctx, thumb_x, thumb_y, thumb_size, thumb_size, thumb_r,
                       thumb_g, thumb_b, 1.0f, thumb_size * 0.5f);

  return changed;
}

bool stygian_slider_ex(StygianContext *ctx, StygianSlider *state,
                       const StygianWidgetStyle *style) {
  uint32_t id = widget_id(state->x, state->y, NULL);
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool focused;
  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               state->x, state->y, state->w, state->h);
  bool changed = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
      state->dragging = true;
    }
  }

  if (!g_widget_state.mouse_down) {
    state->dragging = false;
    if (g_widget_state.active_id == id) {
      g_widget_state.active_id = 0;
    }
  }

  if (focused &&
      (g_widget_state.nav_left_pressed || g_widget_state.nav_right_pressed ||
       g_widget_state.nav_up_pressed || g_widget_state.nav_down_pressed)) {
    float span = state->max - state->min;
    float step = span * 0.01f;
    if (step <= 0.0f)
      step = 0.01f;
    if (g_widget_state.nav_left_pressed || g_widget_state.nav_down_pressed)
      state->value -= step;
    if (g_widget_state.nav_right_pressed || g_widget_state.nav_up_pressed)
      state->value += step;
    if (state->value < state->min)
      state->value = state->min;
    if (state->value > state->max)
      state->value = state->max;
    changed = true;
  }

  if (state->dragging && g_widget_state.mouse_down) {
    float t = (g_widget_state.mouse_x - state->x) / state->w;
    if (t < 0.0f)
      t = 0.0f;
    if (t > 1.0f)
      t = 1.0f;
    float new_value = state->min + t * (state->max - state->min);
    if (new_value != state->value) {
      state->value = new_value;
      changed = true;
    }
  }

  // Render with custom style
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h,
                       style->bg_color[0], style->bg_color[1],
                       style->bg_color[2], style->bg_color[3],
                       style->border_radius);

  float t = (state->value - state->min) / (state->max - state->min);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  float fill_w = state->w * t;

  if (fill_w > 0.0f) {
    stygian_rect_rounded(ctx, state->x, state->y, fill_w, state->h,
                         style->active_color[0], style->active_color[1],
                         style->active_color[2], style->active_color[3],
                         style->border_radius);
  }

  return changed;
}

// ============================================================================
// Checkbox Widget
// ============================================================================

bool stygian_checkbox(StygianContext *ctx, StygianFont font, const char *label,
                      float x, float y, bool *checked) {
  float box_size = 20.0f;
  uint32_t id = widget_id(x, y, label);
  bool focused;

  // Calculate bounds (box + label)
  float label_w = label ? stygian_text_width(ctx, font, label, 16.0f) : 0.0f;
  float total_w = box_size + (label ? 8.0f + label_w : 0.0f);
  widget_register_region_internal(x, y, total_w, box_size,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               x, y, total_w, box_size);
  bool clicked = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);

  if (active && widget_mouse_released()) {
    if (hovered) {
      *checked = !*checked;
      clicked = true;
    }
    g_widget_state.active_id = 0; // Always clear after release
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    *checked = !*checked;
    clicked = true;
  }

  // Render checkbox box
  float bg_r = 0.2f, bg_g = 0.2f, bg_b = 0.2f;
  if (active) {
    bg_r = 0.15f;
    bg_g = 0.15f;
    bg_b = 0.15f;
  } else if (hovered) {
    bg_r = 0.3f;
    bg_g = 0.3f;
    bg_b = 0.3f;
  }

  stygian_rect_rounded(ctx, x, y, box_size, box_size, bg_r, bg_g, bg_b, 1.0f,
                       3.0f);

  // Render checkmark if checked
  if (*checked) {
    float check_padding = 4.0f;
    stygian_rect_rounded(
        ctx, x + check_padding, y + check_padding, box_size - check_padding * 2,
        box_size - check_padding * 2, 0.4f, 0.7f, 1.0f, 1.0f, 2.0f);
  }

  // Render label
  if (label) {
    float text_x = x + box_size + 8.0f;
    float text_y = y + (box_size - 16.0f) * 0.5f;
    stygian_text(ctx, font, label, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return clicked;
}

// ============================================================================
// Radio Button Widget
// ============================================================================

bool stygian_radio_button(StygianContext *ctx, StygianFont font,
                          const char *label, float x, float y, int *selected,
                          int value) {
  float circle_size = 20.0f;
  // Use selected pointer + value for stable ID across frames
  uint32_t id = widget_id(x, y, label) + (uint32_t)value;
  bool focused;

  // Calculate bounds (circle + label)
  float label_w = 0.0f;
  if (label) {
    if (font) {
      label_w = stygian_text_width(ctx, font, label, 16.0f);
    } else {
      // Estimate when no font (8px per char)
      label_w = strlen(label) * 8.0f;
    }
  }
  float total_w = circle_size + (label_w > 0 ? 8.0f + label_w : 0.0f);
  widget_register_region_internal(x, y, total_w, circle_size,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);

  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               x, y, total_w, circle_size);
  bool clicked = false;

  widget_register_focusable(id);
  widget_nav_prepare();
  focused = (g_widget_state.focus_id == id);

  if (hovered) {
    g_widget_state.hot_id = id;
    if (widget_mouse_pressed()) {
      g_widget_state.active_id = id;
      g_widget_state.focus_id = id;
    }
  }

  bool active = (g_widget_state.active_id == id);
  bool is_selected = (*selected == value);

  // Process click on mouse release
  if (active && widget_mouse_released()) {
    if (hovered) {
      *selected = value;
      clicked = true;
    }
    g_widget_state.active_id = 0; // Always clear after release
  }

  if (focused &&
      (g_widget_state.nav_enter_pressed || g_widget_state.nav_space_pressed)) {
    *selected = value;
    clicked = true;
  }

  // Render radio button circle
  float bg_r = 0.2f, bg_g = 0.2f, bg_b = 0.2f;
  if (active) {
    bg_r = 0.15f;
    bg_g = 0.15f;
    bg_b = 0.15f;
  } else if (hovered) {
    bg_r = 0.3f;
    bg_g = 0.3f;
    bg_b = 0.3f;
  }

  stygian_rect_rounded(ctx, x, y, circle_size, circle_size, bg_r, bg_g, bg_b,
                       1.0f,
                       circle_size / 2.0f); // Full circle

  // Render inner dot if selected
  if (is_selected) {
    float dot_padding = 5.0f;
    stygian_rect_rounded(ctx, x + dot_padding, y + dot_padding,
                         circle_size - dot_padding * 2,
                         circle_size - dot_padding * 2, 0.4f, 0.7f, 1.0f, 1.0f,
                         (circle_size - dot_padding * 2) / 2.0f);
  }

  // Render label
  if (label) {
    float text_x = x + circle_size + 8.0f;
    float text_y = y + (circle_size - 16.0f) * 0.5f;
    stygian_text(ctx, font, label, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return clicked;
}

// ============================================================================
// Text Input Widget
// ============================================================================

bool stygian_text_input(StygianContext *ctx, StygianFont font, float x, float y,
                        float w, float h, char *buffer, int buffer_size) {
  // Stable per-field ID: do NOT hash buffer contents (changes each keystroke).
  uint32_t id = widget_id(x, y, "text_input");
  uintptr_t buf_addr = (uintptr_t)buffer;
  id ^= (uint32_t)(buf_addr & 0xFFFFFFFFu);
  id *= 16777619u;
  id ^= (uint32_t)((buf_addr >> 32) & 0xFFFFFFFFu);
  id *= 16777619u;
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  bool hovered =
      point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y, x, y, w, h);
  bool changed = false;

  widget_register_focusable(id);
  widget_nav_prepare();

  // Focus management
  if (hovered && widget_mouse_pressed()) {
    g_widget_state.focus_id = id;
  } else if (!hovered && widget_mouse_pressed() &&
             g_widget_state.focus_id == id) {
    g_widget_state.focus_id = 0;
  }

  bool focused = (g_widget_state.focus_id == id);

  // Keyboard input handling (single-line input)
  if (focused && buffer && buffer_size > 1) {
    size_t len = strlen(buffer);
    int i;

    // Process key events first (editing/navigation keys).
    for (i = 0; i < g_widget_state.key_count; i++) {
      StygianKey key = g_widget_state.key_events[i].key;
      if (!g_widget_state.key_events[i].down)
        continue;

      if (key == STYGIAN_KEY_BACKSPACE) {
        if (len > 0) {
          buffer[len - 1] = '\0';
          len--;
          changed = true;
        }
      } else if (key == STYGIAN_KEY_DELETE) {
        if (len > 0) {
          buffer[0] = '\0';
          len = 0;
          changed = true;
        }
      } else if (key == STYGIAN_KEY_V &&
                 (g_widget_state.key_events[i].mods & STYGIAN_MOD_CTRL)) {
        char *clip = stygian_clipboard_pop(ctx);
        if (clip) {
          const char *p = clip;
          while (*p && (int)len < (buffer_size - 1)) {
            unsigned char c = (unsigned char)*p++;
            // Keep insertion path simple/consistent with current single-line
            // handling.
            if (c >= 32 && c <= 0x7E) {
              buffer[len++] = (char)c;
              changed = true;
            }
          }
          buffer[len] = '\0';
          free(clip);
        }
      }
    }

    // Process character input (UTF-32 codepoints from event queue).
    // Keep this simple for now: BMP/ASCII append path for chat input.
    for (i = 0; i < g_widget_state.char_count; i++) {
      uint32_t cp = g_widget_state.char_events[i];
      if (cp >= 32 && cp <= 0x7E) {
        if ((int)len < (buffer_size - 1)) {
          buffer[len++] = (char)cp;
          buffer[len] = '\0';
          changed = true;
        }
      }
    }
  }

  // Render background
  float bg_r = 0.15f, bg_g = 0.15f, bg_b = 0.15f;
  if (focused) {
    bg_r = 0.2f;
    bg_g = 0.2f;
    bg_b = 0.25f;
  } else if (hovered) {
    bg_r = 0.18f;
    bg_g = 0.18f;
    bg_b = 0.18f;
  }

  stygian_rect_rounded(ctx, x, y, w, h, bg_r, bg_g, bg_b, 1.0f, 4.0f);

  // Render border if focused
  if (focused) {
    // TODO: Use STYGIAN_RECT_OUTLINE when available
    // For now, just render a subtle highlight
    stygian_rect_rounded(ctx, x - 1, y - 1, w + 2, h + 2, 0.4f, 0.6f, 0.9f,
                         0.3f, 4.0f);
  }

  // Render text
  if (buffer && buffer[0]) {
    float text_x = x + 8.0f;
    float text_y = y + (h - 16.0f) * 0.5f;
    stygian_text(ctx, font, buffer, text_x, text_y, 16.0f, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  // Render cursor if focused
  if (focused) {
    float cursor_x = x + 8.0f;
    if (buffer && buffer[0]) {
      cursor_x += stygian_text_width(ctx, font, buffer, 16.0f);
    }
    float cursor_y = y + 4.0f;
    float cursor_h = h - 8.0f;

    // Blinking cursor (simple version - always visible for now)
    stygian_rect(ctx, cursor_x, cursor_y, 2.0f, cursor_h, 1.0f, 1.0f, 1.0f,
                 1.0f);
  }

  return changed;
}

// ============================================================================
// Text Area Widget (Multiline)
// ============================================================================

// ============================================================================
// Text Area Logic (Wrapping & Interaction)
// ============================================================================

typedef struct {
  StygianContext *ctx;
  StygianFont font;
  const char *text;
  float max_w;

  // State
  const char *p; // Current char pointer
  const char *line_start;
  float current_w; // Width accumulated for current line
  float y;         // Current Y offset
  int line_index;
  bool done;

  // Cached ASCII glyph advance widths (eliminates per-char font lookups)
  float advance_lut[128];
} TextAreaIter;

static void iter_begin(TextAreaIter *it, StygianContext *ctx, StygianFont font,
                       const char *text, float max_w) {
  it->ctx = ctx;
  it->font = font;
  it->text = text;
  it->max_w = max_w;
  it->p = text;
  it->line_start = text;
  it->current_w = 0;
  it->y = 0;
  it->line_index = 0;
  it->done = (*text == 0);

  // Pre-compute ASCII advance widths once
  for (int c = 0; c < 128; c++) {
    char temp[2] = {(char)c, 0};
    it->advance_lut[c] =
        (c >= 32) ? stygian_text_width(ctx, font, temp, 16.0f) : 0.0f;
  }
}

// Advances to next line (hard or soft break)
// Returns true if there is a line to process
static bool iter_next_line(TextAreaIter *it, const char **out_start,
                           const char **out_end) {
  if (it->done)
    return false;

  it->line_start = it->p;
  it->current_w = 0;

  const char *scan = it->p;
  const char *last_space = NULL;

  while (*scan) {
    if (*scan == '\n') {
      *out_start = it->line_start;
      *out_end = scan;
      it->p = scan + 1; // Skip newline
      it->y += 18.0f;
      return true;
    }

    // Measure char (Brutal: using fixed step or real measure)
    // Fast ASCII advance from LUT, fallback for non-ASCII
    unsigned char uc = (unsigned char)*scan;
    float cw = (uc < 128) ? it->advance_lut[uc]
                          : stygian_text_width(it->ctx, it->font,
                                               (char[]){*scan, 0}, 16.0f);

    if (it->current_w + cw > it->max_w) {
      // Soft wrap needed
      if (last_space) {
        // Break at space
        *out_start = it->line_start;
        *out_end = last_space;
        it->p = last_space + 1; // Skip space
      } else {
        // Forced break at char
        *out_start = it->line_start;
        *out_end = scan;
        it->p = scan; // Start next line here
      }
      it->y += 18.0f;
      return true;
    }

    it->current_w += cw;
    if (*scan == ' ') {
      last_space = scan;
    }
    scan++;
  }

  // End of string
  *out_start = it->line_start;
  *out_end = scan;
  it->p = scan;    // Point to null
  it->done = true; // Mark as last pass
  it->y += 18.0f;
  return true;
}

// Helper to measure a single line (with consistent "fat space" logic)
// Uses advance LUT when available, falls back to stygian_text_width.
static float measure_line_lut(const float *lut, StygianContext *ctx,
                              StygianFont font, const char *start,
                              const char *end) {
  float w = 0;
  const char *p = start;
  while (p < end) {
    unsigned char uc = (unsigned char)*p;
    float cw = (lut && uc < 128)
                   ? lut[uc]
                   : stygian_text_width(ctx, font, (char[]){*p, 0}, 16.0f);
    if (*p == ' ' && cw < 1.0f)
      cw = 4.0f; // Minimal width for spaces
    w += cw;
    p++;
  }
  return w;
}

static int text_xy_to_index(StygianContext *ctx, StygianFont font,
                            const char *text, float param_x, float param_y,
                            float scroll_y, float max_w) {
  if (!text || !*text)
    return 0;

  TextAreaIter it;
  iter_begin(&it, ctx, font, text, max_w);

  const char *start, *end;
  float target_y = param_y + scroll_y;

  // Iterate lines to find Y match
  while (iter_next_line(&it, &start, &end)) {
    // Check Y bounds of this line
    float line_top = it.y - 18.0f; // iter_next_line advances Y
    float line_bottom = it.y;

    if (target_y >= line_top && target_y < line_bottom) {
      // Found the line. Scan X.
      // Linear scan within line
      const char *scan = start;
      float lx = 0;
      while (scan < end) {
        float cw = measure_line_lut(NULL, ctx, font, scan, scan + 1);
        float mid_x = lx + cw * 0.5f;
        if (param_x < mid_x)
          return (int)(scan - text);
        lx += cw;
        scan++;
      }
      return (int)(end - text); // Clicked past end of line
    }
  }
  return (int)strlen(text); // Below all text
}

// Helper to insert character at index
static void buffer_insert(char *buf, int size, int idx, char c) {
  int len = strlen(buf);
  if (len + 1 >= size)
    return;
  memmove(buf + idx + 1, buf + idx, len - idx + 1);
  buf[idx] = c;
}

// Helper to delete character before index
static void buffer_delete(char *buf, int idx) {
  if (idx <= 0)
    return;
  int len = strlen(buf);
  memmove(buf + idx - 1, buf + idx, len - idx + 1);
}

bool stygian_text_area(StygianContext *ctx, StygianFont font,
                       StygianTextArea *state) {
  uint32_t id = widget_id(state->x, state->y, "textarea");
  StygianWidgetRegionFlags region_flags =
      STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES;
  if (state->total_height > state->h) {
    region_flags =
        (StygianWidgetRegionFlags)(region_flags | STYGIAN_WIDGET_REGION_SCROLL);
  }
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  region_flags);
  bool hovered = point_in_rect(g_widget_state.mouse_x, g_widget_state.mouse_y,
                               state->x, state->y, state->w, state->h);
  widget_register_focusable(id);
  widget_nav_prepare();

  // Input Handling
  if (hovered && widget_mouse_pressed()) {
    g_widget_state.focus_id = id;
    g_widget_state.active_id = id; // Start dragging
    state->focused = true;
    float local_x = g_widget_state.mouse_x - state->x;
    float local_y = g_widget_state.mouse_y - state->y;
    // Pass state->w - 10 (padding) as max wrap width
    int idx = text_xy_to_index(ctx, font, state->buffer, local_x, local_y,
                               state->scroll_y, state->w - 10.0f);
    state->cursor_idx = idx;

    // Shift-Click extends selection
    bool shift = stygian_key_down(stygian_get_window(ctx), STYGIAN_KEY_SHIFT);
    if (shift) {
      if (state->selection_start == -1) {
        state->selection_start = idx;
        state->selection_end = idx;
      } else {
        state->selection_end = idx;
      }
    } else {
      state->selection_start = idx;
      state->selection_end = idx;
    }
  }

  // Drag Selection
  if (g_widget_state.active_id == id && g_widget_state.mouse_down) {
    float local_x = g_widget_state.mouse_x - state->x;
    float local_y = g_widget_state.mouse_y - state->y;
    int idx = text_xy_to_index(ctx, font, state->buffer, local_x, local_y,
                               state->scroll_y, state->w - 10.0f);
    state->cursor_idx = idx;
    // Update end while keeping start as anchor
    state->selection_end = idx;
  }

  if (!g_widget_state.mouse_down && g_widget_state.active_id == id) {
    g_widget_state.active_id = 0;
  }

  // Force global sync
  if (g_widget_state.focus_id != id)
    state->focused = false;
  else
    state->focused = true;

  // Normalized range helper
  int sel_min = state->selection_start < state->selection_end
                    ? state->selection_start
                    : state->selection_end;
  int sel_max = state->selection_start > state->selection_end
                    ? state->selection_start
                    : state->selection_end;
  bool has_selection = (sel_min != sel_max);

  bool changed = false;
  if (state->focused) {
    // Process Buffer Keys
    for (int i = 0; i < g_widget_state.key_count; i++) {
      if (!g_widget_state.key_events[i].down)
        continue;
      StygianKey key = g_widget_state.key_events[i].key;
      bool shift = (g_widget_state.key_events[i].mods & STYGIAN_MOD_SHIFT);

      if (key == STYGIAN_KEY_BACKSPACE) {
        if (has_selection) {
          // Delete range
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
          state->selection_start = state->selection_end = sel_min;
          changed = true;
        } else if (state->cursor_idx > 0) {
          buffer_delete(state->buffer, state->cursor_idx);
          state->cursor_idx--;
          state->selection_start = state->selection_end = state->cursor_idx;
          changed = true;
        }
      } else if (key == STYGIAN_KEY_ENTER) {
        if (has_selection) {
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
        }
        buffer_insert(state->buffer, state->buffer_size, state->cursor_idx,
                      '\n');
        state->cursor_idx++;
        state->selection_start = state->selection_end = state->cursor_idx;
        changed = true;
      } else if (key == STYGIAN_KEY_LEFT) {
        if (state->cursor_idx > 0)
          state->cursor_idx--;
        if (shift)
          state->selection_end = state->cursor_idx;
        else
          state->selection_start = state->selection_end = state->cursor_idx;
      } else if (key == STYGIAN_KEY_RIGHT) {
        if (state->buffer[state->cursor_idx] != 0)
          state->cursor_idx++;
        if (shift)
          state->selection_end = state->cursor_idx;
        else
          state->selection_start = state->selection_end = state->cursor_idx;
      } else if (key == STYGIAN_KEY_UP) {
        // TODO: Line jump logic
      } else if (key == STYGIAN_KEY_DOWN) {
        // TODO: Line jump logic
      } else if (key == STYGIAN_KEY_C &&
                 (g_widget_state.key_events[i].mods & STYGIAN_MOD_CTRL)) {
        // Universal Copy
        if (has_selection) {
          int len = sel_max - sel_min;
          if (len > 0 && len < 8192) {
            char temp[8192];
            memcpy(temp, state->buffer + sel_min, len);
            temp[len] = 0;
            stygian_clipboard_push(ctx, temp, NULL);
          }
        } else {
          stygian_clipboard_push(ctx, state->buffer, NULL);
        }
      } else if (key == STYGIAN_KEY_V &&
                 (g_widget_state.key_events[i].mods & STYGIAN_MOD_CTRL)) {
        // Universal Paste
        if (has_selection) {
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
        }
        char *clip = stygian_clipboard_pop(ctx);
        if (clip) {
          const char *p = clip;
          while (*p) {
            buffer_insert(state->buffer, state->buffer_size, state->cursor_idx,
                          *p);
            state->cursor_idx++;
            p++;
          }
          free(clip); // Use standard free (matches _strdup)
          state->selection_start = state->selection_end = state->cursor_idx;
          changed = true;
        }
      }
    }

    // Process Char Input
    for (int i = 0; i < g_widget_state.char_count; i++) {
      char c = (char)g_widget_state.char_events[i];
      if (c >= 32 && c <= 126) { // Printable ASCII for now
        if (has_selection) {
          int buf_len = strlen(state->buffer);
          memmove(state->buffer + sel_min, state->buffer + sel_max,
                  buf_len - sel_max + 1);
          state->cursor_idx = sel_min;
          // state->selection_start = state->selection_end = sel_min; //
          // handled at end
          has_selection = false;
          sel_min = sel_max = state->cursor_idx;
        }
        buffer_insert(state->buffer, state->buffer_size, state->cursor_idx, c);
        state->cursor_idx++;
        state->selection_start = state->selection_end = state->cursor_idx;
        changed = true;
      }
    }
  }
  // Scroll Handling
  if (hovered &&
      (g_widget_state.scroll_dy != 0 || g_widget_state.scroll_dx != 0)) {
    state->scroll_y -= g_widget_state.scroll_dy * 20.0f;
  }
  // Clamp scroll
  if (state->scroll_y < 0.0f)
    state->scroll_y = 0;
  if (state->total_height > state->h &&
      state->scroll_y > state->total_height - state->h)
    state->scroll_y = state->total_height - state->h;

  // Render Background
  float bg_col[4] = {0.1f, 0.1f, 0.12f, 1.0f};
  if (state->focused) {
    bg_col[0] = 0.12f;
    bg_col[1] = 0.12f;
    bg_col[2] = 0.15f;
  }
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, bg_col[0],
                       bg_col[1], bg_col[2], 1.0f, 4.0f);

  // Clip Content
  stygian_clip_push(ctx, state->x, state->y, state->w, state->h);

  // Render Text Line by Line (Wrapped)
  float x_off = state->x + 5.0f;
  bool show_scrollbar = state->total_height > state->h;
  float max_w = state->w - (show_scrollbar ? 14.0f : 10.0f);
  if (max_w < 20.0f)
    max_w = 20.0f;

  TextAreaIter it;
  iter_begin(&it, ctx, font, state->buffer, max_w);
  const char *start, *end;

  while (iter_next_line(&it, &start, &end)) {
    // Draw visible lines
    float line_top = it.y - 18.0f;
    float abs_top = state->y + line_top - state->scroll_y;

    if (abs_top + 18.0f > state->y && abs_top < state->y + state->h) {
      float lx = x_off;

      // Draw Selection Rect (Block)
      if (has_selection) {
        int idx_start = (int)(start - state->buffer);
        int idx_end = (int)(end - state->buffer);

        int intersect_min = (sel_min > idx_start) ? sel_min : idx_start;
        int intersect_max = (sel_max < idx_end) ? sel_max : idx_end;

        if (intersect_min < intersect_max) {
          // Valid intersection on this line
          float pre_width = measure_line_lut(NULL, ctx, font, start,
                                             state->buffer + intersect_min);
          float sel_width =
              measure_line_lut(NULL, ctx, font, state->buffer + intersect_min,
                               state->buffer + intersect_max);

          stygian_rect(ctx, lx + pre_width, abs_top, sel_width, 18.0f, 0.2f,
                       0.4f, 0.8f, 0.5f);
        }
      }

      const char *lp = start;
      while (lp < end) {
        char temp[2] = {*lp, 0};
        // Re-measure for consistent placement
        float cw = stygian_text_width(ctx, font, temp, 16.0f);
        if (*lp == ' ' && cw < 1.0f)
          cw = 4.0f;

        stygian_text(ctx, font, temp, lx, abs_top, 16.0f, 0.9f, 0.9f, 0.9f,
                     1.0f);
        lx += cw;
        lp++;
      }

      // Cursor
      if (state->focused) {
        int idx_start = (int)(start - state->buffer);
        int idx_end = (int)(end - state->buffer);
        if (state->cursor_idx >= idx_start && state->cursor_idx <= idx_end) {
          float cx =
              x_off + measure_line_lut(NULL, ctx, font, start,
                                       state->buffer + state->cursor_idx);
          stygian_rect(ctx, cx, abs_top, 2.0f, 16.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        }
      }
    }
  }

  stygian_clip_pop(ctx);
  // Auto-scroll logic could be added here
  state->total_height = it.y;

  stygian_scrollbar_v(ctx, state->x + state->w - 8.0f, state->y + 2.0f, 6.0f,
                      state->h - 4.0f, state->total_height, &state->scroll_y);

  return changed;
}

bool stygian_scrollbar_v(StygianContext *ctx, float x, float y, float w,
                         float h, float content_height, float *scroll_y) {
  uint32_t id;
  float max_scroll;
  float thumb_h;
  float travel;
  float ratio;
  float thumb_y;
  bool changed = false;
  bool hovered;
  bool thumb_hovered;
  bool mouse_pressed;
  bool mouse_released;

  if (!ctx || !scroll_y || h <= 1.0f || w <= 1.0f)
    return false;

  id = widget_id(x, y, "vscroll");
  max_scroll = content_height - h;
  if (max_scroll < 0.0f)
    max_scroll = 0.0f;
  widget_register_region_internal(x, y, w, h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
  if (max_scroll > 0.0f) {
    widget_register_region_internal(x, y, w, h, STYGIAN_WIDGET_REGION_SCROLL);
  }
  if (*scroll_y < 0.0f)
    *scroll_y = 0.0f;
  if (*scroll_y > max_scroll)
    *scroll_y = max_scroll;

  // Draw track always so layout is stable.
  stygian_rect_rounded(ctx, x, y, w, h, 0.16f, 0.16f, 0.18f, 0.55f, 3.0f);

  if (max_scroll <= 0.0f)
    return false;

  thumb_h = h * (h / content_height);
  if (thumb_h < 18.0f)
    thumb_h = 18.0f;
  if (thumb_h > h)
    thumb_h = h;

  travel = h - thumb_h;
  ratio = (max_scroll > 0.0f) ? (*scroll_y / max_scroll) : 0.0f;
  if (ratio < 0.0f)
    ratio = 0.0f;
  if (ratio > 1.0f)
    ratio = 1.0f;
  thumb_y = y + ratio * travel;

  hovered = point_in_rect((float)g_widget_state.mouse_x,
                          (float)g_widget_state.mouse_y, x, y, w, h);
  thumb_hovered =
      point_in_rect((float)g_widget_state.mouse_x,
                    (float)g_widget_state.mouse_y, x, thumb_y, w, thumb_h);
  mouse_pressed = widget_mouse_pressed();
  mouse_released = widget_mouse_released();

  if (hovered && g_widget_state.scroll_dy != 0.0f) {
    *scroll_y -= g_widget_state.scroll_dy * 24.0f;
    if (*scroll_y < 0.0f)
      *scroll_y = 0.0f;
    if (*scroll_y > max_scroll)
      *scroll_y = max_scroll;
    changed = true;
  }

  if (thumb_hovered)
    g_widget_state.hot_id = id;

  if (mouse_pressed && hovered) {
    g_widget_state.active_id = id;
    if (!thumb_hovered) {
      // Page jump to the clicked track position.
      float local = (float)g_widget_state.mouse_y - y - thumb_h * 0.5f;
      if (local < 0.0f)
        local = 0.0f;
      if (local > travel)
        local = travel;
      *scroll_y = (travel > 0.0f) ? (local / travel) * max_scroll : 0.0f;
      changed = true;
    }
  }

  if (g_widget_state.active_id == id) {
    if (g_widget_state.mouse_down) {
      float local = (float)g_widget_state.mouse_y - y - thumb_h * 0.5f;
      if (local < 0.0f)
        local = 0.0f;
      if (local > travel)
        local = travel;
      {
        float new_scroll =
            (travel > 0.0f) ? (local / travel) * max_scroll : 0.0f;
        if (fabsf(new_scroll - *scroll_y) > 0.01f) {
          *scroll_y = new_scroll;
          changed = true;
        }
      }
    } else if (mouse_released) {
      g_widget_state.active_id = 0;
    }
  }

  // Recompute thumb after any change.
  ratio = (max_scroll > 0.0f) ? (*scroll_y / max_scroll) : 0.0f;
  if (ratio < 0.0f)
    ratio = 0.0f;
  if (ratio > 1.0f)
    ratio = 1.0f;
  thumb_y = y + ratio * travel;

  {
    bool active = (g_widget_state.active_id == id);
    float r = active ? 0.64f : (thumb_hovered ? 0.56f : 0.48f);
    float g = active ? 0.67f : (thumb_hovered ? 0.58f : 0.5f);
    float b = active ? 0.76f : (thumb_hovered ? 0.66f : 0.56f);
    float a = active ? 0.95f : 0.88f;
    stygian_rect_rounded(ctx, x, thumb_y, w, thumb_h, r, g, b, a, 3.0f);
  }

  return changed;
}

// ============================================================================
// Panel Widget
// ============================================================================

static struct {
  float x, y, w, h;
  uint8_t clip_id;
  bool active;
} g_panel_state = {0};

void stygian_panel_begin(StygianContext *ctx, float x, float y, float w,
                         float h) {
  // Render panel background
  stygian_rect_rounded(ctx, x, y, w, h, 0.12f, 0.12f, 0.12f, 1.0f, 6.0f);

  // Set up clipping
  g_panel_state.x = x;
  g_panel_state.y = y;
  g_panel_state.w = w;
  g_panel_state.h = h;
  g_panel_state.clip_id = stygian_clip_push(ctx, x, y, w, h);
  g_panel_state.active = true;
}

void stygian_panel_end(StygianContext *ctx) {
  if (g_panel_state.active) {
    stygian_clip_pop(ctx);
    g_panel_state.active = false;
  }
}

// ============================================================================
// Node Graph Editor (Spatial JIT Implementation)
// ============================================================================

// Helper: Cubic Bezier wire (single element, optimized)
static void draw_cubic_bezier(StygianContext *ctx, float x1, float y1, float x2,
                              float y2, float thick, float color[4]) {
  // Control points for S-curve (standard node editor style)
  float cp1x = x1 + (x2 - x1) * 0.5f;
  float cp1y = y1;
  float cp2x = x1 + (x2 - x1) * 0.5f;
  float cp2y = y2;

  // Single SDF cubic bezier element - no gap artifacts, low overhead
  stygian_wire(ctx, x1, y1, cp1x, cp1y, cp2x, cp2y, x2, y2, thick, color[0],
               color[1], color[2], color[3]);
}

static void draw_straight_wire(StygianContext *ctx, float x1, float y1,
                               float x2, float y2, float thick,
                               float color[4]) {
  float mx = (x1 + x2) * 0.5f;
  float my = (y1 + y2) * 0.5f;
  stygian_wire(ctx, x1, y1, mx, my, mx, my, x2, y2, thick, color[0], color[1],
               color[2], color[3]);
}

static void draw_orthogonal_wire(StygianContext *ctx, float x1, float y1,
                                 float x2, float y2, float thick,
                                 float color[4]) {
  float mid_x = (x1 + x2) * 0.5f;
  draw_straight_wire(ctx, x1, y1, mid_x, y1, thick, color);
  draw_straight_wire(ctx, mid_x, y1, mid_x, y2, thick, color);
  draw_straight_wire(ctx, mid_x, y2, x2, y2, thick, color);
}

static void graph_view_bounds(const StygianGraphState *state, float padding,
                              float *l, float *t, float *r, float *b) {
  float pad = (padding > 0.0f) ? padding : 0.0f;
  *l = -state->pan_x - (state->w * 0.5f) / state->zoom - pad;
  *r = -state->pan_x + (state->w * 0.5f) / state->zoom + pad;
  *t = -state->pan_y - (state->h * 0.5f) / state->zoom - pad;
  *b = -state->pan_y + (state->h * 0.5f) / state->zoom + pad;
}

void stygian_node_graph_begin(StygianContext *ctx, StygianGraphState *state,
                              StygianNodeBuffers *data, int count) {
  // 1. Handle Input (Pan / Zoom)
  widget_register_region_internal(state->x, state->y, state->w, state->h,
                                  STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES |
                                      STYGIAN_WIDGET_REGION_SCROLL);
  bool hovered = point_in_rect((float)g_widget_state.mouse_x,
                               (float)g_widget_state.mouse_y, state->x,
                               state->y, state->w, state->h);

  // Pan: Middle Mouse
  if (hovered &&
      stygian_mouse_down(stygian_get_window(ctx), STYGIAN_MOUSE_MIDDLE)) {
    state->pan_x += g_widget_state.mouse_dx / state->zoom;
    state->pan_y += g_widget_state.mouse_dy / state->zoom;
  }

  // Zoom: Scroll events
  if (hovered && g_widget_state.scroll_dy != 0) {
    float prev_zoom = state->zoom;
    float zoom_factor = 1.0f + (g_widget_state.scroll_dy * 0.1f);

    float center_x = state->x + (state->w * 0.5f);
    float center_y = state->y + (state->h * 0.5f);

    float screen_mx = (float)g_widget_state.mouse_x;
    float screen_my = (float)g_widget_state.mouse_y;

    float world_x = (screen_mx - center_x) / prev_zoom - state->pan_x;
    float world_y = (screen_my - center_y) / prev_zoom - state->pan_y;

    state->zoom *= zoom_factor;
    if (state->zoom < 0.05f)
      state->zoom = 0.05f;
    if (state->zoom > 10.0f)
      state->zoom = 10.0f;

    state->pan_x = (screen_mx - center_x) / state->zoom - world_x;
    state->pan_y = (screen_my - center_y) / state->zoom - world_y;
  }

  // Node Dragging Logic
  if (hovered &&
      !stygian_mouse_down(stygian_get_window(ctx), STYGIAN_MOUSE_MIDDLE)) {
    if (widget_mouse_pressed()) {
      // Try to pick a node
      int pick =
          stygian_graph_pick_node(state, data, (float)g_widget_state.mouse_x,
                                  (float)g_widget_state.mouse_y);
      if (pick >= 0) {
        state->dragging_id = pick + 1; // 1-based
      }
    }
  }

  if (!g_widget_state.mouse_down) {
    state->dragging_id = 0;
  }

  if (state->dragging_id > 0) {
    int idx = state->dragging_id - 1;
    data->x[idx] += g_widget_state.mouse_dx / state->zoom;
    data->y[idx] += g_widget_state.mouse_dy / state->zoom;
    if (state->snap_enabled && state->snap_size > 0.0f) {
      stygian_graph_snap_pos(state, &data->x[idx], &data->y[idx]);
    }
  }

  // 2. Spatial JIT Culling
  state->iter_idx = 0;
  state->visible_count = 0;

  float view_l = 0.0f;
  float view_t = 0.0f;
  float view_r = 0.0f;
  float view_b = 0.0f;
  graph_view_bounds(state, 0.0f, &view_l, &view_t, &view_r, &view_b);

  // Linear Cull
  for (int i = 0; i < count; ++i) {
    float nx = data->x[i];
    float ny = data->y[i];
    float nw = data->w[i];
    float nh = data->h[i];

    if (nx + nw > view_l && nx < view_r && ny + nh > view_t && ny < view_b) {
      if (state->visible_count < 8192) {
        state->visible_ids[state->visible_count++] = i;
      }
    }
  }

  // 3. Draw Background Grid
  stygian_clip_push(ctx, state->x, state->y, state->w, state->h);
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.05f, 0.05f, 0.05f,
               1.0f);

  // Grid Lines (default)
  stygian_graph_draw_grid(ctx, state, 100.0f, 20.0f, 0.15f, 0.15f, 0.15f, 0.5f);
}

bool stygian_node_graph_next(StygianContext *ctx, StygianGraphState *state,
                             int *out_index) {
  if (state->iter_idx < state->visible_count) {
    *out_index = state->visible_ids[state->iter_idx++];
    return true;
  }
  return false;
}

bool stygian_node_def(StygianContext *ctx, const char *title, float x, float y,
                      float w, float h, bool selected) {
  stygian_rect_rounded(ctx, x, y, w, h, 0.15f, 0.15f, 0.18f, 1.0f, 8.0f);
  stygian_rect_rounded(ctx, x, y, w, 24.0f, 0.25f, 0.25f, 0.28f, 1.0f, 8.0f);
  stygian_text(ctx, 0, title, x + 10, y + 4, 16.0f, 0.9f, 0.9f, 0.9f, 1.0f);
  return false;
}

void stygian_node_graph_end(StygianContext *ctx, StygianGraphState *state) {
  stygian_clip_pop(ctx);
}

void stygian_node_link(StygianContext *ctx, float x1, float y1, float x2,
                       float y2, float thick, float color[4]) {
  draw_cubic_bezier(ctx, x1, y1, x2, y2, thick, color);
}

void stygian_graph_link(StygianContext *ctx, const StygianGraphState *state,
                        float x1, float y1, float x2, float y2, float thick,
                        float color[4]) {
  int style = state ? state->wire_style : STYGIAN_WIRE_SMOOTH;
  if (style == STYGIAN_WIRE_SHARP) {
    draw_orthogonal_wire(ctx, x1, y1, x2, y2, thick, color);
  } else {
    draw_cubic_bezier(ctx, x1, y1, x2, y2, thick, color);
  }
}

void stygian_graph_set_wire_style(StygianGraphState *state, int style) {
  if (!state)
    return;
  state->wire_style = style;
}

void stygian_graph_set_snap(StygianGraphState *state, bool enabled,
                            float size) {
  if (!state)
    return;
  state->snap_enabled = enabled;
  state->snap_size = (size > 0.0f) ? size : 0.0f;
}

void stygian_graph_snap_pos(const StygianGraphState *state, float *x,
                            float *y) {
  if (!state || !state->snap_enabled || state->snap_size <= 0.0f || !x || !y)
    return;
  float s = state->snap_size;
  *x = floorf((*x / s) + 0.5f) * s;
  *y = floorf((*y / s) + 0.5f) * s;
}

void stygian_graph_world_to_screen(const StygianGraphState *state, float wx,
                                   float wy, float *sx, float *sy) {
  if (!state || !sx || !sy)
    return;
  float center_x = state->x + (state->w * 0.5f);
  float center_y = state->y + (state->h * 0.5f);
  *sx = (wx + state->pan_x) * state->zoom + center_x;
  *sy = (wy + state->pan_y) * state->zoom + center_y;
}

void stygian_graph_screen_to_world(const StygianGraphState *state, float sx,
                                   float sy, float *wx, float *wy) {
  if (!state || !wx || !wy)
    return;
  float center_x = state->x + (state->w * 0.5f);
  float center_y = state->y + (state->h * 0.5f);
  *wx = (sx - center_x) / state->zoom - state->pan_x;
  *wy = (sy - center_y) / state->zoom - state->pan_y;
}

void stygian_graph_node_screen_rect(const StygianGraphState *state, float wx,
                                    float wy, float ww, float wh, float *sx,
                                    float *sy, float *sw, float *sh) {
  if (!state || !sx || !sy || !sw || !sh)
    return;
  stygian_graph_world_to_screen(state, wx, wy, sx, sy);
  *sw = ww * state->zoom;
  *sh = wh * state->zoom;
}

void stygian_graph_pin_center_world(const StygianGraphState *state, float wx,
                                    float wy, float ww, bool output, float *px,
                                    float *py) {
  if (!state || !px || !py)
    return;
  float offset_y = (state->pin_y_offset > 0.0f) ? state->pin_y_offset : 48.0f;
  *px = output ? (wx + ww) : wx;
  *py = wy + offset_y;
}

void stygian_graph_pin_rect_screen(const StygianGraphState *state, float wx,
                                   float wy, float ww, bool output, float *x,
                                   float *y, float *w, float *h) {
  if (!state || !x || !y || !w || !h)
    return;
  float psize =
      (state->pin_size > 0.0f) ? state->pin_size : (16.0f * state->zoom);
  float px_world = 0.0f;
  float py_world = 0.0f;
  stygian_graph_pin_center_world(state, wx, wy, ww, output, &px_world,
                                 &py_world);
  float sx = 0.0f;
  float sy = 0.0f;
  stygian_graph_world_to_screen(state, px_world, py_world, &sx, &sy);
  *w = psize;
  *h = psize;
  *x = sx - (*w * 0.5f);
  *y = sy - (*h * 0.5f);
}

bool stygian_graph_pin_hit_test(const StygianGraphState *state, float wx,
                                float wy, float ww, bool output, float mx,
                                float my) {
  if (!state)
    return false;
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
  stygian_graph_pin_rect_screen(state, wx, wy, ww, output, &x, &y, &w, &h);
  return point_in_rect(mx, my, x, y, w, h);
}

bool stygian_graph_link_visible(const StygianGraphState *state, float ax,
                                float ay, float bx, float by, float padding) {
  if (!state)
    return true;
  float view_l = 0.0f;
  float view_t = 0.0f;
  float view_r = 0.0f;
  float view_b = 0.0f;
  graph_view_bounds(state, padding, &view_l, &view_t, &view_r, &view_b);
  return ((ax > view_l || bx > view_l) && (ax < view_r || bx < view_r) &&
          (ay > view_t || by > view_t) && (ay < view_b || by < view_b));
}

bool stygian_graph_link_visible_bezier(const StygianGraphState *state, float x1,
                                       float y1, float x2, float y2,
                                       float padding) {
  if (!state)
    return true;
  float mid_x = (x1 + x2) * 0.5f;
  float cp1x = mid_x;
  float cp1y = y1;
  float cp2x = mid_x;
  float cp2y = y2;
  float minx = x1;
  if (cp1x < minx)
    minx = cp1x;
  if (cp2x < minx)
    minx = cp2x;
  if (x2 < minx)
    minx = x2;
  float maxx = x1;
  if (cp1x > maxx)
    maxx = cp1x;
  if (cp2x > maxx)
    maxx = cp2x;
  if (x2 > maxx)
    maxx = x2;
  float miny = y1;
  if (cp1y < miny)
    miny = cp1y;
  if (cp2y < miny)
    miny = cp2y;
  if (y2 < miny)
    miny = y2;
  float maxy = y1;
  if (cp1y > maxy)
    maxy = cp1y;
  if (cp2y > maxy)
    maxy = cp2y;
  if (y2 > maxy)
    maxy = y2;
  float view_l = 0.0f;
  float view_t = 0.0f;
  float view_r = 0.0f;
  float view_b = 0.0f;
  graph_view_bounds(state, padding, &view_l, &view_t, &view_r, &view_b);
  return (maxx > view_l && minx < view_r && maxy > view_t && miny < view_b);
}

void stygian_graph_draw_grid(StygianContext *ctx,
                             const StygianGraphState *state, float major,
                             float minor, float r, float g, float b, float a) {
  (void)minor;
  if (!ctx || !state || major <= 0.0f)
    return;
  float world_l = 0.0f;
  float world_t = 0.0f;
  float world_r = 0.0f;
  float world_b = 0.0f;
  graph_view_bounds(state, 0.0f, &world_l, &world_t, &world_r, &world_b);
  float grid_start_x = floorf(world_l / major) * major;
  float grid_start_y = floorf(world_t / major) * major;
  int line_count = 0;
  for (float wx = grid_start_x; wx < world_r && line_count < 200;
       wx += major, line_count++) {
    float sx = (wx + state->pan_x) * state->zoom + state->x + state->w * 0.5f;
    if (sx >= state->x && sx <= state->x + state->w) {
      stygian_rect(ctx, sx, state->y, 1.0f, state->h, r, g, b, a);
    }
  }
  line_count = 0;
  for (float wy = grid_start_y; wy < world_b && line_count < 200;
       wy += major, line_count++) {
    float sy = (wy + state->pan_y) * state->zoom + state->y + state->h * 0.5f;
    if (sy >= state->y && sy <= state->y + state->h) {
      stygian_rect(ctx, state->x, sy, state->w, 1.0f, r, g, b, a);
    }
  }
}

bool stygian_graph_node_hit_test(const StygianGraphState *state, float wx,
                                 float wy, float ww, float wh, float mx,
                                 float my) {
  if (!state)
    return false;
  float sx = 0.0f;
  float sy = 0.0f;
  float sw = 0.0f;
  float sh = 0.0f;
  stygian_graph_node_screen_rect(state, wx, wy, ww, wh, &sx, &sy, &sw, &sh);
  return point_in_rect(mx, my, sx, sy, sw, sh);
}

int stygian_graph_pick_node(const StygianGraphState *state,
                            const StygianNodeBuffers *data, float mx,
                            float my) {
  if (!state || !data)
    return -1;
  for (int i = state->visible_count - 1; i >= 0; i--) {
    int idx = state->visible_ids[i];
    if (stygian_graph_node_hit_test(state, data->x[idx], data->y[idx],
                                    data->w[idx], data->h[idx], mx, my)) {
      return idx;
    }
  }
  return -1;
}


# stygian_widgets.h
// stygian_widgets.h - Widget API for Stygian UI Library
// High-level widgets built on top of Stygian rendering primitives
#ifndef STYGIAN_WIDGETS_H
#define STYGIAN_WIDGETS_H

#include "../include/stygian.h"
#include "../window/stygian_input.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Widget Configuration
// ============================================================================

typedef struct StygianWidgetStyle {
  float bg_color[4];
  float hover_color[4];
  float active_color[4];
  float text_color[4];
  float border_radius;
  float padding;
} StygianWidgetStyle;

// ============================================================================
// Button Widget
// ============================================================================

typedef struct StygianButton {
  float x, y, w, h;
  const char *label;
  bool hovered;
  bool pressed;
  bool clicked; // True for one frame when clicked
} StygianButton;

// Returns true if button was clicked this frame
bool stygian_button(StygianContext *ctx, StygianFont font, const char *label,
                    float x, float y, float w, float h);

// Button with custom style
bool stygian_button_ex(StygianContext *ctx, StygianFont font,
                       StygianButton *state, const StygianWidgetStyle *style);

// ============================================================================
// Slider Widget
// ============================================================================

typedef struct StygianSlider {
  float x, y, w, h;
  float value;    // Current value (0.0 - 1.0)
  float min, max; // Value range
  bool dragging;
} StygianSlider;

// Returns true if value changed
bool stygian_slider(StygianContext *ctx, float x, float y, float w, float h,
                    float *value, float min, float max);

// Slider with state management
bool stygian_slider_ex(StygianContext *ctx, StygianSlider *state,
                       const StygianWidgetStyle *style);

// ============================================================================
// Text Input Widget
// ============================================================================

typedef struct StygianTextInput {
  float x, y, w, h;
  char *buffer;
  int buffer_size;
  int cursor_pos;
  int selection_start;
  int selection_end;
  bool focused;
} StygianTextInput;

// Returns true if text changed
bool stygian_text_input(StygianContext *ctx, StygianFont font, float x, float y,
                        float w, float h, char *buffer, int buffer_size);

// Multiline Text Area with Scrolling & Selection
typedef struct StygianTextArea {
  float x, y, w, h;
  char *buffer;
  int buffer_size;
  int cursor_idx;      // Byte index
  int selection_start; // Byte index (-1 if no selection)
  int selection_end;   // Byte index (-1 if no selection)
  float scroll_y;
  float total_height; // Computed
  bool focused;
} StygianTextArea;

bool stygian_text_area(StygianContext *ctx, StygianFont font,
                       StygianTextArea *state);

// Vertical scrollbar for custom panels/areas.
// content_height: total scrollable content height in pixels.
// scroll_y: in/out scroll offset (0..content_height-viewport_height).
// Returns true when scroll_y changed.
bool stygian_scrollbar_v(StygianContext *ctx, float x, float y, float w,
                         float h, float content_height, float *scroll_y);

// ============================================================================
// Checkbox Widget
// ============================================================================

bool stygian_checkbox(StygianContext *ctx, StygianFont font, const char *label,
                      float x, float y, bool *checked);

// Radio button (returns true when clicked)
// selected: pointer to int holding current selection
// value: value this radio button represents
bool stygian_radio_button(StygianContext *ctx, StygianFont font,
                          const char *label, float x, float y, int *selected,
                          int value);

// ============================================================================
// Frame Management
// ============================================================================

typedef uint32_t StygianWidgetEventImpact;
#define STYGIAN_IMPACT_NONE 0u
#define STYGIAN_IMPACT_POINTER_ONLY (1u << 0)
#define STYGIAN_IMPACT_MUTATED_STATE (1u << 1)
#define STYGIAN_IMPACT_REQUEST_REPAINT (1u << 2)
#define STYGIAN_IMPACT_REQUEST_EVAL (1u << 3)
#define STYGIAN_IMPACT_LAYOUT_CHANGED STYGIAN_IMPACT_MUTATED_STATE

typedef uint32_t StygianWidgetRegionFlags;
#define STYGIAN_WIDGET_REGION_POINTER_LEFT (1u << 0)
#define STYGIAN_WIDGET_REGION_POINTER_RIGHT (1u << 1)
#define STYGIAN_WIDGET_REGION_SCROLL (1u << 2)
#define STYGIAN_WIDGET_REGION_MUTATES (1u << 3)
#define STYGIAN_WIDGET_REGION_POINTER                                           \
  (STYGIAN_WIDGET_REGION_POINTER_LEFT | STYGIAN_WIDGET_REGION_POINTER_RIGHT)
#define STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES                              \
  (STYGIAN_WIDGET_REGION_POINTER_LEFT | STYGIAN_WIDGET_REGION_MUTATES)
#define STYGIAN_WIDGET_REGION_POINTER_RIGHT_MUTATES                             \
  (STYGIAN_WIDGET_REGION_POINTER_RIGHT | STYGIAN_WIDGET_REGION_MUTATES)

// Call at start of frame to update widget input state
void stygian_widgets_begin_frame(StygianContext *ctx);

// Process input events (call for every event in window loop)
void stygian_widgets_process_event(StygianContext *ctx, StygianEvent *e);
StygianWidgetEventImpact
stygian_widgets_process_event_ex(StygianContext *ctx, const StygianEvent *e);

// Register custom interactive regions (captured this frame, used for next-frame
// input routing). Use this for app-defined scroll/click areas that are not
// built from stock widgets.
// Note: pointer events only report mutation impact when the hit region includes
// STYGIAN_WIDGET_REGION_MUTATES.
void stygian_widgets_register_region(float x, float y, float w, float h,
                                     StygianWidgetRegionFlags flags);

// Commit captured regions after a rendered frame so routing can use the last
// valid frame snapshot even while render is skipped during idle.
void stygian_widgets_commit_regions(void);

// Per-frame scroll delta from processed input events.
float stygian_widgets_scroll_dx(void);
float stygian_widgets_scroll_dy(void);

// Request repaint cadence for this frame (max request wins).
// hz=0 is ignored.
void stygian_widgets_request_repaint_hz(uint32_t hz);

// Compute wait timeout for event-driven loops.
// Returns idle_wait_ms when no repaint is requested, otherwise clamp(1000/hz,
// 1..idle_wait_ms) with the highest requested hz.
uint32_t stygian_widgets_repaint_wait_ms(uint32_t idle_wait_ms);

// True when any widget requests a repaint (e.g., animated diagnostics graph).
bool stygian_widgets_wants_repaint(void);

// ============================================================================
// Diagnostics Widgets
// ============================================================================

#define STYGIAN_PERF_HISTORY_MAX 180

typedef struct StygianPerfWidget {
  float x, y, w, h;
  const char *renderer_name;
  bool enabled;
  bool show_graph;
  bool show_input;
  bool auto_scale_graph;
  uint32_t history_count;
  uint32_t history_head;
  float history_ms[STYGIAN_PERF_HISTORY_MAX];
  double last_sample_seconds;
  double last_render_seconds;
  float fps_smoothed;
  float fps_wall_smoothed;
  uint32_t history_window; // 0 = auto(120), else 60/120/180...
  uint32_t idle_hz;        // 0 = default 30
  uint32_t active_hz;      // 0 = default 60
  uint32_t text_hz;        // 0 = default 5
  uint32_t graph_max_segments; // 0 = default 64
  float graph_scale_ms;    // Smoothed Y-scale for graph autoscale
  float graph_filtered_ms; // Smoothed graph sample value
  uint32_t max_stress_hz;  // 0 = disabled, else upper bound (e.g. 120/144)
  bool stress_mode;        // when true, use max_stress_hz for diagnostics tick
  uint32_t budget_miss_count;
  bool compact_mode;
  bool show_memory;
  bool show_glyphs;
  bool show_triad;
} StygianPerfWidget;

void stygian_perf_widget(StygianContext *ctx, StygianFont font,
                         StygianPerfWidget *state);
void stygian_perf_widget_set_rates(StygianPerfWidget *state, uint32_t graph_hz,
                                   uint32_t text_hz);
void stygian_perf_widget_set_enabled(StygianPerfWidget *state, bool enabled);

// ============================================================================
// Overlay Widgets
// ============================================================================

typedef struct StygianTooltip {
  const char *text;
  float x, y;
  float max_w;
  bool show;
} StygianTooltip;

void stygian_tooltip(StygianContext *ctx, StygianFont font,
                     const StygianTooltip *tooltip);

typedef struct StygianContextMenu {
  bool open;
  float x, y;
  float w;
  float item_h;
} StygianContextMenu;

bool stygian_context_menu_trigger_region(StygianContext *ctx,
                                         StygianContextMenu *state, float x,
                                         float y, float w, float h);
bool stygian_context_menu_begin(StygianContext *ctx, StygianFont font,
                                StygianContextMenu *state, int item_count);
bool stygian_context_menu_item(StygianContext *ctx, StygianFont font,
                               StygianContextMenu *state, const char *label,
                               int item_index);
void stygian_context_menu_end(StygianContext *ctx, StygianContextMenu *state);

typedef struct StygianModal {
  bool open;
  bool close_on_backdrop;
  float w, h;
  const char *title;
} StygianModal;

bool stygian_modal_begin(StygianContext *ctx, StygianFont font,
                         StygianModal *state, float viewport_w,
                         float viewport_h);
void stygian_modal_end(StygianContext *ctx, StygianModal *state);

// ============================================================================
// Panel Widget
// ============================================================================

void stygian_panel_begin(StygianContext *ctx, float x, float y, float w,
                         float h);
void stygian_panel_end(StygianContext *ctx);

// ============================================================================
// IDE Widgets - File Navigation
// ============================================================================

typedef struct StygianFileEntry {
  char name[64];
  bool is_directory;
  bool is_expanded;
  bool is_selected;
  int depth;
  // Internal use
  struct StygianFileEntry *next;
  struct StygianFileEntry *children;
} StygianFileEntry;

typedef struct StygianFileExplorer {
  float x, y, w, h;
  const char *root_path;
  char selected_path[256];
  float scroll_y;
  // Callback for file selection/opening
  void (*on_file_select)(const char *path);
  void (*on_file_open)(const char *path);
} StygianFileExplorer;

// Returns true if a file was selected/opened this frame
bool stygian_file_explorer(StygianContext *ctx, StygianFont font,
                           StygianFileExplorer *state);

typedef struct StygianBreadcrumb {
  float x, y, w, h;
  const char *path; // e.g., "src/widgets/ide/file_explorer.c"
  char separator;   // e.g., '/' or '>'
} StygianBreadcrumb;

// Returns true if a path segment was clicked.
// out_path will be filled with the path up to the clicked segment.
bool stygian_breadcrumb(StygianContext *ctx, StygianFont font,
                        StygianBreadcrumb *state, char *out_path, int max_len);

// ============================================================================
// IDE Widgets - Output & Diagnostics
// ============================================================================

typedef struct StygianOutputPanel {
  float x, y, w, h;
  const char *title;
  // Circular buffer or pointer to log data
  // For demo, standard C string
  const char *text_buffer;
  float scroll_y;
  bool auto_scroll;
} StygianOutputPanel;

void stygian_output_panel(StygianContext *ctx, StygianFont font,
                          StygianOutputPanel *state);

typedef struct StygianProblem {
  int line;
  int column;
  int severity; // 0=Info, 1=Warning, 2=Error
  char message[128];
  char file[64];
} StygianProblem;

typedef struct StygianProblemsPanel {
  float x, y, w, h;
  StygianProblem *problems;
  int problem_count;
  float scroll_y;
  int selected_index;
} StygianProblemsPanel;

bool stygian_problems_panel(StygianContext *ctx, StygianFont font,
                            StygianProblemsPanel *state);

// ============================================================================
// IDE Widgets - Debugging
// ============================================================================

typedef struct StygianDebugToolbar {
  float x, y, w, h;
  bool is_paused;
  // Callback
  void (*on_action)(
      int action_id); // 0=cont, 1=step_over, 2=step_into, 3=step_out, 4=stop
} StygianDebugToolbar;

void stygian_debug_toolbar(StygianContext *ctx, StygianFont font,
                           StygianDebugToolbar *state);

typedef struct StygianStackFrame {
  char function[64];
  char file[64];
  int line;
  uintptr_t address;
} StygianStackFrame;

typedef struct StygianCallStack {
  float x, y, w, h;
  StygianStackFrame *frames;
  int frame_count;
  int selected_frame;
} StygianCallStack;

bool stygian_call_stack(StygianContext *ctx, StygianFont font,
                        StygianCallStack *state);

// ============================================================================
// CAD Widgets - Precision Input
// ============================================================================

typedef struct StygianCoordinateInput {
  float x_val, y_val, z_val;
  bool locked_x, locked_y, locked_z;
  float x, y, w, h;
  const char *label; // Optional group label
} StygianCoordinateInput;

// Returns true if values changed
bool stygian_coordinate_input(StygianContext *ctx, StygianFont font,
                              StygianCoordinateInput *state);

typedef struct StygianSnapSettings {
  bool grid_snap;
  float grid_size;
  bool angel_snap;
  float angle_step;
  bool object_snap; // Endpoints, midpoints, etc.
  float x, y, w, h;
} StygianSnapSettings;

bool stygian_snap_settings(StygianContext *ctx, StygianFont font,
                           StygianSnapSettings *state);

// ============================================================================
// CAD Widgets - 3D Manipulation
// ============================================================================

typedef enum StygianGizmoMode {
  STYGIAN_GIZMO_TRANSLATE,
  STYGIAN_GIZMO_ROTATE,
  STYGIAN_GIZMO_SCALE
} StygianGizmoMode;

typedef struct StygianCADGizmo {
  float x, y, w, h; // 2D Screen rect usually, but here just a panel control
  StygianGizmoMode mode;
  bool local_space;
  // Interaction state
  int active_axis; // -1=none, 0=X, 1=Y, 2=Z
} StygianCADGizmo;

void stygian_cad_gizmo_controls(StygianContext *ctx, StygianFont font,
                                StygianCADGizmo *state);

typedef struct StygianLayer {
  char name[32];
  bool visible;
  bool locked;
  struct StygianLayer *next;
} StygianLayer;

typedef struct StygianLayerManager {
  float x, y, w, h;
  StygianLayer *layers;
  int layer_count;
  int active_layer_index;
  float scroll_y;
} StygianLayerManager;

bool stygian_layer_manager(StygianContext *ctx, StygianFont font,
                           StygianLayerManager *state);

// ============================================================================
// Game Engine Widgets - Viewport & Scene
// ============================================================================

typedef struct StygianSceneViewport {
  float x, y, w, h;
  uint32_t framebuffer_texture; // External texture ID from game engine
  bool show_grid;
  bool show_gizmo;
} StygianSceneViewport;

void stygian_scene_viewport(StygianContext *ctx, StygianSceneViewport *state);

typedef struct StygianSceneNode {
  char name[64];
  bool visible;
  bool selected;
  int depth;
  struct StygianSceneNode *next;
  struct StygianSceneNode *children;
} StygianSceneNode;

typedef struct StygianSceneHierarchy {
  float x, y, w, h;
  StygianSceneNode *root;
  int selected_node_id;
  float scroll_y;
} StygianSceneHierarchy;

bool stygian_scene_hierarchy(StygianContext *ctx, StygianFont font,
                             StygianSceneHierarchy *state);

typedef struct StygianProperty {
  char name[32];
  char value[64];
  int type; // 0=string, 1=float, 2=int, 3=bool, 4=color
} StygianProperty;

typedef struct StygianInspector {
  float x, y, w, h;
  const char *object_name;
  StygianProperty *properties;
  int property_count;
  float scroll_y;
} StygianInspector;

bool stygian_inspector(StygianContext *ctx, StygianFont font,
                       StygianInspector *state);

// ============================================================================
// Game Engine Widgets - Assets & Console
// ============================================================================

typedef struct StygianAsset {
  char name[64];
  int type; // 0=texture, 1=model, 2=material, 3=script, etc.
  bool selected;
} StygianAsset;

typedef struct StygianAssetBrowser {
  float x, y, w, h;
  StygianAsset *assets;
  int asset_count;
  float scroll_y;
  int selected_index;
} StygianAssetBrowser;

bool stygian_asset_browser(StygianContext *ctx, StygianFont font,
                           StygianAssetBrowser *state);

typedef struct StygianConsoleLog {
  float x, y, w, h;
  const char *log_buffer; // Newline-separated log entries
  float scroll_y;
  bool auto_scroll;
} StygianConsoleLog;

void stygian_console_log(StygianContext *ctx, StygianFont font,
                         StygianConsoleLog *state);

// ============================================================================
// Advanced Features - Docking & Layout
// NOTE: TabBar moved to layout/stygian_tabs.h (production version)
// ============================================================================

typedef struct StygianSplitPanel {
  float x, y, w, h;
  bool vertical;     // true=vertical split, false=horizontal
  float split_ratio; // 0.0-1.0
  bool dragging;
} StygianSplitPanel;

// Returns true if split ratio changed
bool stygian_split_panel(StygianContext *ctx, StygianSplitPanel *state,
                         float *out_left_x, float *out_left_y,
                         float *out_left_w, float *out_left_h,
                         float *out_right_x, float *out_right_y,
                         float *out_right_w, float *out_right_h);

typedef struct StygianMenuBar {
  float x, y, w, h;
  const char **menu_labels;
  int menu_count;
  int hot_menu;
  int open_menu;
} StygianMenuBar;

void stygian_menu_bar(StygianContext *ctx, StygianFont font,
                      StygianMenuBar *state);

typedef struct StygianToolbar {
  float x, y, w, h;
  const char **tool_icons;
  const char **tool_tooltips;
  int tool_count;
  int active_tool;
} StygianToolbar;

int stygian_toolbar(StygianContext *ctx, StygianFont font,
                    StygianToolbar *state);

// ============================================================================
// Node Graph Editor (Spatial JIT Architecture)
// ============================================================================

typedef struct StygianNodeBuffers {
  float *x;
  float *y;
  float *w;
  float *h;
  // Optional
  int *type_id;
  bool *selected;
  // Pointers to your flat arrays
} StygianNodeBuffers;

typedef struct StygianGraphState {
  float x, y, w, h; // Widget rect
  float pan_x, pan_y;
  float zoom;
  bool snap_enabled;
  float snap_size;
  float pin_y_offset;
  float pin_size;
  int wire_style;
  int selection_count;
  // Internal spatial hash
  void *spatial_grid;
  // Internal JIT iterator
  int iter_idx;
  int visible_ids[8192]; // Max visible nodes per frame (cull buffer) -
                         // Increased for robustness using stack/struct size
  int visible_count;

  // Interaction
  int hovered_id;
  int dragging_id;
  float drag_offset_x, drag_offset_y;
} StygianGraphState;

// 1. Begin the graph frame. Uploads/Culls visible nodes.
void stygian_node_graph_begin(StygianContext *ctx, StygianGraphState *state,
                              StygianNodeBuffers *data, int count);

// 2. Iterate ONLY visible nodes. Returns true if next node is ready.
bool stygian_node_graph_next(StygianContext *ctx, StygianGraphState *state,
                             int *out_index);

// 3. Helper to draw a node (User calls this inside the loop, or writes their
// own) Returns true if node interaction occurred
bool stygian_node_def(StygianContext *ctx, const char *title, float x, float y,
                      float w, float h, bool selected);

// 4. End the graph frame. Handles background grid, selection logic.
void stygian_node_graph_end(StygianContext *ctx, StygianGraphState *state);

// 5. Connections (User iterates visible connections manually or uses this
// helper)
void stygian_node_link(StygianContext *ctx, float x1, float y1, float x2,
                       float y2, float thick, float color[4]);
void stygian_graph_link(StygianContext *ctx, const StygianGraphState *state,
                        float x1, float y1, float x2, float y2, float thick,
                        float color[4]);

// Grid snapping helpers (world space)
void stygian_graph_set_snap(StygianGraphState *state, bool enabled,
                            float size);
void stygian_graph_snap_pos(const StygianGraphState *state, float *x, float *y);

// Graph coordinate helpers
void stygian_graph_world_to_screen(const StygianGraphState *state, float wx,
                                   float wy, float *sx, float *sy);
void stygian_graph_screen_to_world(const StygianGraphState *state, float sx,
                                   float sy, float *wx, float *wy);
void stygian_graph_node_screen_rect(const StygianGraphState *state, float wx,
                                    float wy, float ww, float wh, float *sx,
                                    float *sy, float *sw, float *sh);
void stygian_graph_pin_center_world(const StygianGraphState *state, float wx,
                                    float wy, float ww, bool output, float *px,
                                    float *py);
void stygian_graph_pin_rect_screen(const StygianGraphState *state, float wx,
                                   float wy, float ww, bool output, float *x,
                                   float *y, float *w, float *h);
bool stygian_graph_pin_hit_test(const StygianGraphState *state, float wx,
                                float wy, float ww, bool output, float mx,
                                float my);
bool stygian_graph_link_visible(const StygianGraphState *state, float ax,
                                float ay, float bx, float by, float padding);
bool stygian_graph_link_visible_bezier(const StygianGraphState *state, float x1,
                                       float y1, float x2, float y2,
                                       float padding);
void stygian_graph_draw_grid(StygianContext *ctx,
                             const StygianGraphState *state, float major,
                             float minor, float r, float g, float b,
                             float a);
bool stygian_graph_node_hit_test(const StygianGraphState *state, float wx,
                                 float wy, float ww, float wh, float mx,
                                 float my);
int stygian_graph_pick_node(const StygianGraphState *state,
                            const StygianNodeBuffers *data, float mx,
                            float my);

// Wire style
enum {
  STYGIAN_WIRE_SMOOTH = 0,
  STYGIAN_WIRE_SHARP = 1,
};
void stygian_graph_set_wire_style(StygianGraphState *state, int style);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_WIDGETS_H


# manipulators.c
// manipulators.c - CAD Manipulators & Layers for Stygian
// Part of CAD Widgets Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>


// Helper
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// CAD Gizmo Controls
// ============================================================================

void stygian_cad_gizmo_controls(StygianContext *ctx, StygianFont font,
                                StygianCADGizmo *state) {
  // Panel background
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.15f,
                       0.15f, 0.15f, 1.0f, 4.0f);

  const char *labels[] = {"T", "R", "S"}; // Translate, Rotate, Scale
  float btn_w = 24.0f;
  float btn_h = 24.0f;
  float padding = 4.0f;
  float cur_x = state->x + padding;
  float cur_y = state->y + (state->h - btn_h) / 2;

  // Mode Buttons
  for (int i = 0; i < 3; i++) {
    bool selected = (state->mode == (StygianGizmoMode)i);
    bool hovered = is_mouse_over(ctx, cur_x, cur_y, btn_w, btn_h);

    float r = 0.25f, g = 0.25f, b = 0.25f;
    if (selected) {
      r = 0.3f;
      g = 0.5f;
      b = 0.8f;
    } else if (hovered) {
      r = 0.35f;
      g = 0.35f;
      b = 0.35f;
    }

    stygian_rect_rounded(ctx, cur_x, cur_y, btn_w, btn_h, r, g, b, 1.0f, 4.0f);

    if (font) {
      stygian_text(ctx, font, labels[i], cur_x + 8, cur_y + 6, 14.0f, 0.9f,
                   0.9f, 0.9f, 1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      state->mode = (StygianGizmoMode)i;
    }

    cur_x += btn_w + padding;
  }

  // Space Toggle (Global/Local)
  cur_x += padding * 2; // Separator
  float space_btn_w = 48.0f;
  bool space_hovered = is_mouse_over(ctx, cur_x, cur_y, space_btn_w, btn_h);

  stygian_rect_rounded(ctx, cur_x, cur_y, space_btn_w, btn_h,
                       space_hovered ? 0.35f : 0.25f, 0.25f, 0.25f, 1.0f, 4.0f);

  const char *space_label = state->local_space ? "LOCAL" : "GLOBAL";
  if (font) {
    stygian_text(ctx, font, space_label, cur_x + 6, cur_y + 6, 12.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  StygianWindow *win = stygian_get_window(ctx);
  if (space_hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
    // Toggle (needs debounce in real app, simplified here)
    // state->local_space = !state->local_space;
  }
}

// ============================================================================
// Layer Manager
// ============================================================================

bool stygian_layer_manager(StygianContext *ctx, StygianFont font,
                           StygianLayerManager *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Layers", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  float row_h = 24.0f;
  float content_y = state->y + 24.0f;
  bool changed = false;

  StygianLayer *layer = state->layers;
  int index = 0;

  while (layer) {
    float cur_y = content_y + (index * row_h) - state->scroll_y;

    // Culling
    if (cur_y + row_h < content_y) {
      layer = layer->next;
      index++;
      continue;
    }
    if (cur_y > state->y + state->h)
      break;

    bool selected = (state->active_layer_index == index);
    bool hovered = is_mouse_over(ctx, state->x, cur_y, state->w, row_h);

    // Row Background
    if (selected) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.3f, 0.4f,
                   1.0f);
    } else if (hovered) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.15f, 0.15f, 0.15f,
                   1.0f);
    }

    // Interaction (Selection)
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      state->active_layer_index = index;
    }

    float x_offset = state->x + 4.0f;

    // Visibility Toggle (Eye icon placeholder)
    stygian_rect(ctx, x_offset, cur_y + 6, 12, 12, layer->visible ? 0.8f : 0.3f,
                 0.8f, 0.8f, 1.0f); // Cyan if visible
    x_offset += 20.0f;

    // Lock Toggle (Lock icon placeholder)
    stygian_rect(ctx, x_offset, cur_y + 6, 12, 12, layer->locked ? 0.8f : 0.3f,
                 0.3f, 0.3f, 1.0f); // Red if locked
    x_offset += 20.0f;

    // Name
    if (font) {
      stygian_text(ctx, font, layer->name, x_offset, cur_y + 4, 14.0f, 0.9f,
                   0.9f, 0.9f, 1.0f);
    }

    layer = layer->next;
    index++;
  }

  stygian_panel_end(ctx);
  return changed;
}


# precision_inputs.c
// precision_inputs.c - CAD Precision Inputs for Stygian
// Part of CAD Widgets Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>


// Helper to check hover
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Coordinate Input Widget (X, Y, Z)
// ============================================================================

bool stygian_coordinate_input(StygianContext *ctx, StygianFont font,
                              StygianCoordinateInput *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.15f,
                       0.15f, 0.15f, 1.0f, 4.0f);

  float padding = 4.0f;
  float label_w = 0.0f;

  if (state->label && font) {
    label_w = stygian_text_width(ctx, font, state->label, 14.0f) + 8.0f;
    stygian_text(ctx, font, state->label, state->x + padding,
                 state->y + (state->h - 14) / 2 + 2, 14.0f, 0.8f, 0.8f, 0.8f,
                 1.0f);
  }

  // Calculate width for each field (3 fields)
  float field_gap = 4.0f;
  float avail_w = state->w - label_w - (padding * 2);
  float field_w = (avail_w - (field_gap * 2)) / 3.0f;

  bool changed = false;
  const char *labels[] = {"X", "Y", "Z"};
  float *values[] = {&state->x_val, &state->y_val, &state->z_val};
  float colors[][3] = {
      {0.8f, 0.3f, 0.3f}, {0.3f, 0.8f, 0.3f}, {0.3f, 0.3f, 0.8f}}; // R, G, B

  float cur_x = state->x + label_w + padding;
  float cur_y = state->y + padding;
  float field_h = state->h - (padding * 2);

  for (int i = 0; i < 3; i++) {
    // Axis label background
    float axis_w = 16.0f;
    stygian_rect_rounded(ctx, cur_x, cur_y, axis_w, field_h, colors[i][0],
                         colors[i][1], colors[i][2], 1.0f, 2.0f);

    if (font) {
      stygian_text(ctx, font, labels[i], cur_x + 4,
                   cur_y + (field_h - 12) / 2 + 2, 12.0f, 0.1f, 0.1f, 0.1f,
                   1.0f);
    }

    // Value field background
    float input_x = cur_x + axis_w;
    float input_w = field_w - axis_w;

    // Simple drag logic (similar to slider but infinite)
    bool hovered = is_mouse_over(ctx, input_x, cur_y, input_w, field_h);

    // Visual state
    float r = 0.1f, g = 0.1f, b = 0.1f;
    if (hovered) {
      r = 0.2f;
      g = 0.2f;
      b = 0.2f;
    }

    stygian_rect_rounded(ctx, input_x, cur_y, input_w, field_h, r, g, b, 1.0f,
                         2.0f);

    // Render Value
    if (font) {
      char val_str[32];
      snprintf(val_str, sizeof(val_str), "%.2f", *values[i]);
      stygian_text(ctx, font, val_str, input_x + 4,
                   cur_y + (field_h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    // Interaction (Simple increment on click for demo)
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      // Very simple click-to-increment for demo purposes
      // Real CAD input would have text entry + drag
      *values[i] += 0.01f;
      changed = true;
    }

    cur_x += field_w + field_gap;
  }

  stygian_panel_end(ctx);
  return changed;
}

// ============================================================================
// Snap Settings Widget
// ============================================================================

bool stygian_snap_settings(StygianContext *ctx, StygianFont font,
                           StygianSnapSettings *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f,
                       0.1f, 1.0f, 4.0f);

  // Title
  if (font) {
    stygian_text(ctx, font, "Snapping", state->x + 8, state->y + 24, 14.0f,
                 0.8f, 0.8f, 0.8f, 1.0f);
  }

  float padding = 8.0f;
  float cur_y = state->y + 32.0f;
  bool changed = false;

  // Grid Snap Toggle
  if (stygian_checkbox(ctx, font, "Grid Snap", state->x + padding, cur_y,
                       &state->grid_snap))
    changed = true;
  cur_y += 24.0f;

  // Angle Snap Toggle
  if (stygian_checkbox(ctx, font, "Angle Snap", state->x + padding, cur_y,
                       &state->angel_snap))
    changed = true;
  cur_y += 24.0f;

  // Object Snap Toggle
  if (stygian_checkbox(ctx, font, "Object Snap", state->x + padding, cur_y,
                       &state->object_snap))
    changed = true;

  stygian_panel_end(ctx);
  return changed;
}


# assets_console.c
// assets_console.c - Game Engine Assets & Console Widgets
// Part of Game Engine Widgets Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>

// Helper
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Asset Browser Widget
// ============================================================================

bool stygian_asset_browser(StygianContext *ctx, StygianFont font,
                           StygianAssetBrowser *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.08f, 0.08f, 0.08f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.12f, 0.12f, 0.12f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Assets", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  // Grid layout for assets (simplified to list for now)
  float item_h = 60.0f;
  float content_y = state->y + 28.0f;
  bool clicked = false;

  for (int i = 0; i < state->asset_count; i++) {
    float cur_y = content_y + (i * item_h) - state->scroll_y;

    if (cur_y + item_h < content_y)
      continue;
    if (cur_y > state->y + state->h)
      break;

    StygianAsset *asset = &state->assets[i];
    bool selected = (i == state->selected_index);
    bool hovered =
        is_mouse_over(ctx, state->x + 4, cur_y, state->w - 8, item_h - 4);

    // Background
    float r = 0.1f, g = 0.1f, b = 0.1f;
    if (selected) {
      r = 0.2f;
      g = 0.3f;
      b = 0.5f;
    } else if (hovered) {
      r = 0.15f;
      g = 0.15f;
      b = 0.15f;
    }

    stygian_rect_rounded(ctx, state->x + 4, cur_y, state->w - 8, item_h - 4, r,
                         g, b, 1.0f, 4.0f);

    // Thumbnail placeholder (colored square based on type)
    float thumb_size = 48.0f;
    float thumb_colors[][3] = {
        {0.8f, 0.3f, 0.3f}, // Texture
        {0.3f, 0.8f, 0.3f}, // Model
        {0.3f, 0.3f, 0.8f}, // Material
        {0.8f, 0.8f, 0.3f}  // Script
    };
    int type_idx = asset->type < 4 ? asset->type : 0;

    stygian_rect_rounded(ctx, state->x + 8, cur_y + 4, thumb_size, thumb_size,
                         thumb_colors[type_idx][0], thumb_colors[type_idx][1],
                         thumb_colors[type_idx][2], 1.0f, 4.0f);

    // Name
    if (font) {
      stygian_text(ctx, font, asset->name, state->x + 12 + thumb_size,
                   cur_y + 20, 13.0f, 0.9f, 0.9f, 0.9f, 1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      state->selected_index = i;
      clicked = true;
    }
  }

  stygian_panel_end(ctx);
  return clicked;
}

// ============================================================================
// Console Log Widget
// ============================================================================

void stygian_console_log(StygianContext *ctx, StygianFont font,
                         StygianConsoleLog *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background (dark console)
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.05f, 0.05f, 0.05f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.08f, 0.08f, 0.08f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Console", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  // Log content
  float row_h = 16.0f;
  float content_y = state->y + 28.0f;

  if (state->log_buffer && font) {
    const char *ptr = state->log_buffer;
    const char *line_start = ptr;
    float cur_y = content_y - state->scroll_y;

    while (*ptr) {
      if (*ptr == '\n' || *ptr == 0) {
        int len = (int)(ptr - line_start);
        if (len > 0) {
          char line[256];
          if (len >= 256)
            len = 255;
          memcpy(line, line_start, len);
          line[len] = 0;

          // Visibility check
          if (cur_y + row_h > content_y && cur_y < state->y + state->h) {
            // Color based on prefix (simple log level detection)
            float r = 0.8f, g = 0.8f, b = 0.8f;
            if (strstr(line, "[ERROR]")) {
              r = 0.9f;
              g = 0.3f;
              b = 0.3f;
            } else if (strstr(line, "[WARN]")) {
              r = 0.9f;
              g = 0.8f;
              b = 0.2f;
            } else if (strstr(line, "[INFO]")) {
              r = 0.3f;
              g = 0.8f;
              b = 0.9f;
            }

            stygian_text(ctx, font, line, state->x + 8, cur_y, 12.0f, r, g, b,
                         1.0f);
          }
        }
        cur_y += row_h;
        line_start = ptr + 1;
      }
      if (*ptr == 0)
        break;
      ptr++;
    }
  }

  stygian_panel_end(ctx);
}


# viewport_scene.c
// viewport_scene.c - Game Engine Viewport & Scene Widgets
// Part of Game Engine Widgets Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>


// Helper
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Scene Viewport Widget
// ============================================================================

void stygian_scene_viewport(StygianContext *ctx, StygianSceneViewport *state) {
  // Border/Frame
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.05f, 0.05f, 0.05f,
               1.0f);

  // If external texture provided, render it
  if (state->framebuffer_texture != 0) {
    stygian_image(ctx, state->framebuffer_texture, state->x + 2, state->y + 2,
                  state->w - 4, state->h - 4);
  } else {
    // Placeholder checkerboard or gradient
    stygian_rect(ctx, state->x + 2, state->y + 2, state->w - 4, state->h - 4,
                 0.1f, 0.1f, 0.15f, 1.0f);
  }

  // Overlays (Grid, Gizmo indicators would go here in real impl)
  // For now, just show labels if enabled
}

// ============================================================================
// Scene Hierarchy Widget
// ============================================================================

static void render_scene_node(StygianContext *ctx, StygianFont font,
                              StygianSceneHierarchy *state,
                              StygianSceneNode *node, int depth,
                              float *y_offset) {
  if (!node)
    return;

  float row_h = 20.0f;
  float indent = 16.0f;
  float x = state->x + 4 + (depth * indent);
  float y = state->y + 24 + *y_offset - state->scroll_y;

  // Culling
  if (y + row_h < state->y + 24 || y > state->y + state->h) {
    *y_offset += row_h;
    return;
  }

  bool hovered = is_mouse_over(ctx, state->x, y, state->w, row_h);

  // Background
  if (node->selected) {
    stygian_rect(ctx, state->x, y, state->w, row_h, 0.2f, 0.3f, 0.5f, 1.0f);
  } else if (hovered) {
    stygian_rect(ctx, state->x, y, state->w, row_h, 0.15f, 0.15f, 0.15f, 1.0f);
  }

  // Visibility toggle (eye icon placeholder)
  float icon_x = x;
  stygian_rect(ctx, icon_x, y + 4, 12, 12, node->visible ? 0.8f : 0.3f, 0.8f,
               0.8f, 1.0f);

  // Name
  if (font) {
    stygian_text(ctx, font, node->name, icon_x + 16, y + 3, 13.0f, 0.9f, 0.9f,
                 0.9f, 1.0f);
  }

  *y_offset += row_h;

  // Render children recursively
  if (node->children) {
    render_scene_node(ctx, font, state, node->children, depth + 1, y_offset);
  }

  // Next sibling
  if (node->next) {
    render_scene_node(ctx, font, state, node->next, depth, y_offset);
  }
}

bool stygian_scene_hierarchy(StygianContext *ctx, StygianFont font,
                             StygianSceneHierarchy *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, "Scene", state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  float y_offset = 0;
  if (state->root) {
    render_scene_node(ctx, font, state, state->root, 0, &y_offset);
  }

  stygian_panel_end(ctx);
  return false; // Selection logic would go here
}

// ============================================================================
// Inspector Widget
// ============================================================================

bool stygian_inspector(StygianContext *ctx, StygianFont font,
                       StygianInspector *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    const char *title = state->object_name ? state->object_name : "Inspector";
    stygian_text(ctx, font, title, state->x + 8, state->y + 4, 14.0f, 0.8f,
                 0.8f, 0.8f, 1.0f);
  }

  float row_h = 24.0f;
  float content_y = state->y + 28.0f;
  bool changed = false;

  for (int i = 0; i < state->property_count; i++) {
    float cur_y = content_y + (i * row_h) - state->scroll_y;

    if (cur_y + row_h < content_y)
      continue;
    if (cur_y > state->y + state->h)
      break;

    StygianProperty *prop = &state->properties[i];

    // Property name (left)
    if (font) {
      stygian_text(ctx, font, prop->name, state->x + 8, cur_y + 4, 13.0f, 0.7f,
                   0.7f, 0.7f, 1.0f);
    }

    // Property value (right) - simplified, just display
    if (font) {
      float name_w = stygian_text_width(ctx, font, prop->name, 13.0f);
      stygian_text(ctx, font, prop->value, state->x + 16 + name_w, cur_y + 4,
                   13.0f, 0.9f, 0.9f, 0.9f, 1.0f);
    }
  }

  stygian_panel_end(ctx);
  return changed;
}


# debug_tools.c
// debug_tools.c - Debugging Widgets for Stygian
// Part of IDE Panels Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>


// Helper (duplicated)
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Debug Toolbar Widget
// ============================================================================

void stygian_debug_toolbar(StygianContext *ctx, StygianFont font,
                           StygianDebugToolbar *state) {
  // Toolbar background (floating pill shape usually, but here just a panel)
  stygian_rect_rounded(ctx, state->x, state->y, state->w, state->h, 0.2f, 0.2f,
                       0.2f, 1.0f, 4.0f);

  const char *icons[] = {
      state->is_paused ? ">" : "||", // Continue/Pause
      "->",                          // Step Over
      "v",                           // Step Into
      "^",                           // Step Out
      "X"                            // Stop
  };

  const char *tooltips[] = {"Continue", "Step Over", "Step Into", "Step Out",
                            "Stop"};

  float btn_w = 32.0f;
  float btn_h = state->h - 4.0f;
  float cur_x = state->x + 4.0f;
  float cur_y = state->y + 2.0f;

  for (int i = 0; i < 5; i++) {
    // Simple button logic inline for custom styling
    bool hovered = is_mouse_over(ctx, cur_x, cur_y, btn_w, btn_h);
    float r = 0.25f, g = 0.25f, b = 0.25f;

    if (hovered) {
      r = 0.35f;
      g = 0.35f;
      b = 0.35f;
      StygianWindow *win = stygian_get_window(ctx);
      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        r = 0.15f;
        // Callback
        if (state->on_action)
          state->on_action(i);
      }
    }

    // Color coding for Stop button
    if (i == 4) {
      r += 0.1f;
      b -= 0.1f;
      g -= 0.1f;
    } // Reddish
    // Color for Play
    if (i == 0 && state->is_paused) {
      g += 0.2f;
    } // Greenish

    stygian_rect_rounded(ctx, cur_x, cur_y, btn_w, btn_h, r, g, b, 1.0f, 4.0f);

    if (font) {
      float text_w = stygian_text_width(ctx, font, icons[i], 16.0f);
      stygian_text(ctx, font, icons[i], cur_x + (btn_w - text_w) / 2,
                   cur_y + (btn_h - 16) / 2 + 2, 16.0f, 0.9f, 0.9f, 0.9f, 1.0f);
    }

    cur_x += btn_w + 4.0f;
  }
}

// ============================================================================
// Call Stack Widget
// ============================================================================

bool stygian_call_stack(StygianContext *ctx, StygianFont font,
                        StygianCallStack *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Header
  stygian_rect(ctx, state->x, state->y, state->w, 24.0f, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font)
    stygian_text(ctx, font, "Call Stack", state->x + 8, state->y + 4, 14.0f,
                 0.8f, 0.8f, 0.8f, 1.0f);

  float row_h = 20.0f;
  float content_y = state->y + 24.0f;
  bool clicked = false;

  for (int i = 0; i < state->frame_count; i++) {
    float cur_y = content_y + i * row_h;
    if (cur_y > state->y + state->h)
      break;

    StygianStackFrame *f = &state->frames[i];
    bool selected = (i == state->selected_frame);
    bool hovered = is_mouse_over(ctx, state->x, cur_y, state->w, row_h);

    if (selected) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.3f, 0.2f,
                   1.0f); // Greenish highlight
    } else if (hovered) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.2f, 0.2f,
                   1.0f);
      StygianWindow *win = stygian_get_window(ctx);
      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        state->selected_frame = i;
        clicked = true;
      }
    }

    if (font) {
      char buf[128];
      snprintf(buf, sizeof(buf), "%s  %s:%d", f->function, f->file, f->line);
      stygian_text(ctx, font, buf, state->x + 8, cur_y + 3, 13.0f, 0.9f, 0.9f,
                   0.9f, 1.0f);
    }
  }

  stygian_panel_end(ctx);
  return clicked;
}


# file_explorer.c
// file_explorer.c - File Navigation Widgets for Stygian
// Part of IDE Panels Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h" // For clipboard/path utils if needed
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Helper Functions
// ============================================================================

static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

static bool is_clicked(StygianContext *ctx, float x, float y, float w,
                       float h) {
  StygianWindow *win = stygian_get_window(ctx);
  if (is_mouse_over(ctx, x, y, w, h)) {
    // Simple click detection - ideal would be tracking down/up sequence
    return stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
    // Note: Real implementation needs state tracking like stygian_button to
    // avoid repeat
  }
  return false;
}

// ============================================================================
// File Explorer Widget
// ============================================================================

// Mock file tree for demonstration/initial implementation
// In a real app, this would query the filesystem
static void render_file_node(StygianContext *ctx, StygianFont font,
                             StygianFileExplorer *state, const char *name,
                             bool is_dir, int depth, float *y_offset) {

  float item_h = 24.0f;
  float indent = 16.0f;
  float x = state->x;
  float y = state->y + *y_offset - state->scroll_y;
  float w = state->w;

  // Virtual culling (simple)
  if (y + item_h < state->y || y > state->y + state->h) {
    *y_offset += item_h;
    return;
  }

  // Interaction
  bool hovered = is_mouse_over(ctx, x, y, w, item_h);

  // Background
  if (hovered) {
    stygian_rect(ctx, x, y, w, item_h, 0.25f, 0.25f, 0.25f, 1.0f);
  }

  // Selection highlight
  // (Simple string match for demo)
  if (state->selected_path[0] && strstr(state->selected_path, name)) {
    stygian_rect(ctx, x, y, w, item_h, 0.2f, 0.3f, 0.5f, 0.8f);
  }

  // Icon (placeholder)
  float icon_x = x + 4 + (depth * indent);
  float icon_size = 16.0f;
  if (is_dir) {
    // Folder icon (yellow-ish box)
    stygian_rect(ctx, icon_x, y + 4, icon_size, icon_size, 0.8f, 0.7f, 0.2f,
                 1.0f);
  } else {
    // File icon (white-ish sheet)
    stygian_rect(ctx, icon_x, y + 4, icon_size, icon_size, 0.7f, 0.7f, 0.7f,
                 1.0f);
  }

  // Text
  if (font) {
    float text_x = icon_x + icon_size + 8;
    stygian_text(ctx, font, name, text_x, y + 4, 14.0f, 0.9f, 0.9f, 0.9f, 1.0f);
  }

  *y_offset += item_h;
}

bool stygian_file_explorer(StygianContext *ctx, StygianFont font,
                           StygianFileExplorer *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.12f, 0.12f, 0.12f,
               1.0f);

  float y_offset = 0;

  // Mock data rendering
  // Root
  render_file_node(ctx, font, state, state->root_path ? state->root_path : "/",
                   true, 0, &y_offset);

  // Mock Children (simulating an expanded tree)
  render_file_node(ctx, font, state, "src", true, 1, &y_offset);
  render_file_node(ctx, font, state, "include", true, 1, &y_offset);
  render_file_node(ctx, font, state, "main.c", false, 1, &y_offset);
  render_file_node(ctx, font, state, "utils.h", false, 1, &y_offset);

  // Nested
  render_file_node(ctx, font, state, "widgets", true, 2,
                   &y_offset); // Under src
  render_file_node(ctx, font, state, "file_explorer.c", false, 3, &y_offset);

  stygian_panel_end(ctx);
  return false; // Toggle/Select logic would go here
}

// ============================================================================
// Breadcrumb Widget
// ============================================================================

bool stygian_breadcrumb(StygianContext *ctx, StygianFont font,
                        StygianBreadcrumb *state, char *out_path, int max_len) {
  if (!state->path || !state->path[0])
    return false;

  // Make a copy to tokenize safely (or parse manually)
  // For immediate mode, we'll parse on the fly

  float cur_x = state->x;
  float cur_y = state->y;
  float h = state->h > 0 ? state->h : 24.0f;

  const char *ptr = state->path;
  const char *start = ptr;

  bool clicked_any = false;

  // Background container
  // stygian_rect(ctx, state->x, state->y, state->w, h, 0.15f, 0.15f,
  // 0.15f, 1.0f);

  while (*ptr) {
    if (*ptr == state->separator || *ptr == '/' || *ptr == '\\' ||
        *(ptr + 1) == 0) {
      int len = (int)(ptr - start);
      if (*(ptr + 1) == 0 && *ptr != state->separator && *ptr != '/' &&
          *ptr != '\\')
        len++;

      if (len > 0) {
        // Render segment
        char segment[64];
        if (len >= 64)
          len = 63;
        memcpy(segment, start, len);
        segment[len] = 0;

        float text_w = stygian_text_width(ctx, font, segment, 14.0f);
        float item_w = text_w + 16.0f; // Padding

        // Interaction
        bool hovered = is_mouse_over(ctx, cur_x, cur_y, item_w, h);
        if (hovered) {
          stygian_rect_rounded(ctx, cur_x, cur_y + 2, item_w, h - 4, 0.3f, 0.3f,
                               0.3f, 1.0f, 4.0f);

          // Check click (simple)
          StygianWindow *win = stygian_get_window(ctx);
          if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
            // Build path up to this segment
            int path_len = (int)(ptr - state->path);
            if (*(ptr + 1) == 0)
              path_len = (int)strlen(state->path);

            if (path_len < max_len) {
              memcpy(out_path, state->path, path_len);
              out_path[path_len] = 0;
              clicked_any = true;
            }
          }
        }

        stygian_text(ctx, font, segment, cur_x + 8, cur_y + (h - 14) / 2 + 2,
                     14.0f, 0.9f, 0.9f, 0.9f, 1.0f);

        cur_x += item_w;

        // Render separator
        if (*(ptr + 1) != 0 || *ptr == state->separator) {
          char sep_str[2] = {state->separator ? state->separator : '>', 0};
          stygian_text(ctx, font, sep_str, cur_x, cur_y + (h - 14) / 2 + 2,
                       14.0f, 0.5f, 0.5f, 0.5f, 1.0f);
          cur_x += 16.0f;
        }
      }

      start = ptr + 1;
    }
    ptr++;
  }

  return clicked_any;
}


# output_panel.c
// output_panel.c - Output & Diagnostic Widgets for Stygian
// Part of IDE Panels Phase

#include "../../window/stygian_input.h"
#include "../../window/stygian_window.h"
#include "../stygian_widgets.h"
#include <stdio.h>
#include <string.h>

// Helper (duplicated for now, should be in common utils)
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Output Panel Widget
// ============================================================================

void stygian_output_panel(StygianContext *ctx, StygianFont font,
                          StygianOutputPanel *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background (Terminal black)
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.08f, 0.08f, 0.08f,
               1.0f);

  // Header
  float header_h = 24.0f;
  stygian_rect(ctx, state->x, state->y, state->w, header_h, 0.15f, 0.15f, 0.15f,
               1.0f);
  if (font) {
    stygian_text(ctx, font, state->title ? state->title : "Output",
                 state->x + 8, state->y + 4, 14.0f, 0.8f, 0.8f, 0.8f, 1.0f);
  }

  // Content area
  float content_y = state->y + header_h + 4;
  float row_h = 18.0f;

  // Simple line rendering
  if (state->text_buffer && font) {
    const char *ptr = state->text_buffer;
    const char *line_start = ptr;
    float cur_y = content_y - state->scroll_y;

    while (*ptr) {
      if (*ptr == '\n' || *ptr == 0) {
        int len = (int)(ptr - line_start);
        if (len > 0) {
          char line[256];
          if (len >= 256)
            len = 255;
          memcpy(line, line_start, len);
          line[len] = 0;

          // Simple visibility check
          if (cur_y + row_h > content_y && cur_y < state->y + state->h) {
            // TODO: Parse ANSI color codes here
            // For now, render plain white/grey
            stygian_text(ctx, font, line, state->x + 8, cur_y, 14.0f, 0.8f,
                         0.8f, 0.8f, 1.0f);
          }
        }
        cur_y += row_h;
        line_start = ptr + 1;
      }
      if (*ptr == 0)
        break;
      ptr++;
    }
  }

  stygian_panel_end(ctx);
}

// ============================================================================
// Problems Panel Widget
// ============================================================================

bool stygian_problems_panel(StygianContext *ctx, StygianFont font,
                            StygianProblemsPanel *state) {
  stygian_panel_begin(ctx, state->x, state->y, state->w, state->h);

  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  // Header
  float header_h = 24.0f;
  stygian_rect(ctx, state->x, state->y, state->w, header_h, 0.18f, 0.18f, 0.18f,
               1.0f);
  if (font) {
    char title[64];
    snprintf(title, sizeof(title), "Problems (%d)", state->problem_count);
    stygian_text(ctx, font, title, state->x + 8, state->y + 4, 14.0f, 0.9f,
                 0.9f, 0.9f, 1.0f);
  }

  float content_y = state->y + header_h;
  float row_h = 24.0f;
  bool item_clicked = false;

  for (int i = 0; i < state->problem_count; i++) {
    float cur_y = content_y + (i * row_h) - state->scroll_y;

    // Culling
    if (cur_y + row_h < content_y)
      continue;
    if (cur_y > state->y + state->h)
      break;

    StygianProblem *p = &state->problems[i];

    // Hover/Selection
    bool hovered = is_mouse_over(ctx, state->x, cur_y, state->w, row_h);
    bool selected = (i == state->selected_index);

    if (selected) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.3f, 0.5f,
                   1.0f);
    } else if (hovered) {
      stygian_rect(ctx, state->x, cur_y, state->w, row_h, 0.2f, 0.2f, 0.2f,
                   1.0f);
      StygianWindow *win = stygian_get_window(ctx);
      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        state->selected_index = i;
        item_clicked = true;
      }
    }

    // Icon/Severity Color
    float r = 0.8f, g = 0.8f, b = 0.8f;
    const char *icon = "i";
    if (p->severity == 2) {
      r = 0.9f;
      g = 0.3f;
      b = 0.3f;
      icon = "x";
    } // Error
    else if (p->severity == 1) {
      r = 0.9f;
      g = 0.8f;
      b = 0.2f;
      icon = "!";
    } // Warning

    if (font) {
      // Icon
      stygian_text(ctx, font, icon, state->x + 8, cur_y + 4, 14.0f, r, g, b,
                   1.0f);

      // Message
      stygian_text(ctx, font, p->message, state->x + 30, cur_y + 4, 14.0f, 0.9f,
                   0.9f, 0.9f, 1.0f);

      // File location (right aligned or after message)
      char loc[64];
      snprintf(loc, sizeof(loc), "%s:%d", p->file, p->line);
      float loc_w = stygian_text_width(ctx, font, loc, 14.0f);
      stygian_text(ctx, font, loc, state->x + state->w - loc_w - 8, cur_y + 4,
                   14.0f, 0.5f, 0.5f, 0.5f, 1.0f);
    }
  }

  stygian_panel_end(ctx);
  return item_clicked;
}


# stygian_dock.h
// stygian_dock.h - Modern Docking System with Data Driven Immediate (DDI)
// Integration Part of Layout System - Multiviewport, Tabbed Containers,
// Floating Windows
//
// Architecture:
//   - Binary tree of DockNodes (splits or tab containers)
//   - Each panel renders via DDI callbacks (immediate mode)
//   - Floating windows use shared OpenGL contexts
//   - Metaball SDF melting effect on undock/redock

#ifndef STYGIAN_DOCK_H
#define STYGIAN_DOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct StygianContext StygianContext;
typedef uint32_t StygianFont;

// ============================================================================
// Constants
// ============================================================================

// Real-world panel counts (research):
//   VS Code: 8-15, Unreal: 12-25, Maya: 12-30, Blender: 8-18
//   Power users rarely exceed 25 panels. 32 is generous.
#define STYGIAN_DOCK_MAX_PANELS 32
#define STYGIAN_DOCK_MAX_NODES 64        // 2x panels for splits
#define STYGIAN_DOCK_MAX_TABS_PER_NODE 8 // Rarely need >8 tabs in one node
#define STYGIAN_DOCK_MAX_FLOATING 8      // Floating windows rare

// ============================================================================
// Enums
// ============================================================================

typedef enum StygianDockSplit {
  STYGIAN_DOCK_SPLIT_NONE = 0,   // Leaf node (tab container)
  STYGIAN_DOCK_SPLIT_HORIZONTAL, // Top/Bottom children
  STYGIAN_DOCK_SPLIT_VERTICAL    // Left/Right children
} StygianDockSplit;

typedef enum StygianDockDropZone {
  STYGIAN_DROP_NONE = 0,
  STYGIAN_DROP_CENTER, // Tab into existing container
  STYGIAN_DROP_LEFT,   // Split and insert left
  STYGIAN_DROP_RIGHT,  // Split and insert right
  STYGIAN_DROP_TOP,    // Split and insert top
  STYGIAN_DROP_BOTTOM, // Split and insert bottom
  STYGIAN_DROP_FLOAT   // Detach as floating window
} StygianDockDropZone;

// ============================================================================
// Dock Panel - Content with FBO render target
// ============================================================================

// ============================================================================
// Dock Panel - Content
// ============================================================================

typedef struct StygianDockPanel StygianDockPanel; // Opaque

// DDI render callback - your immediate-mode UI code goes here
typedef void (*StygianDockPanelRenderFn)(StygianDockPanel *panel,
                                         StygianContext *ctx, StygianFont font,
                                         float x, float y, float w, float h);

// ============================================================================
// Dock Node - Binary tree node
// ============================================================================

typedef struct StygianDockNode StygianDockNode; // Opaque

// ============================================================================
// Floating Window
// ============================================================================

typedef struct StygianFloatingWindow StygianFloatingWindow; // Opaque

// ============================================================================
// Dock Space - Root container
// ============================================================================

typedef struct StygianDockSpace StygianDockSpace; // Opaque

// ============================================================================
// API - Initialization & Lifecycle
// ============================================================================

// Create a new dock space on the heap
StygianDockSpace *stygian_dock_create(void *main_gl_context,
                                      void *main_device_context);

// Destroy dock space and free all resources
void stygian_dock_destroy(StygianDockSpace *dock);

// ============================================================================
// API - Panel Management
// ============================================================================

// Register a new panel with DDI render callback
// Returns panel ID, or 0 on failure
uint32_t stygian_dock_register_panel(StygianDockSpace *dock, const char *title,
                                     bool closable,
                                     StygianDockPanelRenderFn render,
                                     void *user_data);

// Unregister and remove panel
void stygian_dock_unregister_panel(StygianDockSpace *dock, uint32_t panel_id);

// Mark panel as needing re-render
void stygian_dock_mark_dirty(StygianDockSpace *dock, uint32_t panel_id);

// Get panel by ID (returns NULL if not found)
StygianDockPanel *stygian_dock_get_panel(StygianDockSpace *dock,
                                         uint32_t panel_id);

// ============================================================================
// API - Layout Building
// ============================================================================

// Add panel to a node (creates tab if node already has panels)
void stygian_dock_add_panel_to_node(StygianDockSpace *dock,
                                    StygianDockNode *node, uint32_t panel_id);

// Split a node, returns the two child nodes
// The original panels stay in child_a
void stygian_dock_split(StygianDockSpace *dock, StygianDockNode *node,
                        StygianDockSplit direction, float ratio,
                        StygianDockNode **out_a, StygianDockNode **out_b);

// Get or create root node
StygianDockNode *stygian_dock_get_root(StygianDockSpace *dock);

// ============================================================================
// API - Floating Windows
// ============================================================================

// Float a panel (creates new window with render surface)
void stygian_dock_float_panel(StygianContext *ctx, StygianDockSpace *dock,
                              uint32_t panel_id, float x, float y, float w,
                              float h);

// Dock a floating window back into main dock space
void stygian_dock_dock_floating(StygianContext *ctx, StygianDockSpace *dock,
                                int floating_idx, StygianDockNode *target,
                                StygianDockDropZone zone);

// ============================================================================
// API - Rendering & Interaction
// ============================================================================

// Update layout, handle input, render to FBOs
void stygian_dock_update(StygianContext *ctx, StygianFont font,
                         StygianDockSpace *dock, float x, float y, float w,
                         float h);

// Composite FBOs to main window (call after stygian_dock_update)
void stygian_dock_composite_main(StygianDockSpace *dock);

// Render floating windows (call for each floating window's message loop)
void stygian_dock_render_floating(StygianDockSpace *dock, int floating_idx);

// ============================================================================
// API - Serialization
// ============================================================================

// Save layout to JSON file
bool stygian_dock_save(StygianDockSpace *dock, const char *path);

// Load layout from JSON file
bool stygian_dock_load(StygianDockSpace *dock, const char *path);

// Apply preset layout
void stygian_dock_preset_ide(
    StygianDockSpace *dock); // Editor + Console + Files
void stygian_dock_preset_3d_editor(
    StygianDockSpace *dock); // Viewport + Props + Hierarchy

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_DOCK_H


# stygian_docking.c
// stygian_docking.c - Advanced Docking & Layout Widgets
// Part of Layout System (alongside stygian_layout.c)

#include "../layout/stygian_tabs.h"
#include "../layout/stygian_tabs_internal.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <stdio.h>
#include <string.h>

// Helper
static bool is_mouse_over(StygianContext *ctx, float x, float y, float w,
                          float h) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

// ============================================================================
// Tab Bar Widget
// ============================================================================

int stygian_tab_bar(StygianContext *ctx, StygianFont font,
                    StygianTabBar *state) {
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.12f, 0.12f, 0.12f,
               1.0f);

  float tab_w = 120.0f;
  float cur_x = state->x + 2.0f;
  int clicked_tab = -1;

  for (int i = 0; i < state->tab_count; i++) {
    StygianTabItem *tab = &state->tabs[i];
    bool active = (i == state->active_tab);
    bool hovered = is_mouse_over(ctx, cur_x, state->y, tab_w, state->h);

    // Tab background
    float r = 0.15f, g = 0.15f, b = 0.15f;
    if (active) {
      r = 0.2f;
      g = 0.25f;
      b = 0.35f;
    } else if (hovered) {
      r = 0.18f;
      g = 0.18f;
      b = 0.18f;
    }

    stygian_rect_rounded(ctx, cur_x, state->y + 2, tab_w - 4, state->h - 2, r,
                         g, b, 1.0f, 4.0f);

    // Title
    if (font) {
      stygian_text(ctx, font, tab->title, cur_x + 8,
                   state->y + (state->h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    // Close button (if closable)
    if (tab->closable) {
      float close_x = cur_x + tab_w - 20;
      float close_y = state->y + (state->h - 12) / 2;
      stygian_rect(ctx, close_x, close_y, 12, 12, 0.8f, 0.3f, 0.3f, 1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      clicked_tab = i;
    }

    cur_x += tab_w;
  }

  return clicked_tab;
}

// ============================================================================
// Split Panel Widget
// ============================================================================

bool stygian_split_panel(StygianContext *ctx, StygianSplitPanel *state,
                         float *out_left_x, float *out_left_y,
                         float *out_left_w, float *out_left_h,
                         float *out_right_x, float *out_right_y,
                         float *out_right_w, float *out_right_h) {

  float splitter_size = 4.0f;
  bool changed = false;

  if (state->vertical) {
    // Vertical split (left/right)
    float split_pos = state->x + (state->w * state->split_ratio);

    // Left panel
    *out_left_x = state->x;
    *out_left_y = state->y;
    *out_left_w = split_pos - state->x;
    *out_left_h = state->h;

    // Splitter
    float splitter_x = split_pos;
    bool hovered = is_mouse_over(ctx, splitter_x - splitter_size / 2, state->y,
                                 splitter_size, state->h);

    stygian_rect(ctx, splitter_x - splitter_size / 2, state->y, splitter_size,
                 state->h, hovered ? 0.3f : 0.2f, hovered ? 0.3f : 0.2f,
                 hovered ? 0.3f : 0.2f, 1.0f);

    // Drag logic (simplified)
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      int mx, my;
      stygian_mouse_pos(win, &mx, &my);
      state->split_ratio = (mx - state->x) / state->w;
      if (state->split_ratio < 0.1f)
        state->split_ratio = 0.1f;
      if (state->split_ratio > 0.9f)
        state->split_ratio = 0.9f;
      changed = true;
    }

    // Right panel
    *out_right_x = split_pos + splitter_size;
    *out_right_y = state->y;
    *out_right_w = state->x + state->w - *out_right_x;
    *out_right_h = state->h;

  } else {
    // Horizontal split (top/bottom)
    float split_pos = state->y + (state->h * state->split_ratio);

    // Top panel
    *out_left_x = state->x;
    *out_left_y = state->y;
    *out_left_w = state->w;
    *out_left_h = split_pos - state->y;

    // Splitter
    float splitter_y = split_pos;
    bool hovered = is_mouse_over(ctx, state->x, splitter_y - splitter_size / 2,
                                 state->w, splitter_size);

    stygian_rect(ctx, state->x, splitter_y - splitter_size / 2, state->w,
                 splitter_size, hovered ? 0.3f : 0.2f, hovered ? 0.3f : 0.2f,
                 hovered ? 0.3f : 0.2f, 1.0f);

    // Drag logic
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      int mx, my;
      stygian_mouse_pos(win, &mx, &my);
      state->split_ratio = (my - state->y) / state->h;
      if (state->split_ratio < 0.1f)
        state->split_ratio = 0.1f;
      if (state->split_ratio > 0.9f)
        state->split_ratio = 0.9f;
      changed = true;
    }

    // Bottom panel
    *out_right_x = state->x;
    *out_right_y = split_pos + splitter_size;
    *out_right_w = state->w;
    *out_right_h = state->y + state->h - *out_right_y;
  }

  return changed;
}

// ============================================================================
// Menu Bar Widget
// ============================================================================

void stygian_menu_bar(StygianContext *ctx, StygianFont font,
                      StygianMenuBar *state) {
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.1f, 0.1f, 0.1f,
               1.0f);

  float cur_x = state->x + 8.0f;

  for (int i = 0; i < state->menu_count; i++) {
    const char *label = state->menu_labels[i];
    float label_w = font ? stygian_text_width(ctx, font, label, 14.0f) : 60.0f;
    float item_w = label_w + 16.0f;

    bool hovered = is_mouse_over(ctx, cur_x, state->y, item_w, state->h);

    // Highlight
    if (hovered || i == state->open_menu) {
      stygian_rect(ctx, cur_x, state->y, item_w, state->h, 0.2f, 0.2f, 0.2f,
                   1.0f);
    }

    // Label
    if (font) {
      stygian_text(ctx, font, label, cur_x + 8,
                   state->y + (state->h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    cur_x += item_w;
  }
}

// ============================================================================
// Toolbar Widget
// ============================================================================

int stygian_toolbar(StygianContext *ctx, StygianFont font,
                    StygianToolbar *state) {
  // Background
  stygian_rect(ctx, state->x, state->y, state->w, state->h, 0.12f, 0.12f, 0.12f,
               1.0f);

  float btn_size = state->h - 4.0f;
  float cur_x = state->x + 4.0f;
  int clicked_tool = -1;

  for (int i = 0; i < state->tool_count; i++) {
    bool active = (i == state->active_tool);
    bool hovered = is_mouse_over(ctx, cur_x, state->y + 2, btn_size, btn_size);

    // Button background
    float r = 0.15f, g = 0.15f, b = 0.15f;
    if (active) {
      r = 0.3f;
      g = 0.5f;
      b = 0.8f;
    } else if (hovered) {
      r = 0.25f;
      g = 0.25f;
      b = 0.25f;
    }

    stygian_rect_rounded(ctx, cur_x, state->y + 2, btn_size, btn_size, r, g, b,
                         1.0f, 4.0f);

    // Icon (simplified - just text)
    if (font && state->tool_icons[i]) {
      stygian_text(ctx, font, state->tool_icons[i], cur_x + 8,
                   state->y + (state->h - 14) / 2 + 2, 14.0f, 0.9f, 0.9f, 0.9f,
                   1.0f);
    }

    // Interaction
    StygianWindow *win = stygian_get_window(ctx);
    if (hovered && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      clicked_tool = i;
    }

    cur_x += btn_size + 4.0f;
  }

  return clicked_tool;
}


# stygian_dock_impl.c
// stygian_dock.c - Modern Docking System Implementation
// Part of Layout System - Multiviewport, Tabbed Containers, Floating Windows

#include "../backends/stygian_ap.h"
#include "../include/stygian.h"
#include "../src/stygian_internal.h" // For ctx->elements access
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include "stygian_dock.h"
#include "stygian_dock_internal.h"
#include "stygian_tabs.h"

#include <stdio.h>
#include <string.h>

// Platform time helper for double-click detection
#ifdef _WIN32
#include <windows.h>
static uint64_t get_time_ms(void) { return GetTickCount64(); }
#else
#include <time.h>
static uint64_t get_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

// Double-click timing (ms)
#define DOUBLE_CLICK_TIME_MS 400
#define DOUBLE_CLICK_DISTANCE 5.0f

// Ghost window dimensions
#define GHOST_WIDTH 300
#define GHOST_HEIGHT 200

// ============================================================================
// Ghost Window (WS_POPUP for external drag preview)
// ============================================================================

#ifdef _WIN32
static void ghost_create(StygianDockSpace *dock) {
  if (dock->ghost_hwnd)
    return;

  // Register window class (once)
  static bool class_registered = false;
  if (!class_registered) {
    WNDCLASSEXA wc = {
        .cbSize = sizeof(WNDCLASSEXA),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = DefWindowProcA,
        .hInstance = GetModuleHandle(NULL),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszClassName = "StygianGhostClass",
    };
    RegisterClassExA(&wc);
    class_registered = true;
  }

  // Create layered popup window
  // WS_EX_TRANSPARENT: Click-through and DWM optimization
  dock->ghost_hwnd = CreateWindowExA(
      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
      "StygianGhostClass", NULL, WS_POPUP, 0, 0, GHOST_WIDTH, GHOST_HEIGHT,
      NULL, NULL, GetModuleHandle(NULL), NULL);

  dock->ghost_w = GHOST_WIDTH;
  dock->ghost_h = GHOST_HEIGHT;

  // Set translucent (50% opacity)
  SetLayeredWindowAttributes((HWND)dock->ghost_hwnd, 0, 128, LWA_ALPHA);
}

static void ghost_destroy(StygianDockSpace *dock) {
  if (dock->ghost_hwnd) {
    DestroyWindow((HWND)dock->ghost_hwnd);
    dock->ghost_hwnd = NULL;
  }
  dock->ghost_visible = false;
}

static void ghost_show(StygianDockSpace *dock, int screen_x, int screen_y) {
  if (!dock->ghost_hwnd)
    ghost_create(dock);

  // Center ghost on cursor
  int x = screen_x - dock->ghost_w / 2;
  int y = screen_y - dock->ghost_h / 2;

  SetWindowPos((HWND)dock->ghost_hwnd, HWND_TOPMOST, x, y, 0, 0,
               SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  dock->ghost_visible = true;
}

static void ghost_hide(StygianDockSpace *dock) {
  if (dock->ghost_hwnd && dock->ghost_visible) {
    ShowWindow((HWND)dock->ghost_hwnd, SW_HIDE);
    dock->ghost_visible = false;
  }
}

static void ghost_move(StygianDockSpace *dock, int screen_x, int screen_y) {
  if (dock->ghost_hwnd && dock->ghost_visible) {
    int x = screen_x - dock->ghost_w / 2;
    int y = screen_y - dock->ghost_h / 2;

    // Optimization: Only update if changed
    // (Assuming dock stores last ghost pos, or just checking vs window rect)
    RECT rc;
    GetWindowRect((HWND)dock->ghost_hwnd, &rc);
    if (rc.left != x || rc.top != y) {
      SetWindowPos((HWND)dock->ghost_hwnd, HWND_TOPMOST, x, y, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
  }
}
#else
// Stubs for non-Windows platforms
static void ghost_create(StygianDockSpace *dock) { (void)dock; }
static void ghost_destroy(StygianDockSpace *dock) { (void)dock; }
static void ghost_show(StygianDockSpace *dock, int x, int y) {
  (void)dock;
  (void)x;
  (void)y;
}
static void ghost_hide(StygianDockSpace *dock) { (void)dock; }
static void ghost_move(StygianDockSpace *dock, int x, int y) {
  (void)dock;
  (void)x;
  (void)y;
}
#endif

// ============================================================================
// Internal Helpers
// ============================================================================

static StygianDockNode *alloc_node(StygianDockSpace *dock) {
  if (dock->node_count >= STYGIAN_DOCK_MAX_NODES) {
    return NULL;
  }
  StygianDockNode *node = &dock->nodes[dock->node_count++];
  memset(node, 0, sizeof(StygianDockNode));
  node->id = ++dock->next_node_id;
  node->split_ratio = 0.5f;
  return node;
}

static StygianDockPanel *find_panel(StygianDockSpace *dock, uint32_t panel_id) {
  for (int i = 0; i < dock->panel_count; i++) {
    if (dock->panels[i].id == panel_id) {
      return &dock->panels[i];
    }
  }
  return NULL;
}

// NOTE: Docked panels use pure DDI - render directly to backbuffer.
// NO FBO allocation for docked panels (memory optimization).
// FBOs are ONLY used for floating windows (separate OS window surface).

// ============================================================================
// API - Initialization
// ============================================================================

StygianDockSpace *stygian_dock_create(void *main_gl_context,
                                      void *main_device_context) {
  StygianDockSpace *dock =
      (StygianDockSpace *)calloc(1, sizeof(StygianDockSpace));
  if (!dock)
    return NULL;

  dock->main_gl_context = main_gl_context;
  dock->main_device_context = main_device_context;

  // Defaults
  dock->tab_height = 28.0f;
  dock->splitter_size = 4.0f;
  dock->min_panel_size = 50.0f;
  dock->drop_zone_size = 40.0f;

  dock->metaball_enabled = true;
  dock->last_click_y = 0.0f;

  // Tab reorder
  dock->reorder_src_idx = -1;
  dock->reorder_dst_idx = -1;
  dock->reorder_node = NULL;

  printf("[Stygian Dock] Initialized\n");
  return dock;
}

void stygian_dock_shutdown(StygianDockSpace *dock) {
  // Delete all panel FBOs
  for (int i = 0; i < dock->panel_count; i++) {
    // StygianDockPanel *panel = &dock->panels[i];
    // No FBOs to delete (DDI mode)
  }

  // Close floating windows
  for (int i = 0; i < dock->floating_count; i++) {
    StygianFloatingWindow *fw = &dock->floating[i];
    if (fw->native_handle) {
      // TODO: DestroyWindow, wglDeleteContext
    }
  }

  // Destroy ghost window
  ghost_destroy(dock);

  memset(dock, 0, sizeof(StygianDockSpace));
  printf("[Stygian Dock] Shutdown\n");
}

void stygian_dock_destroy(StygianDockSpace *dock) {
  if (!dock)
    return;

  stygian_dock_shutdown(dock);
  free(dock);
}

// ============================================================================
// Panel Management
// ============================================================================

uint32_t stygian_dock_register_panel(StygianDockSpace *dock, const char *title,
                                     bool closable,
                                     StygianDockPanelRenderFn render,
                                     void *user_data) {
  if (dock->panel_count >= STYGIAN_DOCK_MAX_PANELS) {
    printf("[Stygian Dock] Error: Max panels reached\n");
    return 0;
  }

  StygianDockPanel *panel = &dock->panels[dock->panel_count++];
  memset(panel, 0, sizeof(StygianDockPanel));

  panel->id = ++dock->next_panel_id;
  strncpy(panel->title, title, sizeof(panel->title) - 1);
  panel->closable = closable;
  panel->visible = true;
  panel->dirty = true;
  panel->render = render;
  panel->user_data = user_data;

  printf("[Stygian Dock] Registered panel: %s (id=%u)\n", title, panel->id);
  return panel->id;
}

void stygian_dock_unregister_panel(StygianDockSpace *dock, uint32_t panel_id) {
  for (int i = 0; i < dock->panel_count; i++) {
    if (dock->panels[i].id == panel_id) {
      // Delete FBO
      // Delete FBO
      // if (dock->panels[i].fbo != 0) { ... }

      // Shift remaining panels
      for (int j = i; j < dock->panel_count - 1; j++) {
        dock->panels[j] = dock->panels[j + 1];
      }
      dock->panel_count--;

      // TODO: Remove from any nodes that reference this panel
      printf("[Stygian Dock] Unregistered panel id=%u\n", panel_id);
      return;
    }
  }
}

void stygian_dock_mark_dirty(StygianDockSpace *dock, uint32_t panel_id) {
  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (panel) {
    panel->dirty = true;
  }
}

StygianDockPanel *stygian_dock_get_panel(StygianDockSpace *dock,
                                         uint32_t panel_id) {
  return find_panel(dock, panel_id);
}

// ============================================================================
// Layout Building
// ============================================================================

StygianDockNode *stygian_dock_get_root(StygianDockSpace *dock) {
  if (!dock->root) {
    dock->root = alloc_node(dock);
    if (dock->root) {
      dock->root->split_type = STYGIAN_DOCK_SPLIT_NONE;
      printf("[Stygian Dock] Created root node (id=%u)\n", dock->root->id);
    }
  }
  return dock->root;
}

void stygian_dock_add_panel_to_node(StygianDockSpace *dock,
                                    StygianDockNode *node, uint32_t panel_id) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE) {
    printf("[Stygian Dock] Error: Can only add panels to leaf nodes\n");
    return;
  }

  if (node->panel_count >= STYGIAN_DOCK_MAX_TABS_PER_NODE) {
    printf("[Stygian Dock] Error: Max tabs in node reached\n");
    return;
  }

  node->panel_ids[node->panel_count++] = panel_id;
  node->active_panel = node->panel_count - 1;

  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (panel) {
    printf("[Stygian Dock] Added panel '%s' to node %u\n", panel->title,
           node->id);
  }
}

void stygian_dock_split(StygianDockSpace *dock, StygianDockNode *node,
                        StygianDockSplit direction, float ratio,
                        StygianDockNode **out_a, StygianDockNode **out_b) {
  if (!node)
    return;

  // Clamp ratio
  if (ratio < 0.1f)
    ratio = 0.1f;
  if (ratio > 0.9f)
    ratio = 0.9f;

  // Create two child nodes
  StygianDockNode *child_a = alloc_node(dock);
  StygianDockNode *child_b = alloc_node(dock);

  if (!child_a || !child_b) {
    printf("[Stygian Dock] Error: Failed to allocate child nodes\n");
    return;
  }

  // Move existing panels to child_a
  child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;
  memcpy(child_a->panel_ids, node->panel_ids, sizeof(node->panel_ids));
  child_a->panel_count = node->panel_count;
  child_a->active_panel = node->active_panel;
  child_a->parent = node;

  // child_b starts empty (leaf)
  child_b->split_type = STYGIAN_DOCK_SPLIT_NONE;
  child_b->parent = node;

  // Convert node to split node
  node->split_type = direction;
  node->split_ratio = ratio;
  node->child_a = child_a;
  node->child_b = child_b;
  node->panel_count = 0;
  memset(node->panel_ids, 0, sizeof(node->panel_ids));

  printf("[Stygian Dock] Split node %u (%s), ratio=%.2f\n", node->id,
         direction == STYGIAN_DOCK_SPLIT_HORIZONTAL ? "H" : "V", ratio);

  // Mark layout dirty - tree structure changed
  dock->layout_dirty = true;

  if (out_a)
    *out_a = child_a;
  if (out_b)
    *out_b = child_b;
}

// ============================================================================
// Layout Calculation (recursive)
// ============================================================================
// OPTIMIZATION IMPLEMENTED: Dirty flag skips recalc when tree unchanged.
// Set layout_dirty=true in: split(), execute_drop(), collapse_node().

static void calculate_layout_recursive(StygianDockSpace *dock,
                                       StygianDockNode *node, float x, float y,
                                       float w, float h) {
  if (!node)
    return;

  node->x = x;
  node->y = y;
  node->w = w;
  node->h = h;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    // Leaf node - pure DDI, no FBO needed
    // Panel content is rendered directly in render_node_recursive
  } else if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
    // Top/Bottom split
    float split_y = y + h * node->split_ratio;
    float splitter = dock->splitter_size;

    calculate_layout_recursive(dock, node->child_a, x, y, w,
                               split_y - y - splitter / 2);
    calculate_layout_recursive(dock, node->child_b, x, split_y + splitter / 2,
                               w, y + h - split_y - splitter / 2);
  } else if (node->split_type == STYGIAN_DOCK_SPLIT_VERTICAL) {
    // Left/Right split
    float split_x = x + w * node->split_ratio;
    float splitter = dock->splitter_size;

    calculate_layout_recursive(dock, node->child_a, x, y,
                               split_x - x - splitter / 2, h);
    calculate_layout_recursive(dock, node->child_b, split_x + splitter / 2, y,
                               x + w - split_x - splitter / 2, h);
  }
}

// ============================================================================
// Rendering
// ============================================================================

static void render_node_recursive(StygianContext *ctx, StygianFont font,
                                  StygianDockSpace *dock,
                                  StygianDockNode *node) {
  if (!node)
    return;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    // Skip empty leaf nodes entirely (they should be pruned)
    if (node->panel_count == 0) {
      return; // Empty node - don't render
    }

    // Render tab bar background
    stygian_rect(ctx, node->x, node->y, node->w, dock->tab_height, 0.12f, 0.12f,
                 0.12f, 1.0f);

    // Render tabs
    float tab_x = node->x + 2.0f;
    float tab_w = 120.0f;
    if (node->panel_count > 0) {
      tab_w = (node->w - 4.0f) / node->panel_count;
      if (tab_w > 150.0f)
        tab_w = 150.0f;
      if (tab_w < 60.0f)
        tab_w = 60.0f;
    }

    for (int i = 0; i < node->panel_count; i++) {
      StygianDockPanel *panel = find_panel(dock, node->panel_ids[i]);
      if (!panel)
        continue;

      bool active = (i == node->active_panel);

      // Tab background
      float r = active ? 0.2f : 0.15f;
      float g = active ? 0.25f : 0.15f;
      float b = active ? 0.35f : 0.15f;

      stygian_rect_rounded(ctx, tab_x, node->y + 2, tab_w - 4,
                           dock->tab_height - 4, r, g, b, 1.0f, 4.0f);

      // Tab title
      if (font) {
        stygian_text(ctx, font, panel->title, tab_x + 8, node->y + 6, 14.0f,
                     0.9f, 0.9f, 0.9f, 1.0f);
      }

      // Close button (right side of tab)
      if (panel->closable) {
        float close_size = 14.0f;
        float close_x = tab_x + tab_w - close_size - 8;
        float close_y = node->y + (dock->tab_height - close_size) / 2.0f;

        // Circle background (subtle)
        stygian_rect_rounded(ctx, close_x, close_y, close_size, close_size,
                             0.4f, 0.3f, 0.3f, 0.8f, close_size / 2);

        // X icon using proper SDF icon element (centered in bounds)
        StygianElement close_icon = stygian_element_transient(ctx);
        stygian_set_bounds(ctx, close_icon, close_x, close_y, close_size,
                           close_size);
        stygian_set_type(ctx, close_icon, STYGIAN_ICON_CLOSE);
        stygian_set_color(ctx, close_icon, 1.0f, 1.0f, 1.0f, 0.9f);
      }

      tab_x += tab_w;
    }

    // Render content area background
    float content_y = node->y + dock->tab_height;
    float content_h = node->h - dock->tab_height;
    stygian_rect(ctx, node->x, content_y, node->w, content_h, 0.08f, 0.08f,
                 0.08f, 1.0f);

    // Focus highlight (border around content when focused)
    if (dock->focused_node == node) {
      float border = 2.0f;
      // Top edge
      stygian_rect(ctx, node->x, content_y, node->w, border, 0.3f, 0.5f, 0.9f,
                   0.8f);
      // Bottom edge
      stygian_rect(ctx, node->x, content_y + content_h - border, node->w,
                   border, 0.3f, 0.5f, 0.9f, 0.8f);
      // Left edge
      stygian_rect(ctx, node->x, content_y, border, content_h, 0.3f, 0.5f, 0.9f,
                   0.8f);
      // Right edge
      stygian_rect(ctx, node->x + node->w - border, content_y, border,
                   content_h, 0.3f, 0.5f, 0.9f, 0.8f);
    }

    // Render active panel content (DDI callback)
    if (node->panel_count > 0 && node->active_panel >= 0 &&
        node->active_panel < node->panel_count) {
      StygianDockPanel *panel =
          find_panel(dock, node->panel_ids[node->active_panel]);
      if (panel && panel->render) {
        // TODO: Bind FBO, render, unbind
        // For now, render directly (no FBO)
        panel->render(panel, ctx, font, node->x, content_y, node->w, content_h);
        panel->dirty = false;
      }
    }

  } else {
    // Split node - render splitter
    float splitter = dock->splitter_size;
    float r = node->splitter_hovered ? 0.4f : 0.2f;

    if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
      float split_y = node->y + node->h * node->split_ratio;
      stygian_rect(ctx, node->x, split_y - splitter / 2, node->w, splitter, r,
                   r, r, 1.0f);
    } else {
      float split_x = node->x + node->w * node->split_ratio;
      stygian_rect(ctx, split_x - splitter / 2, node->y, splitter, node->h, r,
                   r, r, 1.0f);
    }

    // Render children
    render_node_recursive(ctx, font, dock, node->child_a);
    render_node_recursive(ctx, font, dock, node->child_b);
  }
}

// ============================================================================
// Spatial Hash Grid - O(1) Drop Target Lookup
// ============================================================================

// Populate a single leaf node into the spatial grid
static void populate_spatial_grid_leaf(StygianDockSpace *dock,
                                       StygianDockNode *node) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE)
    return;
  if (node->panel_count == 0)
    return; // Skip empty nodes

  int cell_size = STYGIAN_SPATIAL_GRID_CELL;
  int start_col = (int)(node->x) / cell_size;
  int end_col = (int)(node->x + node->w) / cell_size;
  int start_row = (int)(node->y) / cell_size;
  int end_row = (int)(node->y + node->h) / cell_size;

  // Clamp to grid bounds
  if (start_col < 0)
    start_col = 0;
  if (start_row < 0)
    start_row = 0;
  if (end_col >= STYGIAN_SPATIAL_GRID_COLS)
    end_col = STYGIAN_SPATIAL_GRID_COLS - 1;
  if (end_row >= STYGIAN_SPATIAL_GRID_ROWS)
    end_row = STYGIAN_SPATIAL_GRID_ROWS - 1;

  for (int row = start_row; row <= end_row; row++) {
    for (int col = start_col; col <= end_col; col++) {
      dock->spatial_grid[row * STYGIAN_SPATIAL_GRID_COLS + col] = node;
    }
  }
}

// Recursively populate spatial grid from all leaf nodes
static void populate_spatial_grid_recursive(StygianDockSpace *dock,
                                            StygianDockNode *node) {
  if (!node)
    return;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    populate_spatial_grid_leaf(dock, node);
  } else {
    populate_spatial_grid_recursive(dock, node->child_a);
    populate_spatial_grid_recursive(dock, node->child_b);
  }
}

// Rebuild the spatial hash grid (call when layout changes)
static void rebuild_spatial_hash(StygianDockSpace *dock) {
  memset(dock->spatial_grid, 0, sizeof(dock->spatial_grid));
  populate_spatial_grid_recursive(dock, dock->root);
  dock->spatial_dirty = false;
}

// O(1) drop target lookup using spatial hash
static StygianDockNode *find_drop_target_fast(StygianDockSpace *dock, float mx,
                                              float my) {
  int col = (int)(mx) / STYGIAN_SPATIAL_GRID_CELL;
  int row = (int)(my) / STYGIAN_SPATIAL_GRID_CELL;

  // Bounds check
  if (col < 0 || col >= STYGIAN_SPATIAL_GRID_COLS || row < 0 ||
      row >= STYGIAN_SPATIAL_GRID_ROWS) {
    return NULL;
  }

  return dock->spatial_grid[row * STYGIAN_SPATIAL_GRID_COLS + col];
}

// ============================================================================
// Drop Zone Detection - Precise Proportional Zones
// ============================================================================

// Drop zone is 25% of panel dimension, with CENTER in the middle 50%
// Visual and detection MUST match exactly

static StygianDockDropZone detect_drop_zone(StygianDockSpace *dock,
                                            StygianDockNode *node, float mx,
                                            float my) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE)
    return STYGIAN_DROP_NONE;

  // Full node bounds (including tab bar)
  float nx = node->x;
  float ny = node->y;
  float nw = node->w;
  float nh = node->h;

  // Check if mouse is within node bounds at all
  if (mx < nx || mx > nx + nw || my < ny || my > ny + nh) {
    return STYGIAN_DROP_NONE;
  }

  // Tab bar area = CENTER drop (add as tab)
  if (my < ny + dock->tab_height) {
    return STYGIAN_DROP_CENTER;
  }

  // Content area (below tab bar)
  float content_y = ny + dock->tab_height;
  float content_h = nh - dock->tab_height;

  // Proportional zones: 25% edges, 50% center
  float edge_ratio = 0.25f;
  float left_edge = nx + nw * edge_ratio;
  float right_edge = nx + nw * (1.0f - edge_ratio);
  float top_edge = content_y + content_h * edge_ratio;
  float bottom_edge = content_y + content_h * (1.0f - edge_ratio);

  // Determine zone based on position
  // Priority: strongest axis wins
  float dx_left = mx - nx;
  float dx_right = (nx + nw) - mx;
  float dy_top = my - content_y;
  float dy_bottom = (content_y + content_h) - my;

  // Find minimum distance to edge
  float min_x = dx_left < dx_right ? dx_left : dx_right;
  float min_y = dy_top < dy_bottom ? dy_top : dy_bottom;

  // If in center region, return CENTER
  if (mx >= left_edge && mx <= right_edge && my >= top_edge &&
      my <= bottom_edge) {
    return STYGIAN_DROP_CENTER;
  }

  // Otherwise, determine edge zone by closest edge
  if (min_x < min_y) {
    // Horizontal edge is closer
    return dx_left < dx_right ? STYGIAN_DROP_LEFT : STYGIAN_DROP_RIGHT;
  } else {
    // Vertical edge is closer
    return dy_top < dy_bottom ? STYGIAN_DROP_TOP : STYGIAN_DROP_BOTTOM;
  }
}

// NOTE: find_drop_target_recursive() removed - replaced by O(1) spatial hash

// ============================================================================
// Drop Zone Rendering - Matches Detection EXACTLY
// ============================================================================

static void render_drop_zone_overlay(StygianContext *ctx,
                                     StygianDockSpace *dock,
                                     StygianDockNode *node,
                                     StygianDockDropZone zone) {
  if (!node || zone == STYGIAN_DROP_NONE)
    return;

  float nx = node->x;
  float ny = node->y;
  float nw = node->w;
  float nh = node->h;
  float content_y = ny + dock->tab_height;
  float content_h = nh - dock->tab_height;

  // Highlight color (semi-transparent blue)
  float r = 0.2f, g = 0.5f, b = 0.9f, a = 0.35f;

  // Proportional zones: 25% edges
  float edge_ratio = 0.25f;

  switch (zone) {
  case STYGIAN_DROP_LEFT:
    stygian_rect(ctx, nx, content_y, nw * edge_ratio, content_h, r, g, b, a);
    break;
  case STYGIAN_DROP_RIGHT:
    stygian_rect(ctx, nx + nw * (1.0f - edge_ratio), content_y, nw * edge_ratio,
                 content_h, r, g, b, a);
    break;
  case STYGIAN_DROP_TOP:
    stygian_rect(ctx, nx, content_y, nw, content_h * edge_ratio, r, g, b, a);
    break;
  case STYGIAN_DROP_BOTTOM:
    stygian_rect(ctx, nx, content_y + content_h * (1.0f - edge_ratio), nw,
                 content_h * edge_ratio, r, g, b, a);
    break;
  case STYGIAN_DROP_CENTER:
    // Highlight center region AND tab bar
    stygian_rect(ctx, nx, ny, nw, dock->tab_height, r, g, b, a);
    stygian_rect(ctx, nx + nw * edge_ratio, content_y + content_h * edge_ratio,
                 nw * (1.0f - 2.0f * edge_ratio),
                 content_h * (1.0f - 2.0f * edge_ratio), r, g, b, a);
    break;
  default:
    break;
  }
}

// ============================================================================
// Execute Drop
// ============================================================================

static void execute_drop(StygianDockSpace *dock, uint32_t panel_id,
                         StygianDockNode *target, StygianDockDropZone zone) {
  if (!target || zone == STYGIAN_DROP_NONE)
    return;

  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (!panel)
    return;

  printf("[Stygian Dock] Drop panel '%s' into node %u, zone=%d\n", panel->title,
         target->id, zone);

  if (zone == STYGIAN_DROP_CENTER) {
    // Add as tab to existing container
    stygian_dock_add_panel_to_node(dock, target, panel_id);
  } else {
    // Split the target node
    // Key insight: stygian_dock_split() puts existing content in child_a
    // We need to control where the NEW panel goes

    StygianDockSplit split_dir;
    bool new_panel_in_a; // True if new panel goes to child_a (LEFT/TOP)

    switch (zone) {
    case STYGIAN_DROP_LEFT:
      split_dir = STYGIAN_DOCK_SPLIT_VERTICAL;
      new_panel_in_a = true; // New panel on LEFT (child_a)
      break;
    case STYGIAN_DROP_RIGHT:
      split_dir = STYGIAN_DOCK_SPLIT_VERTICAL;
      new_panel_in_a = false; // New panel on RIGHT (child_b)
      break;
    case STYGIAN_DROP_TOP:
      split_dir = STYGIAN_DOCK_SPLIT_HORIZONTAL;
      new_panel_in_a = true; // New panel on TOP (child_a)
      break;
    case STYGIAN_DROP_BOTTOM:
      split_dir = STYGIAN_DOCK_SPLIT_HORIZONTAL;
      new_panel_in_a = false; // New panel on BOTTOM (child_b)
      break;
    default:
      return;
    }

    // Save target's existing panels before split
    uint32_t existing_panel_ids[STYGIAN_DOCK_MAX_TABS_PER_NODE];
    int existing_count = target->panel_count;
    int existing_active = target->active_panel;
    memcpy(existing_panel_ids, target->panel_ids, sizeof(existing_panel_ids));

    // Clear target before split (so split doesn't copy anything)
    target->panel_count = 0;
    memset(target->panel_ids, 0, sizeof(target->panel_ids));

    // Split
    StygianDockNode *child_a, *child_b;
    stygian_dock_split(dock, target, split_dir, 0.5f, &child_a, &child_b);

    // Place panels in correct children
    if (new_panel_in_a) {
      // New panel in child_a (LEFT/TOP), existing in child_b (RIGHT/BOTTOM)
      stygian_dock_add_panel_to_node(dock, child_a, panel_id);
      for (int i = 0; i < existing_count; i++) {
        stygian_dock_add_panel_to_node(dock, child_b, existing_panel_ids[i]);
      }
      child_b->active_panel = existing_active;
    } else {
      // Existing in child_a (LEFT/TOP), new panel in child_b (RIGHT/BOTTOM)
      for (int i = 0; i < existing_count; i++) {
        stygian_dock_add_panel_to_node(dock, child_a, existing_panel_ids[i]);
      }
      child_a->active_panel = existing_active;
      stygian_dock_add_panel_to_node(dock, child_b, panel_id);
    }
  }
}

// ============================================================================
// Tab Reordering (within same node)
// ============================================================================

static void reorder_tabs_in_node(StygianDockNode *node, int from_idx,
                                 int to_idx) {
  if (!node || from_idx == to_idx || from_idx < 0 || to_idx < 0 ||
      from_idx >= node->panel_count || to_idx >= node->panel_count) {
    return;
  }

  uint32_t panel_id = node->panel_ids[from_idx];

  // Shift elements
  if (from_idx < to_idx) {
    // Moving right: shift left
    for (int i = from_idx; i < to_idx; i++) {
      node->panel_ids[i] = node->panel_ids[i + 1];
    }
  } else {
    // Moving left: shift right
    for (int i = from_idx; i > to_idx; i--) {
      node->panel_ids[i] = node->panel_ids[i - 1];
    }
  }
  node->panel_ids[to_idx] = panel_id;
  node->active_panel = to_idx;

  printf("[Stygian Dock] Reordered tab %d -> %d\n", from_idx, to_idx);
}

// ============================================================================
// Remove Panel from Node + Tree Pruning
// ============================================================================

// Forward declaration for recursive pruning
static void prune_empty_nodes(StygianDockSpace *dock, StygianDockNode *node);

// Collapse a split node by promoting child to parent's position
static void collapse_node_into_parent(StygianDockSpace *dock,
                                      StygianDockNode *node,
                                      StygianDockNode *surviving_child) {
  if (!node || !surviving_child)
    return;

  StygianDockNode *parent = node->parent;

  printf("[Stygian Dock] Collapsing node %u, promoting child %u\n", node->id,
         surviving_child->id);

  if (!parent) {
    // Node is root - surviving child becomes new root
    dock->root = surviving_child;
    surviving_child->parent = NULL;
    printf("[Stygian Dock] Child %u is now root\n", surviving_child->id);
  } else {
    // Replace node with surviving_child in parent
    if (parent->child_a == node) {
      parent->child_a = surviving_child;
    } else if (parent->child_b == node) {
      parent->child_b = surviving_child;
    }
    surviving_child->parent = parent;
  }

  // Clear the old node (node pool reuse not implemented yet, just mark dead)
  memset(node, 0, sizeof(StygianDockNode));

  // Mark layout dirty - tree structure changed
  dock->layout_dirty = true;
}

// Check if a node is empty (leaf with no panels)
static bool is_node_empty(StygianDockNode *node) {
  if (!node)
    return true;
  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    return node->panel_count == 0;
  }
  return false;
}

// Recursively prune empty nodes from the tree
static void prune_empty_nodes(StygianDockSpace *dock, StygianDockNode *node) {
  if (!node)
    return;

  if (node->split_type != STYGIAN_DOCK_SPLIT_NONE) {
    // Check if either child is empty leaf
    bool a_empty = is_node_empty(node->child_a);
    bool b_empty = is_node_empty(node->child_b);

    if (a_empty && b_empty) {
      // Both children empty - this split node becomes an empty leaf
      printf(
          "[Stygian Dock] Both children of node %u empty, converting to leaf\n",
          node->id);
      node->split_type = STYGIAN_DOCK_SPLIT_NONE;
      node->child_a = NULL;
      node->child_b = NULL;
      node->panel_count = 0;

      // Now this node is empty, prune upward
      if (node->parent) {
        prune_empty_nodes(dock, node->parent);
      }
    } else if (a_empty) {
      // Only child_a is empty - promote child_b
      StygianDockNode *survivor = node->child_b;
      collapse_node_into_parent(dock, node, survivor);

      // Continue pruning from survivor's parent
      if (survivor->parent) {
        prune_empty_nodes(dock, survivor->parent);
      }
    } else if (b_empty) {
      // Only child_b is empty - promote child_a
      StygianDockNode *survivor = node->child_a;
      collapse_node_into_parent(dock, node, survivor);

      // Continue pruning from survivor's parent
      if (survivor->parent) {
        prune_empty_nodes(dock, survivor->parent);
      }
    }
  }
}

static void remove_panel_from_node(StygianDockSpace *dock,
                                   StygianDockNode *node, uint32_t panel_id) {
  if (!node || node->split_type != STYGIAN_DOCK_SPLIT_NONE)
    return;

  for (int i = 0; i < node->panel_count; i++) {
    if (node->panel_ids[i] == panel_id) {
      // Shift remaining panels
      for (int j = i; j < node->panel_count - 1; j++) {
        node->panel_ids[j] = node->panel_ids[j + 1];
      }
      node->panel_count--;

      // Adjust active panel
      if (node->active_panel >= node->panel_count && node->panel_count > 0) {
        node->active_panel = node->panel_count - 1;
      }

      printf("[Stygian Dock] Removed panel from node %u (remaining: %d)\n",
             node->id, node->panel_count);

      // CRITICAL: Prune tree if this node is now empty
      if (node->panel_count == 0 && node->parent) {
        prune_empty_nodes(dock, node->parent);
      }

      return;
    }
  }
}

static StygianDockNode *find_node_with_panel(StygianDockNode *node,
                                             uint32_t panel_id) {
  if (!node)
    return NULL;

  if (node->split_type == STYGIAN_DOCK_SPLIT_NONE) {
    for (int i = 0; i < node->panel_count; i++) {
      if (node->panel_ids[i] == panel_id) {
        return node;
      }
    }
    return NULL;
  }

  StygianDockNode *found = find_node_with_panel(node->child_a, panel_id);
  if (found)
    return found;
  return find_node_with_panel(node->child_b, panel_id);
}

// ============================================================================
// Input Handling
// ============================================================================

static void handle_input_recursive(StygianContext *ctx, StygianDockSpace *dock,
                                   StygianDockNode *node, int mx, int my,
                                   bool mouse_down, bool was_down) {
  if (!node)
    return;

  if (node->split_type != STYGIAN_DOCK_SPLIT_NONE) {
    // Handle splitter interaction
    float splitter = dock->splitter_size;
    bool over_splitter = false;

    if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
      float split_y = node->y + node->h * node->split_ratio;
      over_splitter = mx >= node->x && mx < node->x + node->w &&
                      my >= split_y - splitter && my < split_y + splitter;
    } else {
      float split_x = node->x + node->w * node->split_ratio;
      over_splitter = mx >= split_x - splitter && mx < split_x + splitter &&
                      my >= node->y && my < node->y + node->h;
    }

    node->splitter_hovered = over_splitter;

    // Double-click splitter = reset to 50%
    if (over_splitter && mouse_down && !was_down && !dock->dragging_panel_id) {
      uint64_t now = get_time_ms();
      float dx = (float)mx - dock->last_click_x;
      float dy = (float)my - dock->last_click_y;
      float dist = dx * dx + dy * dy;

      if (now - dock->last_click_time_ms < DOUBLE_CLICK_TIME_MS &&
          dist < DOUBLE_CLICK_DISTANCE * DOUBLE_CLICK_DISTANCE) {
        // Double-click detected - reset to 50%
        node->split_ratio = 0.5f;
        dock->layout_dirty = true;
        printf("[Stygian Dock] Splitter double-click: reset to 50%%\n");
      } else {
        // Single click - start dragging
        node->splitter_dragging = true;
      }

      dock->last_click_time_ms = now;
      dock->last_click_x = (float)mx;
      dock->last_click_y = (float)my;
    }

    // Continue dragging splitter
    if (node->splitter_dragging && mouse_down) {
      if (node->split_type == STYGIAN_DOCK_SPLIT_HORIZONTAL) {
        node->split_ratio = (my - node->y) / node->h;
      } else {
        node->split_ratio = (mx - node->x) / node->w;
      }
      // Clamp
      if (node->split_ratio < 0.1f)
        node->split_ratio = 0.1f;
      if (node->split_ratio > 0.9f)
        node->split_ratio = 0.9f;

      // CRITICAL: Mark layout dirty so children resize
      dock->layout_dirty = true;
    }

    // Stop dragging splitter
    if (!mouse_down) {
      node->splitter_dragging = false;
    }

    // Recurse to children
    handle_input_recursive(ctx, dock, node->child_a, mx, my, mouse_down,
                           was_down);
    handle_input_recursive(ctx, dock, node->child_b, mx, my, mouse_down,
                           was_down);

  } else {
    // Leaf node - handle tab clicks and drag start
    if (mouse_down && !was_down && !dock->dragging_panel_id) {
      // Check if click is in tab bar
      if (my >= node->y && my < node->y + dock->tab_height && mx >= node->x &&
          mx < node->x + node->w) {

        // Find which tab was clicked
        float tab_x = node->x + 2.0f;
        float tab_w = 120.0f;
        if (node->panel_count > 0) {
          tab_w = (node->w - 4.0f) / node->panel_count;
          if (tab_w > 150.0f)
            tab_w = 150.0f;
          if (tab_w < 60.0f)
            tab_w = 60.0f;
        }

        for (int i = 0; i < node->panel_count; i++) {
          if (mx >= tab_x && mx < tab_x + tab_w) {
            StygianDockPanel *panel = find_panel(dock, node->panel_ids[i]);

            // Check if close button was clicked
            if (panel && panel->closable) {
              float close_size = 14.0f;
              float close_x = tab_x + tab_w - close_size - 8;
              float close_y = node->y + (dock->tab_height - close_size) / 2.0f;

              if (mx >= close_x && mx < close_x + close_size && my >= close_y &&
                  my < close_y + close_size) {
                // Close button clicked - remove panel
                printf("[Stygian Dock] Closing panel: %s\n", panel->title);
                remove_panel_from_node(dock, node, node->panel_ids[i]);
                dock->layout_dirty = true;
                break;
              }
            }

            // Not close button - select tab and start potential drag
            node->active_panel = i;
            dock->focused_node = node; // Set focus to this panel

            dock->dragging_panel_id = node->panel_ids[i];
            dock->drag_start_x = (float)mx;
            dock->drag_start_y = (float)my;
            dock->drag_offset_x = (float)mx - tab_x;
            dock->drag_offset_y = (float)my - node->y;
            dock->drag_started = false;

            if (panel) {
              printf("[Stygian Dock] Clicked panel: %s\n", panel->title);
            }
            break;
          }
          tab_x += tab_w;
        }
      }
    }
  }
}

// ============================================================================
// Main Update
// ============================================================================

void stygian_dock_update(StygianContext *ctx, StygianFont font,
                         StygianDockSpace *dock, float x, float y, float w,
                         float h) {
  if (!dock->root) {
    stygian_dock_get_root(dock);
    dock->layout_dirty = true;
  }

  // Process floating windows - event pump, render, auto-close empty
  StygianAP *ap = stygian_get_ap(ctx);
  bool any_floating_down = false;
  for (int i = dock->floating_count - 1; i >= 0; i--) {
    StygianFloatingWindow *fw = &dock->floating[i];

    // Process window events
    if (fw->window) {
      stygian_window_process_events(fw->window);

      // Check if window was closed
      if (stygian_window_should_close(fw->window)) {
        // Window closed by user - move panel back to main dock
        if (fw->root_node && fw->root_node->panel_count > 0) {
          for (int p = 0; p < fw->root_node->panel_count; p++) {
            stygian_dock_add_panel_to_node(dock, dock->root,
                                           fw->root_node->panel_ids[p]);
          }
        }
        // Destroy surface first
        if (fw->surface && ap) {
          stygian_ap_surface_destroy(ap, fw->surface);
          fw->surface = NULL;
        }
        // Destroy window and remove
        stygian_window_destroy(fw->window);
        for (int j = i; j < dock->floating_count - 1; j++) {
          dock->floating[j] = dock->floating[j + 1];
        }
        dock->floating_count--;
        continue;
      }

      // Render floating window content
      if (fw->surface && fw->root_node && ap) {
        int fw_w, fw_h; // Logical size for layout
        stygian_window_get_size(fw->window, &fw_w, &fw_h);

        int fb_w, fb_h; // Physical size for render target
        stygian_window_get_framebuffer_size(fw->window, &fb_w, &fb_h);

        // Handle input for floating window (for tab dragging)
        int fw_mx, fw_my;
        stygian_mouse_pos(fw->window, &fw_mx, &fw_my);
        bool fw_mouse_down = stygian_mouse_down(fw->window, STYGIAN_MOUSE_LEFT);
        if (fw_mouse_down)
          any_floating_down = true;

        // Calculate layout for floating window's content (Logical)
        fw->root_node->x = 0;
        fw->root_node->y = 0;
        fw->root_node->w = (float)fw_w;
        fw->root_node->h = (float)fw_h;

        // DEBUG: Print sizes periodically
        static int debug_counter = 0;
        if (debug_counter++ % 600 == 0) {
          printf("[Stygian Dock] Float Win %d: Logical %dx%d, Physical %dx%d\n",
                 i, fw_w, fw_h, fb_w, fb_h);
        }

        // Check for tab click/drag in floating window
        if (fw->root_node->panel_count > 0 && fw_mouse_down &&
            !fw->prev_mouse_down) {
          // Check if clicking in tab bar area
          if (fw_my >= 0 && fw_my < dock->tab_height) {
            // Find which tab was clicked
            float tab_w = 120.0f;
            if (fw->root_node->panel_count > 0) {
              tab_w = (fw->root_node->w - 4.0f) / fw->root_node->panel_count;
              if (tab_w > 150.0f)
                tab_w = 150.0f;
              if (tab_w < 60.0f)
                tab_w = 60.0f;
            }
            int clicked_tab = (int)((fw_mx - 2.0f) / tab_w);
            if (clicked_tab >= 0 && clicked_tab < fw->root_node->panel_count) {
              // Start dragging this panel
              dock->dragging_panel_id = fw->root_node->panel_ids[clicked_tab];
              dock->drag_start_x = (float)fw_mx;
              dock->drag_start_y = (float)fw_my;
              dock->drag_started = false;
              dock->dragging_from_floating = true;
              dock->dragging_floating_idx = i;
              printf(
                  "[Stygian Dock] Started dragging from floating: panel %u\n",
                  dock->dragging_panel_id);
            }
          }
        }
        fw->prev_mouse_down = fw_mouse_down;

        // Save current element state (we'll render to floating surface)
        uint32_t saved_element_count = ctx->element_count;

        // Reset element count for fresh floating window render
        ctx->element_count = 0;

        fw->root_node->x = 0;
        fw->root_node->y = 0;
        fw->root_node->w = (float)fw_w;
        fw->root_node->h = (float)fw_h;

        // Render the floating window's dock node content
        render_node_recursive(ctx, font, dock, fw->root_node);

        // Begin rendering to floating window's surface (pass PHYSICAL size)
        // Note: Using a modified surface_begin signature or assuming checks
        stygian_ap_surface_begin(ap, fw->surface, fb_w, fb_h);

        // Submit elements to the floating surface
        if (ctx->element_count > 0) {
          stygian_ap_surface_submit(ap, fw->surface, ctx->elements,
                                    ctx->element_count);
        }

        stygian_ap_surface_end(ap, fw->surface);
        stygian_ap_surface_swap(ap, fw->surface);

        // Restore element count for main window render
        ctx->element_count = saved_element_count;
      }
    }

    // Auto-close empty floating windows
    if (!fw->root_node || fw->root_node->panel_count == 0) {
      printf("[Stygian Dock] Auto-closing empty floating window\n");
      // Destroy surface first
      if (fw->surface && ap) {
        stygian_ap_surface_destroy(ap, fw->surface);
        fw->surface = NULL;
      }
      if (fw->window) {
        stygian_window_destroy(fw->window);
      }
      // Remove from array
      for (int j = i; j < dock->floating_count - 1; j++) {
        dock->floating[j] = dock->floating[j + 1];
      }
      dock->floating_count--;
    }
  }

  // Check if resize happened - forces layout recalc
  if (w != dock->last_layout_w || h != dock->last_layout_h) {
    dock->layout_dirty = true;
    dock->last_layout_w = w;
    dock->last_layout_h = h;
  }

  // O(1) optimization: only recalculate layout when dirty
  if (dock->layout_dirty) {
    calculate_layout_recursive(dock, dock->root, x, y, w, h);
    dock->layout_dirty = false;
    dock->spatial_dirty = true; // Layout changed, rebuild spatial hash
  }

  // O(1) optimization: rebuild spatial hash when layout changes
  if (dock->spatial_dirty) {
    rebuild_spatial_hash(dock);
  }

  // Get input (no static - use struct for thread safety)
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  bool mouse_down = stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
  bool global_mouse_down = mouse_down || any_floating_down;
  bool was_down = dock->prev_mouse_down;

  // Override mouse position for hit testing if dragging from floating (or just
  // always to be safe) This ensures we detect drops even if valid main window
  // events aren't firing
  if (dock->dragging_from_floating || any_floating_down) {
#ifdef _WIN32
    POINT pt;
    if (GetCursorPos(&pt)) {
      int client_x, client_y;
      stygian_window_screen_to_client(win, pt.x, pt.y, &client_x, &client_y);
      mx = client_x;
      my = client_y;
    }
#endif
  }

  // ESC = cancel drag
  if (dock->dragging_panel_id && stygian_key_down(win, STYGIAN_KEY_ESCAPE)) {
    printf("[Stygian Dock] Drag cancelled (ESC)\n");
    dock->dragging_panel_id = 0;
    dock->drag_started = false;
    dock->drop_target = NULL;
    dock->drop_zone = STYGIAN_DROP_NONE;
  }

  // Handle drag continuation
  if (dock->dragging_panel_id && global_mouse_down) {
    float dx, dy;

    // When dragging from floating, use screen-space coordinates
    if (dock->dragging_from_floating) {
#ifdef _WIN32
      POINT pt;
      GetCursorPos(&pt);
      // Use screen-space delta (set drag_started immediately for floating)
      if (!dock->drag_started) {
        dock->drag_started = true;
        // Store screen position as new reference
        dock->drag_start_x = (float)pt.x;
        dock->drag_start_y = (float)pt.y;
        StygianDockPanel *panel = find_panel(dock, dock->dragging_panel_id);
        if (panel) {
          printf("[Stygian Dock] Dragging from floating: %s\n", panel->title);
        }
      }
      dx = (float)pt.x - dock->drag_start_x;
      dy = (float)pt.y - dock->drag_start_y;
#else
      dx = 0;
      dy = 0;
      dock->drag_started = true;
#endif
    } else {
      dx = (float)mx - dock->drag_start_x;
      dy = (float)my - dock->drag_start_y;
    }
    float drag_dist = dx * dx + dy * dy;

    // Check if drag threshold exceeded (5 pixels) - only for main dock drags
    if (!dock->drag_started && !dock->dragging_from_floating &&
        drag_dist > 25.0f) {
      dock->drag_started = true;
      StygianDockPanel *panel = find_panel(dock, dock->dragging_panel_id);
      if (panel) {
        printf("[Stygian Dock] Started dragging: %s\n", panel->title);
      }
    }

    // Find drop target using O(1) spatial hash
    if (dock->drag_started) {
      dock->drop_zone = STYGIAN_DROP_NONE;

      // Edge detection: if near window edge, allow floating (clear drop target)
      int win_w, win_h;
      stygian_window_get_size(win, &win_w, &win_h);
      float edge_margin = 30.0f; // 30px from edge = floating zone
      bool near_edge = (mx < edge_margin || mx > win_w - edge_margin ||
                        my < edge_margin || my > win_h - edge_margin);

      if (!near_edge) {
        // O(1) lookup: get node from spatial grid
        StygianDockNode *target =
            find_drop_target_fast(dock, (float)mx, (float)my);
        if (target) {
          // Now determine which drop zone within this node
          dock->drop_zone =
              detect_drop_zone(dock, target, (float)mx, (float)my);
          dock->drop_target =
              (dock->drop_zone != STYGIAN_DROP_NONE) ? target : NULL;
        } else {
          dock->drop_target = NULL;
        }
        // Hide ghost when not near edge
        ghost_hide(dock);
      } else {
        // Near edge = float zone - show external ghost
        dock->drop_target = NULL;

        // Get screen-space mouse position for ghost window
#ifdef _WIN32
        POINT pt;
        GetCursorPos(&pt);
        ghost_show(dock, pt.x, pt.y);
#else
        // Fallback: use window-relative coords
        ghost_show(dock, mx, my);
#endif
      }
    }
  }

  // Handle drop
  if (dock->dragging_panel_id && !global_mouse_down && was_down) {
    if (dock->drag_started && dock->drop_target &&
        dock->drop_zone != STYGIAN_DROP_NONE) {
      // Drop on valid target
      StygianDockNode *source_node = NULL;

      // Find source - either from floating window or main dock
      if (dock->dragging_from_floating) {
        // Source is in a floating window
        if (dock->dragging_floating_idx >= 0 &&
            dock->dragging_floating_idx < dock->floating_count) {
          StygianFloatingWindow *fw =
              &dock->floating[dock->dragging_floating_idx];
          source_node = fw->root_node;

          // Remove panel from floating window
          remove_panel_from_node(dock, source_node, dock->dragging_panel_id);

          // Execute drop to main dock
          execute_drop(dock, dock->dragging_panel_id, dock->drop_target,
                       dock->drop_zone);
          printf("[Stygian Dock] Redocked panel from floating window\n");
        }
      } else {
        // Source is in main dock
        source_node = find_node_with_panel(dock->root, dock->dragging_panel_id);

        if (source_node && source_node != dock->drop_target) {
          // Different node - normal cross-panel drop
          remove_panel_from_node(dock, source_node, dock->dragging_panel_id);
          execute_drop(dock, dock->dragging_panel_id, dock->drop_target,
                       dock->drop_zone);
        } else if (source_node == dock->drop_target) {
          // Same node - self-drop handling
          if (dock->drop_zone == STYGIAN_DROP_CENTER) {
            // Center drop on same node - check if reordering tabs
            // Calculate which tab position mouse is over
            float tab_w = 120.0f;
            if (source_node->panel_count > 0) {
              tab_w = (source_node->w - 4.0f) / source_node->panel_count;
              if (tab_w > 150.0f)
                tab_w = 150.0f;
              if (tab_w < 60.0f)
                tab_w = 60.0f;
            }

            // Find source index
            int src_idx = -1;
            for (int i = 0; i < source_node->panel_count; i++) {
              if (source_node->panel_ids[i] == dock->dragging_panel_id) {
                src_idx = i;
                break;
              }
            }

            // Find target index based on mouse position
            int dst_idx = (int)((mx - source_node->x - 2.0f) / tab_w);
            if (dst_idx < 0)
              dst_idx = 0;
            if (dst_idx >= source_node->panel_count)
              dst_idx = source_node->panel_count - 1;

            if (src_idx >= 0 && src_idx != dst_idx) {
              reorder_tabs_in_node(source_node, src_idx, dst_idx);
            } else {
              printf("[Stygian Dock] Self-drop to center - no-op\n");
            }
          } else if (source_node->panel_count > 1) {
            // Edge drop on same node with multiple tabs - split this panel
            printf("[Stygian Dock] Self-split: '%s' to zone %d\n",
                   find_panel(dock, dock->dragging_panel_id)->title,
                   dock->drop_zone);

            // Remove the panel first (before split messes with structure)
            remove_panel_from_node(dock, source_node, dock->dragging_panel_id);

            // Now execute drop on the (now modified) source node
            execute_drop(dock, dock->dragging_panel_id, source_node,
                         dock->drop_zone);
          } else {
            // Only one panel in node - splitting would create empty sibling
            printf("[Stygian Dock] Self-drop on single-panel node - no-op\n");
          }
        }
      }
    } else if (dock->drag_started && !dock->drop_target) {
      // Dropped outside any dock zone - float the panel
      // Position at SCREEN mouse location with default size
      float float_w = 400.0f;
      float float_h = 300.0f;
      float screen_x, screen_y;
#ifdef _WIN32
      POINT pt;
      GetCursorPos(&pt);
      screen_x = (float)pt.x - float_w / 2;
      screen_y = (float)pt.y - float_h / 2;
#else
      screen_x = (float)mx - float_w / 2;
      screen_y = (float)my - float_h / 2;
#endif
      stygian_dock_float_panel(ctx, dock, dock->dragging_panel_id, screen_x,
                               screen_y, float_w, float_h);
    }

    // Reset drag state
    dock->dragging_panel_id = 0;
    dock->drag_started = false;
    dock->drop_target = NULL;
    dock->drop_zone = STYGIAN_DROP_NONE;
    dock->dragging_from_floating = false;
    dock->dragging_floating_idx = -1;

    // Hide ghost window
    ghost_hide(dock);
  }

  // Handle regular input (clicks, splitter drag)
  handle_input_recursive(ctx, dock, dock->root, mx, my, global_mouse_down,
                         was_down);
  dock->prev_mouse_down = global_mouse_down; // Update for next frame

  // Render dock layout
  render_node_recursive(ctx, font, dock, dock->root);

  // Render drag preview
  if (dock->dragging_panel_id && dock->drag_started) {
    StygianDockPanel *panel = find_panel(dock, dock->dragging_panel_id);
    if (panel) {
      // Dragged tab preview (semi-transparent)
      float preview_w = 120.0f;
      float preview_h = dock->tab_height;
      float preview_x = (float)mx - dock->drag_offset_x;
      float preview_y = (float)my - dock->drag_offset_y;

      stygian_rect(ctx, preview_x, preview_y, preview_w, preview_h, 0.3f, 0.4f,
                   0.6f, 0.7f);
      if (font) {
        stygian_text(ctx, font, panel->title, preview_x + 8, preview_y + 6,
                     14.0f, 1.0f, 1.0f, 1.0f, 1.0f);
      }
    }

    // Render drop zone highlight or floating preview
    if (dock->drop_target) {
      render_drop_zone_overlay(ctx, dock, dock->drop_target, dock->drop_zone);
    } else {
      // No drop target = floating zone - show floating window preview
      float float_w = 400.0f;
      float float_h = 300.0f;
      float float_x = (float)mx - float_w / 2;
      float float_y = (float)my - float_h / 2;

      // Window-like preview (darker, larger)
      stygian_rect(ctx, float_x, float_y, float_w, float_h, 0.15f, 0.15f, 0.18f,
                   0.85f);
      // Tab bar at top
      stygian_rect(ctx, float_x, float_y, float_w, dock->tab_height, 0.2f,
                   0.22f, 0.25f, 0.9f);
      // Title
      if (font) {
        stygian_text(ctx, font, panel->title, float_x + 10, float_y + 6, 14.0f,
                     0.9f, 0.9f, 0.9f, 1.0f);
      }
      // "Float" indicator
      if (font) {
        stygian_text(ctx, font, "(FLOAT)", float_x + float_w - 60, float_y + 6,
                     12.0f, 0.5f, 0.7f, 1.0f, 1.0f);
      }
    }
  }

  // Restore main window context so subsequent rendering works
  // Restore main window context so subsequent rendering works
  if (ap) {
    stygian_ap_make_current(ap);

    // Also restore viewport (CRITICAL for fixing "small main window" bug)
    // Use framebuffer size to match physical pixels
    StygianWindow *win = stygian_get_window(ctx);
    if (win) {
      int fb_w, fb_h;
      stygian_window_get_framebuffer_size(win, &fb_w, &fb_h);
      stygian_ap_set_viewport(ap, fb_w, fb_h);
    }
  }
}

void stygian_dock_composite_main(StygianDockSpace *dock) {
  // TODO: Blit all docked panel FBOs to main window
}

void stygian_dock_render_floating(StygianDockSpace *dock, int floating_idx) {
  // TODO: Blit floating window panel FBO
}

// ============================================================================
// Floating Windows
// ============================================================================

void stygian_dock_float_panel(StygianContext *ctx, StygianDockSpace *dock,
                              uint32_t panel_id, float x, float y, float w,
                              float h) {
  if (!ctx || !dock || dock->floating_count >= STYGIAN_DOCK_MAX_FLOATING) {
    printf("[Stygian Dock] Cannot float panel: invalid args or max reached\n");
    return;
  }

  // Find panel
  StygianDockPanel *panel = find_panel(dock, panel_id);
  if (!panel) {
    printf("[Stygian Dock] Cannot float panel %u: not found\n", panel_id);
    return;
  }

  // Find and remove from current node
  StygianDockNode *source_node = find_node_with_panel(dock->root, panel_id);
  if (source_node) {
    remove_panel_from_node(dock, source_node, panel_id);
  }

  // Create floating window
  StygianFloatingWindow *fw = &dock->floating[dock->floating_count];
  memset(fw, 0, sizeof(StygianFloatingWindow));

  // Create native window
  StygianWindowConfig cfg = {.width = (int)w,
                             .height = (int)h,
                             .title = panel->title,
                             .flags = STYGIAN_WINDOW_RESIZABLE,
                             .gl_major = 4,
                             .gl_minor = 3};

  StygianWindow *win = stygian_window_create(&cfg);
  if (!win) {
    printf("[Stygian Dock] Failed to create floating window\n");
    return;
  }

  // Position window
  stygian_window_set_position(win, (int)x, (int)y);

  // Store in floating window struct
  fw->window = win;
  fw->native_handle = stygian_window_native_handle(win);
  fw->x = x;
  fw->y = y;
  fw->w = w;
  fw->h = h;

  // Create render surface for this window via AP
  StygianAP *ap = stygian_get_ap(ctx);
  if (ap) {
    fw->surface = stygian_ap_surface_create(ap, win);
    if (!fw->surface) {
      printf("[Stygian Dock] Warning: Failed to create render surface\n");
    }
  }

  // Create a root node for this floating window's content
  if (dock->node_count < STYGIAN_DOCK_MAX_NODES) {
    StygianDockNode *node = alloc_node(dock);
    node->split_type = STYGIAN_DOCK_SPLIT_NONE;
    stygian_dock_add_panel_to_node(dock, node, panel_id);
    fw->root_node = node;
  }

  // Metaball animation state
  fw->undock_progress = 0.0f;
  fw->blend_radius = 100.0f;
  fw->melting = true;
  fw->visible = true;

  dock->floating_count++;
  dock->layout_dirty = true;

  printf("[Stygian Dock] Floated panel '%s' at (%.0f, %.0f) size %.0fx%.0f\n",
         panel->title, x, y, w, h);
}

void stygian_dock_dock_floating(StygianContext *ctx, StygianDockSpace *dock,
                                int floating_idx, StygianDockNode *target,
                                StygianDockDropZone zone) {
  if (!dock || floating_idx < 0 || floating_idx >= dock->floating_count) {
    printf("[Stygian Dock] Invalid floating index %d\n", floating_idx);
    return;
  }

  StygianFloatingWindow *fw = &dock->floating[floating_idx];

  // Get panel from floating window's root node
  if (fw->root_node && fw->root_node->panel_count > 0) {
    uint32_t panel_id = fw->root_node->panel_ids[0];
    StygianDockPanel *panel = find_panel(dock, panel_id);

    if (panel) {
      printf("[Stygian Dock] Re-docking panel '%s' to zone %d\n", panel->title,
             zone);

      // Add panel to target
      execute_drop(dock, panel_id, target, zone);

      // Clear floating window's node
      fw->root_node->panel_count = 0;
    }
  }

  // Destroy render surface first
  if (fw->surface && ctx) {
    StygianAP *ap = stygian_get_ap(ctx);
    if (ap) {
      stygian_ap_surface_destroy(ap, fw->surface);
    }
    fw->surface = NULL;
  }

  // Destroy window
  if (fw->window) {
    stygian_window_destroy(fw->window);
    fw->window = NULL;
  }

  // Remove from floating array (shift remaining)
  for (int i = floating_idx; i < dock->floating_count - 1; i++) {
    dock->floating[i] = dock->floating[i + 1];
  }
  dock->floating_count--;
  dock->layout_dirty = true;
}

// ============================================================================
// Serialization - JSON Format
// ============================================================================

// Simple JSON writer (no external deps)
typedef struct {
  FILE *f;
  int indent;
} JsonWriter;

static void json_indent(JsonWriter *w) {
  for (int i = 0; i < w->indent; i++)
    fprintf(w->f, "  ");
}

static void json_begin_obj(JsonWriter *w) {
  fprintf(w->f, "{\n");
  w->indent++;
}

static void json_end_obj(JsonWriter *w, bool comma) {
  w->indent--;
  json_indent(w);
  fprintf(w->f, "}%s\n", comma ? "," : "");
}

static void json_key(JsonWriter *w, const char *key) {
  json_indent(w);
  fprintf(w->f, "\"%s\": ", key);
}

static void json_str(JsonWriter *w, const char *key, const char *val,
                     bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": \"%s\"%s\n", key, val, comma ? "," : "");
}

static void json_int(JsonWriter *w, const char *key, int val, bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": %d%s\n", key, val, comma ? "," : "");
}

static void json_float(JsonWriter *w, const char *key, float val, bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": %.4f%s\n", key, val, comma ? "," : "");
}

static void json_bool(JsonWriter *w, const char *key, bool val, bool comma) {
  json_indent(w);
  fprintf(w->f, "\"%s\": %s%s\n", key, val ? "true" : "false",
          comma ? "," : "");
}

// Recursive node serialization
static void serialize_node(JsonWriter *w, StygianDockNode *node, bool comma) {
  if (!node) {
    json_indent(w);
    fprintf(w->f, "null%s\n", comma ? "," : "");
    return;
  }

  json_indent(w);
  json_begin_obj(w);

  json_int(w, "id", node->id, true);
  json_int(w, "split_type", node->split_type, true);
  json_float(w, "split_ratio", node->split_ratio, true);

  // Panel IDs array
  json_key(w, "panel_ids");
  fprintf(w->f, "[");
  for (int i = 0; i < node->panel_count; i++) {
    fprintf(w->f, "%d%s", node->panel_ids[i],
            i < node->panel_count - 1 ? ", " : "");
  }
  fprintf(w->f, "],\n");

  json_int(w, "active_panel", node->active_panel, true);

  // Children
  json_key(w, "child_a");
  serialize_node(w, node->child_a, true);
  json_key(w, "child_b");
  serialize_node(w, node->child_b, false);

  json_end_obj(w, comma);
}

bool stygian_dock_save(StygianDockSpace *dock, const char *path) {
  if (!dock || !path)
    return false;

  FILE *f = fopen(path, "w");
  if (!f) {
    printf("[Stygian Dock] Failed to open %s for writing\n", path);
    return false;
  }

  JsonWriter w = {.f = f, .indent = 0};

  json_begin_obj(&w);

  // Version
  json_int(&w, "version", 1, true);

  // Root node tree
  json_key(&w, "root");
  serialize_node(&w, dock->root, true);

  // Floating windows
  json_key(&w, "floating");
  fprintf(f, "[\n");
  w.indent++;
  for (int i = 0; i < dock->floating_count; i++) {
    StygianFloatingWindow *fw = &dock->floating[i];
    json_indent(&w);
    json_begin_obj(&w);

    json_float(&w, "x", fw->x, true);
    json_float(&w, "y", fw->y, true);
    json_float(&w, "w", fw->w, true);
    json_float(&w, "h", fw->h, true);

    // Serialize floating window's root node
    json_key(&w, "root");
    serialize_node(&w, fw->root_node, false);

    json_end_obj(&w, i < dock->floating_count - 1);
  }
  w.indent--;
  json_indent(&w);
  fprintf(f, "]\n");

  json_end_obj(&w, false);

  fclose(f);
  printf("[Stygian Dock] Saved layout to %s\n", path);
  return true;
}

// ============================================================================
// JSON Parsing (Simple)
// ============================================================================

typedef struct {
  const char *p;
  const char *end;
} JsonReader;

static void skip_ws(JsonReader *r) {
  while (r->p < r->end &&
         (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r'))
    r->p++;
}

static bool match_char(JsonReader *r, char c) {
  skip_ws(r);
  if (r->p < r->end && *r->p == c) {
    r->p++;
    return true;
  }
  return false;
}

static bool parse_string(JsonReader *r, char *out, int max_len) {
  skip_ws(r);
  if (*r->p != '"')
    return false;
  r->p++;

  int i = 0;
  while (r->p < r->end && *r->p != '"' && i < max_len - 1) {
    out[i++] = *r->p++;
  }
  out[i] = '\0';

  if (*r->p == '"')
    r->p++;
  return true;
}

static bool parse_int(JsonReader *r, int *out) {
  skip_ws(r);
  char *end;
  *out = (int)strtol(r->p, &end, 10);
  if (end == r->p)
    return false;
  r->p = end;
  return true;
}

static bool parse_float(JsonReader *r, float *out) {
  skip_ws(r);
  char *end;
  *out = strtof(r->p, &end);
  if (end == r->p)
    return false;
  r->p = end;
  return true;
}

static StygianDockNode *parse_node(JsonReader *r, StygianDockSpace *dock);

static bool parse_key_value(JsonReader *r, StygianDockSpace *dock,
                            StygianDockNode *node, StygianFloatingWindow *fw) {
  char key[64];
  if (!parse_string(r, key, sizeof(key)))
    return false;
  if (!match_char(r, ':'))
    return false;

  skip_ws(r);

  if (node) {
    if (strcmp(key, "id") == 0) {
      parse_int(r, (int *)&node->id);
    } else if (strcmp(key, "split_type") == 0) {
      int st;
      parse_int(r, &st);
      node->split_type = (StygianDockSplit)st;
    } else if (strcmp(key, "split_ratio") == 0) {
      parse_float(r, &node->split_ratio);
    } else if (strcmp(key, "active_panel") == 0) {
      parse_int(r, &node->active_panel);
    } else if (strcmp(key, "panel_ids") == 0) {
      if (!match_char(r, '['))
        return false;
      node->panel_count = 0;
      while (!match_char(r, ']')) {
        int id;
        if (parse_int(r, &id)) {
          if (node->panel_count < STYGIAN_DOCK_MAX_TABS_PER_NODE) {
            node->panel_ids[node->panel_count++] = id;
          }
        }
        match_char(r, ',');
      }
    } else if (strcmp(key, "child_a") == 0) {
      node->child_a = parse_node(r, dock);
      if (node->child_a)
        node->child_a->parent = node;
    } else if (strcmp(key, "child_b") == 0) {
      node->child_b = parse_node(r, dock);
      if (node->child_b)
        node->child_b->parent = node;
    }
  }

  if (fw) {
    if (strcmp(key, "x") == 0)
      parse_float(r, &fw->x);
    else if (strcmp(key, "y") == 0)
      parse_float(r, &fw->y);
    else if (strcmp(key, "w") == 0)
      parse_float(r, &fw->w);
    else if (strcmp(key, "h") == 0)
      parse_float(r, &fw->h);
    else if (strcmp(key, "root") == 0)
      fw->root_node = parse_node(r, dock);
  }

  return true;
}

static StygianDockNode *parse_node(JsonReader *r, StygianDockSpace *dock) {
  skip_ws(r);

  // Check for null
  if (r->p + 4 <= r->end && strncmp(r->p, "null", 4) == 0) {
    r->p += 4;
    return NULL;
  }

  if (!match_char(r, '{'))
    return NULL;

  StygianDockNode *node = alloc_node(dock);
  if (!node)
    return NULL;

  while (!match_char(r, '}')) {
    parse_key_value(r, dock, node, NULL);
    match_char(r, ',');
  }

  return node;
}

bool stygian_dock_load(StygianDockSpace *dock, const char *path) {
  if (!dock || !path)
    return false;

  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("[Stygian Dock] Failed to open %s for reading\n", path);
    return false;
  }

  // Read entire file
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *data = (char *)malloc(size + 1);
  if (!data) {
    fclose(f);
    return false;
  }
  fread(data, 1, size, f);
  data[size] = '\0';
  fclose(f);

  // Reset dock (keep panels, clear tree)
  dock->node_count = 0;
  dock->root = NULL;
  dock->floating_count = 0;

  JsonReader r = {.p = data, .end = data + size};

  if (!match_char(&r, '{')) {
    free(data);
    return false;
  }

  while (!match_char(&r, '}')) {
    char key[64];
    if (!parse_string(&r, key, sizeof(key)))
      break;
    if (!match_char(&r, ':'))
      break;

    skip_ws(&r);

    if (strcmp(key, "version") == 0) {
      int version;
      parse_int(&r, &version);
      if (version != 1) {
        printf("[Stygian Dock] Unsupported version: %d\n", version);
        free(data);
        return false;
      }
    } else if (strcmp(key, "root") == 0) {
      dock->root = parse_node(&r, dock);
    } else if (strcmp(key, "floating") == 0) {
      if (!match_char(&r, '['))
        break;

      while (!match_char(&r, ']')) {
        if (dock->floating_count >= STYGIAN_DOCK_MAX_FLOATING)
          break;

        if (!match_char(&r, '{'))
          break;

        StygianFloatingWindow *fw = &dock->floating[dock->floating_count++];
        memset(fw, 0, sizeof(StygianFloatingWindow));

        while (!match_char(&r, '}')) {
          parse_key_value(&r, dock, NULL, fw);
          match_char(&r, ',');
        }

        match_char(&r, ',');
      }
    }

    match_char(&r, ',');
  }

  free(data);
  dock->layout_dirty = true;
  dock->spatial_dirty = true;

  printf("[Stygian Dock] Loaded layout from %s\n", path);
  return true;
}

// ============================================================================
// Presets
// ============================================================================

void stygian_dock_preset_ide(StygianDockSpace *dock) {
  // Clear existing layout
  dock->node_count = 0;
  dock->floating_count = 0;

  // Create root split: Files(20%) | Editor+Console
  dock->root = alloc_node(dock);
  dock->root->split_type = STYGIAN_DOCK_SPLIT_HORIZONTAL;
  dock->root->split_ratio = 0.2f;

  // Left: Files panel
  dock->root->child_a = alloc_node(dock);
  dock->root->child_a->parent = dock->root;
  dock->root->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;
  // Panels assigned by name mapping

  // Right: Editor(70%) + Console(30%)
  dock->root->child_b = alloc_node(dock);
  dock->root->child_b->parent = dock->root;
  dock->root->child_b->split_type = STYGIAN_DOCK_SPLIT_VERTICAL;
  dock->root->child_b->split_ratio = 0.7f;

  dock->root->child_b->child_a = alloc_node(dock);
  dock->root->child_b->child_a->parent = dock->root->child_b;
  dock->root->child_b->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->root->child_b->child_b = alloc_node(dock);
  dock->root->child_b->child_b->parent = dock->root->child_b;
  dock->root->child_b->child_b->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->layout_dirty = true;
  dock->spatial_dirty = true;
  printf("[Stygian Dock] Applied IDE preset\n");
}

void stygian_dock_preset_3d_editor(StygianDockSpace *dock) {
  // Clear existing layout
  dock->node_count = 0;
  dock->floating_count = 0;

  // Create root split: Hierarchy(20%) | Viewport+Props
  dock->root = alloc_node(dock);
  dock->root->split_type = STYGIAN_DOCK_SPLIT_HORIZONTAL;
  dock->root->split_ratio = 0.2f;

  // Left: Hierarchy
  dock->root->child_a = alloc_node(dock);
  dock->root->child_a->parent = dock->root;
  dock->root->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;

  // Right: Viewport(75%) | Inspector(25%)
  dock->root->child_b = alloc_node(dock);
  dock->root->child_b->parent = dock->root;
  dock->root->child_b->split_type = STYGIAN_DOCK_SPLIT_HORIZONTAL;
  dock->root->child_b->split_ratio = 0.75f;

  dock->root->child_b->child_a = alloc_node(dock);
  dock->root->child_b->child_a->parent = dock->root->child_b;
  dock->root->child_b->child_a->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->root->child_b->child_b = alloc_node(dock);
  dock->root->child_b->child_b->parent = dock->root->child_b;
  dock->root->child_b->child_b->split_type = STYGIAN_DOCK_SPLIT_NONE;

  dock->layout_dirty = true;
  dock->spatial_dirty = true;
  printf("[Stygian Dock] Applied 3D Editor preset\n");
}


# stygian_dock_internal.h
// stygian_dock_internal.h - Internal definitions for Stygian Docking
// Not for public consumption. Changes here do not affect public ABI.

#ifndef STYGIAN_DOCK_INTERNAL_H
#define STYGIAN_DOCK_INTERNAL_H

#include "stygian_dock.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Internal Struct Definitions
// ============================================================================

struct StygianDockPanel {
  char title[64];
  uint32_t id;
  void *user_data;

  bool closable;
  bool visible;
  bool dirty; // Needs re-render

  // DDI render callback
  StygianDockPanelRenderFn render;
};

struct StygianDockNode {
  uint32_t id;
  StygianDockSplit split_type;
  float split_ratio; // 0.0-1.0 for split nodes

  // Tree structure
  struct StygianDockNode *child_a; // Left or Top
  struct StygianDockNode *child_b; // Right or Bottom
  struct StygianDockNode *parent;

  // Tab container data (for leaf nodes)
  uint32_t panel_ids[STYGIAN_DOCK_MAX_TABS_PER_NODE];
  int panel_count;
  int active_panel; // Index into panel_ids

  // Computed layout (set during update)
  float x, y, w, h;

  // Interaction state
  bool splitter_hovered;
  bool splitter_dragging;
  StygianDockDropZone hot_zone;
};

struct StygianFloatingWindow {
  struct StygianWindow *window; // Stygian window (full object for lifecycle)
  struct StygianAPSurface *surface; // Render surface (via stygian_ap)
  void *native_handle;              // Cached native handle (HWND on Windows)

  StygianDockNode *root_node; // Root node of floating content

  float x, y, w, h; // Window position/size
  bool dragging;
  bool resizing;
  bool visible;
  bool prev_mouse_down; // Track mouse state per window

  // Metaball melting state
  float undock_progress; // 0.0 = attached, 1.0 = fully separated
  float blend_radius;    // Current metaball blend (for SDF)
  bool melting;          // In undock/redock transition
};

struct StygianDockSpace {
  // Panel registry
  StygianDockPanel panels[STYGIAN_DOCK_MAX_PANELS];
  int panel_count;
  uint32_t next_panel_id;

  // Node pool
  StygianDockNode nodes[STYGIAN_DOCK_MAX_NODES];
  int node_count;
  uint32_t next_node_id;

  // Root node of docked layout
  StygianDockNode *root;

  // Floating windows
  StygianFloatingWindow floating[STYGIAN_DOCK_MAX_FLOATING];
  int floating_count;

  // Main window context (for sharing with floating)
  void *main_gl_context;
  void *main_device_context;

  // Drag state
  uint32_t dragging_panel_id; // Panel being dragged (0 = none)
  float drag_start_x, drag_start_y;
  float drag_offset_x, drag_offset_y;
  StygianDockNode *drop_target;
  StygianDockDropZone drop_zone;
  bool drag_started;    // Passed drag threshold
  bool prev_mouse_down; // Previous frame mouse state (no static!)

  // Style
  float tab_height;
  float splitter_size;
  float min_panel_size;
  float drop_zone_size; // Size of edge drop zones

  // Metaball effect
  bool metaball_enabled;
  float metaball_blend_speed; // Animation speed

  // ===== O(1) OPTIMIZATIONS =====

  // Dirty flag - skip layout when tree unchanged
  bool layout_dirty;
  float last_layout_w, last_layout_h; // Detect resize

// Spatial hash for O(1) drop target lookup
// Grid of 64x64 pixel cells mapping to leaf nodes
#define STYGIAN_SPATIAL_GRID_CELL 64
#define STYGIAN_SPATIAL_GRID_COLS 32 // 2048 / 64
#define STYGIAN_SPATIAL_GRID_ROWS 16 // 1024 / 64
  StygianDockNode
      *spatial_grid[STYGIAN_SPATIAL_GRID_COLS * STYGIAN_SPATIAL_GRID_ROWS];
  bool spatial_dirty; // Rebuild grid when layout changes

  // Focus tracking
  StygianDockNode *focused_node; // Currently focused node (receives input)

  // ===== DOUBLE-CLICK DETECTION =====
  uint64_t last_click_time_ms; // For double-click detection
  float last_click_x, last_click_y;

  // ===== TAB REORDERING =====
  int reorder_src_idx; // Source tab index during reorder (-1 = not reordering)
  int reorder_dst_idx; // Destination tab index
  StygianDockNode *reorder_node; // Node being reordered

  // ===== GHOST WINDOW (external drag preview) =====
  void *ghost_hwnd;     // WS_POPUP window handle (platform-specific)
  bool ghost_visible;   // Ghost is currently showing
  int ghost_w, ghost_h; // Ghost dimensions

  // ===== FLOATING WINDOW DRAG =====
  bool dragging_from_floating; // Dragging started from a floating window
  int dragging_floating_idx;   // Index of source floating window
};

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_DOCK_INTERNAL_H


# stygian_layout.c
// stygian_layout.c - Layout System Implementation
// Flexbox-style layout engine for automatic positioning

#include "../include/stygian_memory.h"
#include "../src/stygian_internal.h"
#include "stygian_layout_internal.h"
#include <string.h>

// ============================================================================
// Layout API Implementation
// ============================================================================

StygianLayout *stygian_layout_begin(StygianContext *ctx, float x, float y,
                                    float w, float h) {
  if (!ctx || !ctx->frame_arena)
    return NULL;

  // Allocate from per-frame arena (auto-reset at begin_frame)
  StygianLayout *layout = (StygianLayout *)stygian_arena_alloc(
      ctx->frame_arena, sizeof(StygianLayout), _Alignof(StygianLayout));
  if (!layout)
    return NULL;

  // Zero-initialize
  memset(layout, 0, sizeof(StygianLayout));

  layout->x = x;
  layout->y = y;
  layout->w = w;
  layout->h = h;
  layout->dir = STYGIAN_LAYOUT_ROW;
  layout->align = STYGIAN_ALIGN_START;
  layout->justify = STYGIAN_JUSTIFY_START;
  layout->gap = 0.0f;
  layout->padding = 0.0f;

  // Initialize cursor to top-left with padding
  layout->cursor_x = x + layout->padding;
  layout->cursor_y = y + layout->padding;
  layout->item_count = 0;

  return layout;
}

void stygian_layout_dir(StygianLayout *layout, StygianLayoutDir dir) {
  layout->dir = dir;
}

void stygian_layout_align(StygianLayout *layout, StygianLayoutAlign align) {
  layout->align = align;
}

void stygian_layout_justify(StygianLayout *layout,
                            StygianLayoutJustify justify) {
  layout->justify = justify;
}

void stygian_layout_gap(StygianLayout *layout, float gap) { layout->gap = gap; }

void stygian_layout_padding(StygianLayout *layout, float padding) {
  layout->padding = padding;
  // Reset cursor with new padding
  layout->cursor_x = layout->x + padding;
  layout->cursor_y = layout->y + padding;
}

void stygian_layout_next(StygianLayout *layout, float w, float h, float *out_x,
                         float *out_y) {
  // Add gap if not first item
  if (layout->item_count > 0) {
    if (layout->dir == STYGIAN_LAYOUT_ROW) {
      layout->cursor_x += layout->gap;
    } else {
      layout->cursor_y += layout->gap;
    }
  }

  // Calculate position based on alignment
  float item_x = layout->cursor_x;
  float item_y = layout->cursor_y;

  if (layout->dir == STYGIAN_LAYOUT_ROW) {
    // Horizontal layout - align vertically
    float available_h = layout->h - layout->padding * 2.0f;
    switch (layout->align) {
    case STYGIAN_ALIGN_START:
      item_y = layout->cursor_y;
      break;
    case STYGIAN_ALIGN_CENTER:
      item_y = layout->y + layout->padding + (available_h - h) * 0.5f;
      break;
    case STYGIAN_ALIGN_END:
      item_y = layout->y + layout->h - layout->padding - h;
      break;
    case STYGIAN_ALIGN_STRETCH:
      item_y = layout->cursor_y;
      h = available_h; // Stretch to fill
      break;
    }
  } else {
    // Vertical layout - align horizontally
    float available_w = layout->w - layout->padding * 2.0f;
    switch (layout->align) {
    case STYGIAN_ALIGN_START:
      item_x = layout->cursor_x;
      break;
    case STYGIAN_ALIGN_CENTER:
      item_x = layout->x + layout->padding + (available_w - w) * 0.5f;
      break;
    case STYGIAN_ALIGN_END:
      item_x = layout->x + layout->w - layout->padding - w;
      break;
    case STYGIAN_ALIGN_STRETCH:
      item_x = layout->cursor_x;
      w = available_w; // Stretch to fill
      break;
    }
  }

  *out_x = item_x;
  *out_y = item_y;

  // Advance cursor
  if (layout->dir == STYGIAN_LAYOUT_ROW) {
    layout->cursor_x += w;
  } else {
    layout->cursor_y += h;
  }

  layout->item_count++;
}

void stygian_layout_remaining(StygianLayout *layout, float *out_w,
                              float *out_h) {
  if (layout->dir == STYGIAN_LAYOUT_ROW) {
    *out_w = (layout->x + layout->w - layout->padding) - layout->cursor_x;
    *out_h = layout->h - layout->padding * 2.0f;
  } else {
    *out_w = layout->w - layout->padding * 2.0f;
    *out_h = (layout->y + layout->h - layout->padding) - layout->cursor_y;
  }

  // Clamp to 0 if negative
  if (*out_w < 0.0f)
    *out_w = 0.0f;
  if (*out_h < 0.0f)
    *out_h = 0.0f;
}

void stygian_layout_end(StygianContext *ctx, StygianLayout *layout) {
  // Arena allocation - no explicit free needed
  // Memory is reclaimed automatically at stygian_begin_frame() via arena reset
  (void)ctx;
  (void)layout;
}


# stygian_layout.h
// stygian_layout.h - Layout System for Stygian UI Library
// Flexbox-style layout engine for automatic positioning
#ifndef STYGIAN_LAYOUT_H
#define STYGIAN_LAYOUT_H

#include "../include/stygian.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Layout Types
// ============================================================================

typedef enum StygianLayoutDir {
  STYGIAN_LAYOUT_ROW = 0,
  STYGIAN_LAYOUT_COLUMN = 1,
} StygianLayoutDir;

typedef enum StygianLayoutAlign {
  STYGIAN_ALIGN_START = 0,
  STYGIAN_ALIGN_CENTER = 1,
  STYGIAN_ALIGN_END = 2,
  STYGIAN_ALIGN_STRETCH = 3,
} StygianLayoutAlign;

typedef enum StygianLayoutJustify {
  STYGIAN_JUSTIFY_START = 0,
  STYGIAN_JUSTIFY_CENTER = 1,
  STYGIAN_JUSTIFY_END = 2,
  STYGIAN_JUSTIFY_SPACE_BETWEEN = 3,
  STYGIAN_JUSTIFY_SPACE_AROUND = 4,
} StygianLayoutJustify;

// ============================================================================
// Layout Container
// ============================================================================

typedef struct StygianLayout StygianLayout; // Opaque

// ============================================================================
// Layout API
// ============================================================================

// Begin a layout container (allocates new layout)
StygianLayout *stygian_layout_begin(StygianContext *ctx, float x, float y,
                                    float w, float h);

// Set layout properties
void stygian_layout_dir(StygianLayout *layout, StygianLayoutDir dir);
void stygian_layout_align(StygianLayout *layout, StygianLayoutAlign align);
void stygian_layout_justify(StygianLayout *layout,
                            StygianLayoutJustify justify);
void stygian_layout_gap(StygianLayout *layout, float gap);
void stygian_layout_padding(StygianLayout *layout, float padding);

// Get position for next item (consumes space)
void stygian_layout_next(StygianLayout *layout, float w, float h, float *out_x,
                         float *out_y);

// Get remaining space in layout
void stygian_layout_remaining(StygianLayout *layout, float *out_w,
                              float *out_h);

// End layout container (frees layout)
void stygian_layout_end(StygianContext *ctx, StygianLayout *layout);

// ============================================================================
// Convenience Macros
// ============================================================================

// Quick horizontal layout
#define STYGIAN_HBOX(ctx, _name, x, y, w, h)                                   \
  StygianLayout *_name = stygian_layout_begin(ctx, x, y, w, h);                \
  stygian_layout_dir(_name, STYGIAN_LAYOUT_ROW)

// Quick vertical layout
#define STYGIAN_VBOX(ctx, _name, x, y, w, h)                                   \
  StygianLayout *_name = stygian_layout_begin(ctx, x, y, w, h);                \
  stygian_layout_dir(_name, STYGIAN_LAYOUT_COLUMN)

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_LAYOUT_H


# stygian_layout_internal.h
#ifndef STYGIAN_LAYOUT_INTERNAL_H
#define STYGIAN_LAYOUT_INTERNAL_H

#include "stygian_layout.h"

// ============================================================================
// Internal Layout Types
// ============================================================================

struct StygianLayout {
  float x, y, w, h;             // Bounds
  StygianLayoutDir dir;         // Row or column
  StygianLayoutAlign align;     // Cross-axis alignment
  StygianLayoutJustify justify; // Main-axis distribution
  float gap;                    // Gap between children
  float padding;                // Inner padding

  // Internal state
  float cursor_x, cursor_y; // Current placement position
  int item_count;           // Number of children added
};

#endif // STYGIAN_LAYOUT_INTERNAL_H


# stygian_tabs.c
// stygian_tabs.c - Production Tab System Implementation
// Chrome-like draggable, reorderable, closable tabs

#include "stygian_tabs.h"
#include "../include/stygian.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include "stygian_tabs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: Check if point is in rect
static bool point_in_rect(int px, int py, float x, float y, float w, float h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}

// ============================================================================
// Tab Bar Implementation
// ============================================================================

StygianTabBar *stygian_tab_bar_create(float x, float y, float w, float h) {
  StygianTabBar *bar = (StygianTabBar *)calloc(1, sizeof(StygianTabBar));
  if (!bar)
    return NULL;

  bar->x = x;
  bar->y = y;
  bar->w = w;
  bar->h = h;
  bar->dragging_tab = -1;
  bar->hot_tab = -1;
  bar->hot_close_button = -1;
  bar->min_tab_width = 80.0f;
  bar->max_tab_width = 200.0f;
  bar->tab_width = bar->max_tab_width;
  return bar;
}

void stygian_tab_bar_destroy(StygianTabBar *bar) {
  if (bar)
    free(bar);
}

void stygian_tab_bar_set_layout(StygianTabBar *bar, float x, float y, float w,
                                float h) {
  bar->x = x;
  bar->y = y;
  bar->w = w;
  bar->h = h;
}

// Deprecated init removed
// void stygian_tab_bar_init(...)

int stygian_tab_bar_add(StygianTabBar *bar, const char *title, bool closable) {
  if (bar->tab_count >= STYGIAN_MAX_TABS)
    return -1;

  int index = bar->tab_count;
  StygianTabItem *tab = &bar->tabs[index];

  strncpy(tab->title, title, sizeof(tab->title) - 1);
  tab->title[sizeof(tab->title) - 1] = '\0';
  tab->closable = closable;
  tab->pinned = false;
  tab->user_data = NULL;
  tab->logical_index = index;
  tab->visual_index = index;

  bar->tab_count++;

  // Recalculate tab width
  float available_width = bar->w - 10.0f; // Padding
  bar->tab_width = available_width / bar->tab_count;
  if (bar->tab_width < bar->min_tab_width)
    bar->tab_width = bar->min_tab_width;
  if (bar->tab_width > bar->max_tab_width)
    bar->tab_width = bar->max_tab_width;

  return index;
}

void stygian_tab_bar_remove(StygianTabBar *bar, int index) {
  if (index < 0 || index >= bar->tab_count)
    return;

  // Shift tabs down
  for (int i = index; i < bar->tab_count - 1; i++) {
    bar->tabs[i] = bar->tabs[i + 1];
    bar->tabs[i].logical_index = i;
  }

  bar->tab_count--;

  // Adjust active tab
  if (bar->active_tab >= bar->tab_count) {
    bar->active_tab = bar->tab_count - 1;
  }
  if (bar->active_tab < 0)
    bar->active_tab = 0;

  // Recalculate tab width
  if (bar->tab_count > 0) {
    float available_width = bar->w - 10.0f;
    bar->tab_width = available_width / bar->tab_count;
    if (bar->tab_width < bar->min_tab_width)
      bar->tab_width = bar->min_tab_width;
    if (bar->tab_width > bar->max_tab_width)
      bar->tab_width = bar->max_tab_width;
  }
}

int stygian_tab_bar_update(StygianContext *ctx, StygianFont font,
                           StygianTabBar *bar) {
  StygianWindow *win = stygian_get_window(ctx);
  int mx, my;
  stygian_mouse_pos(win, &mx, &my);
  bool mouse_down = stygian_mouse_down(win, STYGIAN_MOUSE_LEFT);
  static bool was_mouse_down = false;

  int result = 0; // 0=none, 1=switched, 2=closed, 3=reordered

  // Background
  stygian_rect(ctx, bar->x, bar->y, bar->w, bar->h, 0.1f, 0.1f, 0.1f, 1.0f);

  // Update hot states
  bar->hot_tab = -1;
  bar->hot_close_button = -1;

  // Calculate tab positions (but don't override dragged tab position)
  float cur_x = bar->x + 5.0f;

  for (int i = 0; i < bar->tab_count; i++) {
    StygianTabItem *tab = &bar->tabs[i];

    // Only update position if not being dragged
    if (bar->dragging_tab != i) {
      tab->x = cur_x;
      tab->y = bar->y + 2.0f;
      tab->w = bar->tab_width - 4.0f;
      tab->h = bar->h - 4.0f;
    }

    cur_x += bar->tab_width;
  }

  // Render tabs (non-dragged first)
  for (int i = 0; i < bar->tab_count; i++) {
    if (bar->dragging_tab == i)
      continue; // Render dragged tab last

    StygianTabItem *tab = &bar->tabs[i];

    bool is_active = (i == bar->active_tab);
    bool is_hovered = point_in_rect(mx, my, tab->x, tab->y, tab->w, tab->h);

    if (is_hovered && bar->dragging_tab < 0)
      bar->hot_tab = i;

    // Tab background
    float r = 0.15f, g = 0.15f, b = 0.15f;
    if (is_active) {
      r = 0.2f;
      g = 0.25f;
      b = 0.35f;
    } else if (is_hovered) {
      r = 0.18f;
      g = 0.18f;
      b = 0.18f;
    }

    stygian_rect_rounded(ctx, tab->x, tab->y, tab->w, tab->h, r, g, b, 1.0f,
                         4.0f);

    // Title
    if (font) {
      float text_x = tab->x + 8.0f;
      float text_y = tab->y + (tab->h - 14.0f) / 2.0f;
      stygian_text(ctx, font, tab->title, text_x, text_y, 14.0f, 0.9f, 0.9f,
                   0.9f, 1.0f);
    }

    // Close button
    if (tab->closable && !tab->pinned) {
      float close_size = 16.0f;
      float close_x = tab->x + tab->w - close_size - 4.0f;
      float close_y = tab->y + (tab->h - close_size) / 2.0f;

      bool close_hovered =
          point_in_rect(mx, my, close_x, close_y, close_size, close_size);
      if (close_hovered && bar->dragging_tab < 0) {
        bar->hot_close_button = i;
      }

      float close_r = close_hovered ? 0.9f : 0.6f;
      float close_g = close_hovered ? 0.3f : 0.3f;
      float close_b = close_hovered ? 0.3f : 0.3f;

      stygian_rect_rounded(ctx, close_x, close_y, close_size, close_size,
                           close_r, close_g, close_b, 1.0f, close_size / 2.0f);

      // X mark
      if (font) {
        stygian_text(ctx, font, "x", close_x + 4, close_y + 1, 12.0f, 1.0f,
                     1.0f, 1.0f, 1.0f);
      }
    }
  }

  // Render dragged tab on top
  if (bar->dragging_tab >= 0 && bar->dragging_tab < bar->tab_count) {
    StygianTabItem *tab = &bar->tabs[bar->dragging_tab];

    // Update dragged tab position to follow mouse
    tab->x = mx - bar->drag_offset_x;
    tab->y = bar->y + 2.0f;

    // Clamp to bar bounds
    if (tab->x < bar->x)
      tab->x = bar->x;
    if (tab->x + tab->w > bar->x + bar->w)
      tab->x = bar->x + bar->w - tab->w;

    // Render with elevation
    stygian_rect_rounded(ctx, tab->x, tab->y - 2, tab->w, tab->h + 2, 0.25f,
                         0.3f, 0.4f, 1.0f, 4.0f);

    if (font) {
      float text_x = tab->x + 8.0f;
      float text_y = tab->y + (tab->h - 14.0f) / 2.0f;
      stygian_text(ctx, font, tab->title, text_x, text_y, 14.0f, 1.0f, 1.0f,
                   1.0f, 1.0f);
    }
  }

  // Interaction logic
  if (mouse_down && !was_mouse_down) {
    // Mouse pressed
    if (bar->hot_close_button >= 0) {
      // Close button clicked
      int close_idx = bar->hot_close_button;
      stygian_tab_bar_remove(bar, close_idx);
      result = 2; // Tab closed
    } else if (bar->hot_tab >= 0) {
      // Tab clicked - switch and potentially start drag
      if (bar->active_tab != bar->hot_tab) {
        bar->active_tab = bar->hot_tab;
        result = 1; // Tab switched
      }
      bar->dragging_tab = bar->hot_tab;
      bar->drag_offset_x = mx - bar->tabs[bar->hot_tab].x;
    }
  } else if (!mouse_down && was_mouse_down) {
    // Mouse released
    if (bar->dragging_tab >= 0) {
      // Calculate drop position - find which gap the tab is closest to
      StygianTabItem *dragged = &bar->tabs[bar->dragging_tab];
      float drag_center_x = dragged->x + dragged->w / 2.0f;

      // Find closest gap between tabs
      int target_index = 0;
      float min_dist = 999999.0f;

      // Check gap before each tab
      for (int i = 0; i <= bar->tab_count; i++) {
        float gap_x;
        if (i == 0) {
          gap_x = bar->x + 5.0f; // Before first tab
        } else if (i == bar->tab_count) {
          gap_x =
              bar->x + 5.0f + bar->tab_width * bar->tab_count; // After last tab
        } else {
          gap_x = bar->x + 5.0f + bar->tab_width * i; // Between tabs
        }

        float dist = drag_center_x - gap_x;
        if (dist < 0)
          dist = -dist; // abs

        if (dist < min_dist) {
          min_dist = dist;
          target_index = i;
        }
      }

      // Clamp and adjust for removal of dragged tab
      if (target_index > bar->tab_count)
        target_index = bar->tab_count;
      if (target_index > bar->dragging_tab)
        target_index--; // Account for dragged tab removal

      // Perform reordering if position changed
      if (target_index != bar->dragging_tab) {

        StygianTabItem temp = bar->tabs[bar->dragging_tab];

        // Shift tabs
        if (target_index < bar->dragging_tab) {
          // Moving left
          for (int i = bar->dragging_tab; i > target_index; i--) {
            bar->tabs[i] = bar->tabs[i - 1];
            bar->tabs[i].logical_index = i;
          }
        } else {
          // Moving right
          for (int i = bar->dragging_tab; i < target_index; i++) {
            bar->tabs[i] = bar->tabs[i + 1];
            bar->tabs[i].logical_index = i;
          }
        }

        bar->tabs[target_index] = temp;
        bar->tabs[target_index].logical_index = target_index;

        // Update active tab index if it moved
        if (bar->active_tab == bar->dragging_tab) {
          bar->active_tab = target_index;
        } else if (bar->active_tab >= target_index &&
                   bar->active_tab < bar->dragging_tab) {
          bar->active_tab++;
        } else if (bar->active_tab <= target_index &&
                   bar->active_tab > bar->dragging_tab) {
          bar->active_tab--;
        }

        result = 3; // Tab reordered
      }

      // End drag
      bar->dragging_tab = -1;
    }
  }

  was_mouse_down = mouse_down;

  return result;
}

void *stygian_tab_bar_get_active_data(StygianTabBar *bar) {
  if (bar->active_tab < 0 || bar->active_tab >= bar->tab_count)
    return NULL;
  return bar->tabs[bar->active_tab].user_data;
}

int stygian_tab_bar_get_active_index(StygianTabBar *bar) {
  return bar->active_tab;
}

int stygian_tab_bar_get_count(StygianTabBar *bar) { return bar->tab_count; }

const char *stygian_tab_bar_get_title(StygianTabBar *bar, int index) {
  if (index < 0 || index >= bar->tab_count)
    return NULL;
  return bar->tabs[index].title;
}

// ============================================================================
// Multiviewport System Implementation
// ============================================================================

StygianMultiViewport *stygian_multiviewport_create(void) {
  StygianMultiViewport *mv =
      (StygianMultiViewport *)calloc(1, sizeof(StygianMultiViewport));
  if (!mv)
    return NULL;

  mv->layout_mode = STYGIAN_VIEWPORT_LAYOUT_SINGLE;
  mv->split_ratio_h = 0.5f;
  mv->split_ratio_v = 0.5f;
  return mv;
}

void stygian_multiviewport_destroy(StygianMultiViewport *mv) {
  if (mv)
    free(mv);
}

int stygian_multiviewport_add(StygianMultiViewport *mv, const char *name,
                              StygianViewportType type) {
  if (mv->viewport_count >= STYGIAN_MAX_VIEWPORTS)
    return -1;

  int index = mv->viewport_count;
  StygianViewport *vp = &mv->viewports[index];

  strncpy(vp->name, name, sizeof(vp->name) - 1);
  vp->name[sizeof(vp->name) - 1] = '\0';
  vp->type = type;
  vp->active = (index == 0);
  vp->show_grid = true;
  vp->show_gizmo = true;
  vp->framebuffer_texture = 0;
  vp->user_data = NULL;

  mv->viewport_count++;
  return index;
}

void stygian_multiviewport_set_layout(StygianMultiViewport *mv,
                                      int layout_mode) {
  mv->layout_mode = layout_mode;
}

void stygian_multiviewport_render(StygianContext *ctx, StygianFont font,
                                  StygianMultiViewport *mv) {
  // TODO: Implement layout-specific rendering
  // For now, just render active viewport
  if (mv->active_viewport < 0 || mv->active_viewport >= mv->viewport_count)
    return;

  StygianViewport *vp = &mv->viewports[mv->active_viewport];

  // Render viewport frame
  stygian_rect(ctx, vp->x, vp->y, vp->w, vp->h, 0.05f, 0.05f, 0.05f, 1.0f);

  // Render framebuffer texture if available
  if (vp->framebuffer_texture != 0) {
    stygian_image(ctx, vp->framebuffer_texture, vp->x + 2, vp->y + 2, vp->w - 4,
                  vp->h - 4);
  }

  // Render viewport name
  if (font) {
    stygian_text(ctx, font, vp->name, vp->x + 10, vp->y + 10, 14.0f, 0.7f, 0.7f,
                 0.7f, 1.0f);
  }
}

int stygian_multiviewport_hit_test(StygianMultiViewport *mv, int mouse_x,
                                   int mouse_y) {
  for (int i = 0; i < mv->viewport_count; i++) {
    StygianViewport *vp = &mv->viewports[i];
    if (point_in_rect(mouse_x, mouse_y, vp->x, vp->y, vp->w, vp->h)) {
      return i;
    }
  }
  return -1;
}


# stygian_tabs.h
// stygian_tabs.h - Production Tab System with Chrome-like behavior
// Part of Layout System

#ifndef STYGIAN_TABS_H
#define STYGIAN_TABS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct StygianContext StygianContext;
typedef uint32_t StygianFont; // Use definition from stygian.h

// ============================================================================
// Tab System - Chrome-like draggable, reorderable, closable tabs
// ============================================================================

// ============================================================================
// Tab System - Structs
// ============================================================================

#define STYGIAN_MAX_TABS 64

typedef struct StygianTabItem StygianTabItem; // Opaque
typedef struct StygianTabBar StygianTabBar;   // Opaque

// ============================================================================
// Tab System - API
// ============================================================================

// Create a new tab bar
StygianTabBar *stygian_tab_bar_create(float x, float y, float w, float h);

// Destroy tab bar
void stygian_tab_bar_destroy(StygianTabBar *bar);

// Set layout bounds
void stygian_tab_bar_set_layout(StygianTabBar *bar, float x, float y, float w,
                                float h);

// Set layout bounds
void stygian_tab_bar_set_layout(StygianTabBar *bar, float x, float y, float w,
                                float h);

// Add tab (returns index, or -1 if full)
int stygian_tab_bar_add(StygianTabBar *bar, const char *title, bool closable);

// Remove tab by index
void stygian_tab_bar_remove(StygianTabBar *bar, int index);

// Render and handle interaction
// Returns: 0 = no action, 1 = tab switched, 2 = tab closed, 3 = tab reordered
int stygian_tab_bar_update(StygianContext *ctx, StygianFont font,
                           StygianTabBar *bar);

// Get active tab user data
void *stygian_tab_bar_get_active_data(StygianTabBar *bar);

// Get active tab index
int stygian_tab_bar_get_active_index(StygianTabBar *bar);

// Get tab count
int stygian_tab_bar_get_count(StygianTabBar *bar);

// Get tab title
const char *stygian_tab_bar_get_title(StygianTabBar *bar, int index);

// ============================================================================
// Multiviewport System
// ============================================================================

typedef enum StygianViewportType {
  STYGIAN_VIEWPORT_3D,
  STYGIAN_VIEWPORT_2D,
  STYGIAN_VIEWPORT_TEXT,
  STYGIAN_VIEWPORT_CUSTOM
} StygianViewportType;

#define STYGIAN_MAX_VIEWPORTS 16

typedef struct StygianViewport StygianViewport;           // Opaque
typedef struct StygianMultiViewport StygianMultiViewport; // Opaque

// Create multiviewport system
StygianMultiViewport *stygian_multiviewport_create(void);

// Destroy multiviewport system
void stygian_multiviewport_destroy(StygianMultiViewport *mv);

// Add viewport (returns index, or -1 if full)
int stygian_multiviewport_add(StygianMultiViewport *mv, const char *name,
                              StygianViewportType type);

// Set layout mode
void stygian_multiviewport_set_layout(StygianMultiViewport *mv,
                                      int layout_mode);

// Render viewports
void stygian_multiviewport_render(StygianContext *ctx, StygianFont font,
                                  StygianMultiViewport *mv);

// Get viewport under mouse (returns index or -1)
int stygian_multiviewport_hit_test(StygianMultiViewport *mv, int mouse_x,
                                   int mouse_y);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_TABS_H


# stygian_tabs_internal.h
// stygian_tabs_internal.h - Internal definitions for Stygian Tabs
// Not for public consumption. Changes here do not affect public ABI.

#ifndef STYGIAN_TABS_INTERNAL_H
#define STYGIAN_TABS_INTERNAL_H

#include "stygian_tabs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Internal Struct Definitions
// ============================================================================

struct StygianTabItem {
  char title[64];
  bool closable;
  bool pinned; // Pinned tabs can't be closed or reordered
  void *user_data;

  // Internal state (managed by tab bar)
  float x, y, w, h;  // Current position/size (for animation)
  float target_x;    // Target position (for smooth animation)
  int visual_index;  // Visual position (during drag)
  int logical_index; // Logical position in array
};

struct StygianTabBar {
  float x, y, w, h;

  struct StygianTabItem tabs[STYGIAN_MAX_TABS];
  int tab_count;
  int active_tab; // Logical index of active tab

  // Drag state
  int dragging_tab;      // Index of tab being dragged (-1 if none)
  float drag_offset_x;   // Mouse offset from tab origin
  int drag_target_index; // Target index for drop

  // Visual state
  float tab_width;     // Current tab width (dynamic based on count)
  float min_tab_width; // Minimum tab width
  float max_tab_width; // Maximum tab width

  // Interaction state
  int hot_tab;          // Tab under mouse
  int hot_close_button; // Close button under mouse
};

// ============================================================================
// MultiViewport Internal
// ============================================================================

struct StygianViewport {
  char name[32];
  StygianViewportType type;
  float x, y, w, h;
  uint32_t framebuffer_texture; // External texture from rendering backend
  bool active;
  bool show_grid;
  bool show_gizmo;
  void *user_data;
};

struct StygianMultiViewport {
  struct StygianViewport viewports[STYGIAN_MAX_VIEWPORTS];
  int viewport_count;
  int active_viewport;

  // Layout mode
  enum {
    STYGIAN_VIEWPORT_LAYOUT_SINGLE,
    STYGIAN_VIEWPORT_LAYOUT_SPLIT_H,
    STYGIAN_VIEWPORT_LAYOUT_SPLIT_V,
    STYGIAN_VIEWPORT_LAYOUT_QUAD,
    STYGIAN_VIEWPORT_LAYOUT_CUSTOM
  } layout_mode;

  // Split ratios (for split modes)
  float split_ratio_h;
  float split_ratio_v;
};

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_TABS_INTERNAL_H