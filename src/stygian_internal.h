// stygian_internal.h - Internal types for Stygian
// NOT part of public API
#ifndef STYGIAN_INTERNAL_H
#define STYGIAN_INTERNAL_H

#include "../include/stygian.h"
#include "../include/stygian_cmd.h"
#include "../include/stygian_memory.h"
#include <string.h>

// ============================================================================
// Debug Assert — independent from NDEBUG so Stygian can control its own traps
// ============================================================================
#ifndef STYGIAN_ASSERT
#if !defined(NDEBUG) || defined(STYGIAN_ENABLE_ASSERTS)
#ifdef _MSC_VER
#define STYGIAN_ASSERT(expr)                                                   \
  do {                                                                         \
    if (!(expr)) {                                                             \
      __debugbreak();                                                          \
    }                                                                          \
  } while (0)
#else
#include <assert.h>
#define STYGIAN_ASSERT(expr) assert(expr)
#endif
#else
#define STYGIAN_ASSERT(expr) ((void)0)
#endif
#endif

// ============================================================================
// Safe String Copy — DoD-safe, internal only
// ============================================================================

/**
 * stygian_cpystr — Bounded copy of a trusted NUL-terminated C string.
 *
 * Copies up to (dstsz - 1) bytes from src into dst and guarantees
 * null-termination. No data-dependent branching in the copy loop
 * itself (memcpy). No CRT heap. Cache-friendly linear write.
 *
 * Contract: src must be a valid, NUL-terminated string or NULL.
 * strlen(src) is unbounded by design — callers are responsible for
 * ensuring src is trusted (config paths, shader dirs, internal
 * strings). Do NOT use for untrusted user input or binary blobs.
 *
 * @param dst   Destination buffer (must not be NULL unless dstsz == 0).
 * @param dstsz Total byte capacity of dst, including the null terminator.
 * @param src   Source string (NULL treated as empty string).
 */
static inline void stygian_cpystr(char *dst, size_t dstsz, const char *src) {
  STYGIAN_ASSERT(dstsz > 0);
  if (!dst || dstsz == 0)
    return;

  if (!src) {
    dst[0] = '\0';
    return;
  }

  size_t len = strlen(src);
  if (len >= dstsz)
    len = dstsz - 1;

  memcpy(dst, src, len);
  dst[len] = '\0';
}

// Forward declarations for opaque types
typedef struct StygianWindow StygianWindow;
typedef struct StygianAP StygianAP;
typedef struct StygianTriadRuntime StygianTriadRuntime;

// ============================================================================
// Configuration Constants
// ============================================================================

#define STYGIAN_INLINE_EMOJI_CACHE_SIZE 512

// ============================================================================
// Element Flags
// ============================================================================

#define STYGIAN_FLAG_VISIBLE (1 << 0)
#define STYGIAN_FLAG_ALLOCATED (1 << 1)
#define STYGIAN_FLAG_TRANSIENT (1 << 2)
#define STYGIAN_CLIP_MASK 0xFF00
#define STYGIAN_CLIP_SHIFT 8

// ============================================================================
// Clip Region
// ============================================================================

typedef struct {
  float x, y, w, h;
} StygianClipRect;

typedef struct {
  bool used;
  uint64_t glyph_hash;
  uint32_t texture_id;
  uint16_t width;
  uint16_t height;
  uint32_t last_used;
} StygianInlineEmojiCacheEntry;

typedef struct {
  bool has_glyph;
  float advance;
  float plane_left, plane_bottom, plane_right, plane_top;
  float u0, v0, u1, v1;
} StygianFontGlyph;

typedef struct {
  uint32_t codepoint;
  StygianFontGlyph glyph;
} StygianFontGlyphEntry;

typedef struct {
  uint32_t left;
  uint32_t right;
  float advance;
} StygianFontKernPair;

typedef struct {
  uint32_t requested_hz_max;
  uint64_t deferred_due_ms;
  uint64_t due_ms;
  uint64_t last_frame_ms;
  bool has_pending;
  uint32_t reason_flags;
  uint32_t last_reason_flags;
  char source[32];
  char last_source[32];
} StygianRepaintState;

typedef struct {
  StygianScopeId id;
  bool dirty;
  bool dirty_next;
  uint32_t generation;
  uint32_t range_start;
  uint32_t range_count;
  uint8_t clip_snapshot;
  float z_snapshot;
  uint32_t last_dirty_reason;
  uint32_t last_source_tag;
  uint32_t last_frame_index;
} StygianScopeCacheEntry;

