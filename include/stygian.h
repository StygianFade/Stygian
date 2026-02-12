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

typedef struct StygianContextErrorRecord {
  uint64_t timestamp_ms;
  uint32_t frame_index;
  uint32_t thread_id;
  uint32_t code;
  uint64_t scope_id;
  uint32_t source_tag;
  uint32_t message_hash;
  char message[96];
} StygianContextErrorRecord;

typedef void (*StygianContextErrorCallback)(StygianContext *ctx, uint32_t code,
                                            const char *message,
                                            void *user_data);

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
uint32_t stygian_get_last_commit_applied(const StygianContext *ctx);
uint32_t stygian_get_total_command_drops(const StygianContext *ctx);

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
bool stygian_scope_get_last_dirty_info(const StygianContext *ctx,
                                       StygianScopeId id, uint32_t *out_reason,
                                       uint32_t *out_source_tag,
                                       uint32_t *out_frame_index);

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

void stygian_context_set_error_callback(StygianContext *ctx,
                                        StygianContextErrorCallback callback,
                                        void *user_data);
void stygian_set_default_context_error_callback(
    StygianContextErrorCallback callback, void *user_data);
uint32_t stygian_context_get_recent_errors(const StygianContext *ctx,
                                           StygianContextErrorRecord *out,
                                           uint32_t max_count);
uint32_t stygian_context_get_error_drop_count(const StygianContext *ctx);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_H