#define STYGIAN_SCOPE_CACHE_CAPACITY 1024
#define STYGIAN_CMD_MAX_PRODUCERS 16
#define STYGIAN_CMD_QUEUE_CAPACITY 4096
#define STYGIAN_ERROR_RING_CAPACITY 256
#define STYGIAN_WINNER_RING_CAPACITY 512

typedef struct StygianCmdRecord {
  uint64_t scope_id;
  uint64_t submit_seq;
  uint32_t source_tag;
  uint32_t cmd_index;
  uint32_t element_id;
  uint16_t property_id;
  uint8_t op_priority;
  uint8_t _pad0;
  union {
    struct {
      float x, y, w, h;
    } bounds;
    struct {
      float r, g, b, a;
    } color;
    struct {
      float tl, tr, br, bl;
    } radius;
    struct {
      uint32_t type;
    } type;
    struct {
      uint32_t visible;
    } visible;
    struct {
      float z;
    } depth;
    struct {
      uint32_t texture;
      float u0, v0, u1, v1;
    } texture;
    struct {
      float offset_x, offset_y, blur, spread;
      float r, g, b, a;
    } shadow;
    struct {
      float angle;
      float r1, g1, b1, a1;
      float r2, g2, b2, a2;
    } gradient;
    struct {
      float value;
    } scalar;
  } payload;
} StygianCmdRecord;

typedef struct StygianCmdQueueEpoch {
  StygianCmdRecord *records;
  uint32_t count;
  uint32_t dropped;
} StygianCmdQueueEpoch;

typedef struct StygianCmdProducerQueue {
  uint32_t owner_thread_id;
  uint32_t registered_order;
  StygianCmdQueueEpoch epoch[2];
} StygianCmdProducerQueue;

struct StygianCmdBuffer {
  struct StygianContext *ctx;
  uint32_t queue_index;
  uint32_t epoch;
  uint32_t source_tag;
  uint64_t scope_id;
  uint32_t begin_index;
  uint32_t count;
  bool active;
};

typedef struct StygianWinnerRecord {
  uint64_t scope_id;
  uint64_t winner_submit_seq;
  uint32_t frame_index;
  uint32_t element_id;
  uint16_t property_id;
  uint16_t _pad0;
  uint32_t winner_source_tag;
  uint32_t winner_cmd_index;
} StygianWinnerRecord;

// ============================================================================
// Font Atlas
// ============================================================================

typedef struct StygianFontAtlas {
  uint32_t texture_id;
  int atlas_width, atlas_height;
  float px_range;
  float em_size;
  float line_height;
  float ascender, descender;

  StygianFontGlyph glyphs[256];
  StygianFontGlyphEntry *glyph_entries;
  uint32_t glyph_count;
  uint32_t glyph_capacity;
  int32_t *glyph_hash;
  uint32_t glyph_hash_capacity;

  float kerning_table[256][256];
  bool kerning_has[256][256];
  bool kerning_ready;
  StygianFontKernPair *kerning_pairs;
  uint32_t kerning_pair_count;
} StygianFontAtlas;

// ============================================================================
// Backend Interface (DEPRECATED - use stygian_ap.h instead)
// ============================================================================

// NOTE: The old StygianBackend interface is deprecated.
// All GPU operations now go through StygianAP (see backends/stygian_ap.h)

// ============================================================================
// SoA Element Storage (Hot/Cold Split for 3 SSBOs)
// ============================================================================

// SSBO 0: Hot — always read by every fragment
typedef struct StygianSoAHot {
  float x, y, w, h;    // 16 - bounds
  float color[4];      // 16 - primary RGBA
  uint32_t texture_id; //  4
  uint32_t type;       //  4 - element type | (render_mode << 16)
  uint32_t flags;      //  4 - visible(1), clip_id(8), etc.
  float z;             //  4
} StygianSoAHot;       // 48 bytes

// SSBO 1: Appearance — read only by bordered/rounded/textured elements
typedef struct StygianSoAAppearance {
  float border_color[4];   // 16
  float radius[4];         // 16 - corners (tl,tr,br,bl)
  float uv[4];             // 16 - tex coords (u0,v0,u1,v1)
  float control_points[4]; // 16 - bezier/wire/metaball control data
} StygianSoAAppearance;    // 64 bytes

// SSBO 2: Effects — read only by shadowed/gradient/animated elements
typedef struct StygianSoAEffects {
  float shadow_offset[2];  //  8
  float shadow_blur;       //  4
  float shadow_spread;     //  4
  float shadow_color[4];   // 16
  float gradient_start[4]; // 16
  float gradient_end[4];   // 16
  float hover;             //  4
  float blend;             //  4
  float gradient_angle;    //  4
  float blur_radius;       //  4
  float glow_intensity;    //  4
  uint32_t parent_id;      //  4
  float _pad[2];           //  8
} StygianSoAEffects;       // 96 bytes

// Render mode bits (packed into upper 16 bits of hot.type)
#define STYGIAN_MODE_APPEARANCE (1u << 16)
#define STYGIAN_MODE_EFFECTS (1u << 17)
#define STYGIAN_MODE_TEXT (1u << 18)
#define STYGIAN_TYPE_MASK 0xFFFFu

// Compile-time layout guards — catch padding surprises
_Static_assert(sizeof(StygianSoAHot) == 48,
               "StygianSoAHot must be 48 bytes (3 × vec4)");
_Static_assert(sizeof(StygianSoAAppearance) == 64,
               "StygianSoAAppearance must be 64 bytes (4 × vec4)");
_Static_assert(sizeof(StygianSoAEffects) == 96,
               "StygianSoAEffects must be 96 bytes (6 × vec4)");

// SoA container
typedef struct StygianSoA {
  StygianSoAHot *hot;
  StygianSoAAppearance *appearance;
  StygianSoAEffects *effects;
  uint32_t element_count;
  uint32_t capacity;
} StygianSoA;

// ============================================================================
// Versioned Chunk Tracking (per-buffer dirty ranges)
// ============================================================================

#ifndef STYGIAN_DEFAULT_CHUNK_SIZE
#define STYGIAN_DEFAULT_CHUNK_SIZE 256
#endif

typedef struct StygianBufferChunk {
  // CPU-side versions — incremented on any write to that buffer in this chunk
  uint32_t hot_version;
  uint32_t appearance_version;
  uint32_t effects_version;
  // Per-buffer dirty element range within this chunk (relative to chunk start)
  uint32_t hot_dirty_min, hot_dirty_max;
  uint32_t appearance_dirty_min, appearance_dirty_max;
  uint32_t effects_dirty_min, effects_dirty_max;
} StygianBufferChunk;

// ============================================================================
// Context Structure
// ============================================================================

struct StygianContext {
  StygianConfig config;
  uint32_t glyph_feature_flags;
  StygianAllocator *allocator;
  StygianArena *frame_arena; // Per-frame scratch allocator, reset each frame

  // Window and graphics access point (opaque, owned externally)
  StygianWindow *window;
  StygianAP *ap;

  int width, height;

  // Element pool (AoS removed, now purely SoA)

  uint32_t element_count;

  // ID Reuse
  uint32_t *free_list;
  uint32_t free_count;

  // SoA element storage (Hot/Cold split — 3 SSBOs)
  StygianSoA soa;
  StygianBufferChunk *chunks;
  uint32_t chunk_count;
  uint32_t
      chunk_size; // Elements per chunk (default STYGIAN_DEFAULT_CHUNK_SIZE)

  // Transient pool
  uint32_t transient_start;
  uint32_t transient_count;

  // Clip stack
  StygianClipRect *clips;
  uint16_t clip_count;
  uint8_t clip_stack[32];
  uint8_t clip_stack_top;

  // Fonts
  StygianFontAtlas *fonts;
  uint32_t font_count;

  StygianInlineEmojiCacheEntry
      inline_emoji_cache[STYGIAN_INLINE_EMOJI_CACHE_SIZE];
  uint32_t inline_emoji_clock;

  StygianTriadRuntime *triad_runtime;
  StygianColorProfile output_color_profile;
  StygianColorProfile glyph_source_color_profile;
  bool glyph_color_transform_enabled;

  // Render layers (optional multi-pass draws)
  uint32_t layer_start;
  uint16_t layer_count;
  bool layer_active;
  struct {
    uint32_t start;
    uint32_t count;
  } layers[32];

  // Frame stats
  uint32_t frame_draw_calls;
  uint32_t last_frame_draw_calls;
  uint32_t last_frame_element_count;
  uint32_t last_frame_clip_count;
  uint32_t last_frame_upload_bytes;
  uint32_t last_frame_upload_ranges;
  uint32_t frame_scope_replay_hits;
  uint32_t frame_scope_replay_misses;
  uint32_t frame_scope_forced_rebuilds;
  uint32_t last_frame_scope_replay_hits;
  uint32_t last_frame_scope_replay_misses;
  uint32_t last_frame_scope_forced_rebuilds;
  float last_frame_build_ms;
  float last_frame_submit_ms;
  float last_frame_present_ms;
  float last_frame_gpu_ms;
  uint32_t last_frame_reason_flags;
  uint32_t last_frame_eval_only;
  uint32_t frame_index;
  uint64_t frame_begin_cpu_ms;
  bool skip_frame;         // DDI: True if all scopes clean, skip submit/swap
  bool eval_only_frame;    // Evaluate widgets/state but skip GPU submit/swap
  StygianFrameIntent frame_intent;
  uint32_t frames_skipped; // DDI: Counter for no-op frames

  // Cumulative stats (reset each log interval)
  uint32_t stats_frames_rendered;
  uint32_t stats_frames_skipped;
  uint32_t stats_frames_eval_only;
  uint64_t stats_total_upload_bytes;
  uint32_t stats_scope_replay_hits;
  uint32_t stats_scope_replay_misses;
  uint32_t stats_scope_forced_rebuilds;
  float stats_total_build_ms;
  float stats_total_submit_ms;
  float stats_total_present_ms;
  uint32_t stats_reason_mutation;
  uint32_t stats_reason_timer;
  uint32_t stats_reason_async;
  uint32_t stats_reason_forced;
  uint32_t stats_log_interval_ms; // 0 = disabled, default 10000
  uint64_t stats_last_log_ms;

  // Repaint scheduler
  StygianRepaintState repaint;

  // Optional retained scopes
  StygianScopeCacheEntry scope_cache[STYGIAN_SCOPE_CACHE_CAPACITY];
  uint32_t scope_count;
  uint32_t active_scope_stack[32];
  uint8_t active_scope_stack_top;
  int32_t active_scope_index;
  bool next_scope_dirty;
  bool scope_replay_active;
  uint32_t scope_replay_cursor;
  uint32_t scope_replay_end;
  bool suppress_element_writes;

  StygianCmdProducerQueue cmd_queues[STYGIAN_CMD_MAX_PRODUCERS];
  StygianCmdBuffer cmd_buffers[STYGIAN_CMD_MAX_PRODUCERS];
  uint32_t cmd_queue_count;
  uint32_t cmd_publish_epoch;
  uint64_t cmd_submit_seq_next;
  bool cmd_committing;
  uint32_t last_commit_applied;
  uint32_t total_command_drops;
  StygianCmdRecord *cmd_merge_records;
  uint32_t cmd_merge_capacity;

  StygianWinnerRecord winner_ring[STYGIAN_WINNER_RING_CAPACITY];
  uint32_t winner_ring_head;

  StygianContextErrorCallback error_callback;
  void *error_callback_user_data;
  StygianContextErrorRecord error_ring[STYGIAN_ERROR_RING_CAPACITY];
  uint32_t error_ring_head;
  uint32_t error_ring_count;
  uint32_t error_ring_dropped;

  // NOTE: GPU resources (SSBO, VAO, VBO, program) are now owned by StygianAP
  // The old StygianBackend interface is deprecated

  bool initialized;
};

// ============================================================================
// Internal Helpers
// ============================================================================

// ============================================================================
// SoA Dirty Marking Helpers (per-buffer chunk tracking)
// ============================================================================

static inline void stygian_mark_soa_hot_dirty(StygianContext *ctx,
                                              uint32_t id) {
  uint32_t ci = id / ctx->chunk_size;
  uint32_t local = id % ctx->chunk_size;
  StygianBufferChunk *c = &ctx->chunks[ci];
  c->hot_version++;
  if (local < c->hot_dirty_min)
    c->hot_dirty_min = local;
  if (local > c->hot_dirty_max)
    c->hot_dirty_max = local;
}

static inline void stygian_mark_soa_appearance_dirty(StygianContext *ctx,
                                                     uint32_t id) {
  uint32_t ci = id / ctx->chunk_size;
  uint32_t local = id % ctx->chunk_size;
  StygianBufferChunk *c = &ctx->chunks[ci];
  c->appearance_version++;
  if (local < c->appearance_dirty_min)
    c->appearance_dirty_min = local;
  if (local > c->appearance_dirty_max)
    c->appearance_dirty_max = local;
}

static inline void stygian_mark_soa_effects_dirty(StygianContext *ctx,
                                                  uint32_t id) {
  uint32_t ci = id / ctx->chunk_size;
  uint32_t local = id % ctx->chunk_size;
  StygianBufferChunk *c = &ctx->chunks[ci];
  c->effects_version++;
  if (local < c->effects_dirty_min)
    c->effects_dirty_min = local;
  if (local > c->effects_dirty_max)
    c->effects_dirty_max = local;
}

#endif // STYGIAN_INTERNAL_H
