// stygian.c - Core implementation
// MIT License
#include "../backends/stygian_ap.h"
#include "../include/stygian_memory.h"
#include "../include/stygian_error.h"
#include "../window/stygian_window.h"
#include "stygian_color.h"
#include "stygian_icc.h"
#include "stygian_internal.h"
#include "stygian_mtsdf.h"
#include "stygian_triad.h"
#include "stygian_unicode.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "stygian_platform.h"

#include <assert.h>
#ifdef _WIN32
#include <windows.h>
#endif

// Debug-only: trap CRT heap usage during frame processing
#ifndef NDEBUG
int g_stygian_debug_in_frame = 0;
#endif

static StygianContextErrorCallback g_default_context_error_callback = NULL;
static void *g_default_context_error_callback_user_data = NULL;

static uint32_t stygian_profile_to_flags(StygianGlyphProfile profile) {
  switch (profile) {
  case STYGIAN_GLYPH_PROFILE_DGPU_INTERACTIVE:
    return STYGIAN_GLYPH_FEATURE_DGPU_INTERACTIVE;
  case STYGIAN_GLYPH_PROFILE_IGPU_BG_DECODE:
    return STYGIAN_GLYPH_FEATURE_IGPU_BG_DECODE;
  case STYGIAN_GLYPH_PROFILE_DEFAULT:
  default:
    return STYGIAN_GLYPH_FEATURE_DEFAULT;
  }
}

static void *stygian_system_alloc(StygianAllocator *allocator, size_t size,
                                  size_t alignment) {
  (void)allocator;
  (void)alignment;
  if (size == 0u)
    return NULL;
#ifndef NDEBUG
  // Debug trap: CRT heap hit during frame processing
  extern int g_stygian_debug_in_frame;
  if (g_stygian_debug_in_frame) {
    fprintf(stderr, "[STYGIAN] ASSERT_NO_CRT_HEAP: malloc(%zu) during frame!\n",
            size);
    assert(!"CRT heap allocation during frame processing");
  }
#endif
  return malloc(size);
}

static void stygian_system_free(StygianAllocator *allocator, void *ptr) {
  (void)allocator;
  free(ptr);
}

static void stygian_system_reset(StygianAllocator *allocator) {
  (void)allocator;
}

static StygianAllocator g_stygian_system_allocator = {
    .alloc = stygian_system_alloc,
    .free = stygian_system_free,
    .reset = stygian_system_reset,
    .user_data = NULL,
};

static StygianAllocator *
stygian_resolve_allocator(const StygianConfig *config) {
  if (config && config->persistent_allocator &&
      config->persistent_allocator->alloc) {
    return config->persistent_allocator;
  }
  return &g_stygian_system_allocator;
}

static int stygian_size_mul(size_t a, size_t b, size_t *out) {
  if (!out)
    return 0;
  if (a == 0u || b == 0u) {
    *out = 0u;
    return 1;
  }
  if (a > (SIZE_MAX / b))
    return 0;
  *out = a * b;
  return 1;
}

static void *stygian_alloc_raw(StygianAllocator *allocator, size_t size,
                               size_t alignment, bool zero_init) {
  void *ptr;
  if (!allocator || !allocator->alloc || size == 0u)
    return NULL;
  ptr = allocator->alloc(allocator, size, alignment);
  if (ptr && zero_init) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static void *stygian_alloc_array(StygianAllocator *allocator, size_t count,
                                 size_t elem_size, size_t alignment,
                                 bool zero_init) {
  size_t total = 0u;
  if (!stygian_size_mul(count, elem_size, &total))
    return NULL;
  if (total == 0u)
    return NULL;
  return stygian_alloc_raw(allocator, total, alignment, zero_init);
}

static void stygian_free_raw(StygianAllocator *allocator, void *ptr) {
  if (!ptr)
    return;
  if (allocator && allocator->free) {
    allocator->free(allocator, ptr);
  }
}

static uint64_t stygian_now_ms(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}

static int32_t stygian_scope_find_index(const StygianContext *ctx,
                                        StygianScopeId id) {
  uint32_t i;
  if (!ctx || id == 0u)
    return -1;
  for (i = 0; i < ctx->scope_count; i++) {
    if (ctx->scope_cache[i].id == id)
      return (int32_t)i;
  }
  return -1;
}

static uint32_t stygian_repaint_reason_from_source(const char *source) {
  if (!source || source[0] == '\0')
    return STYGIAN_REPAINT_REASON_NONE;
  if (strncmp(source, "decode", 6) == 0 || strncmp(source, "async", 5) == 0)
    return STYGIAN_REPAINT_REASON_ASYNC;
  if (strncmp(source, "anim", 4) == 0)
    return STYGIAN_REPAINT_REASON_ANIMATION;
  if (strncmp(source, "timer", 5) == 0 || strncmp(source, "diag", 4) == 0 ||
      strncmp(source, "deferred", 8) == 0)
    return STYGIAN_REPAINT_REASON_TIMER;
  if (strncmp(source, "forced", 6) == 0)
    return STYGIAN_REPAINT_REASON_FORCED;
  return STYGIAN_REPAINT_REASON_EVENT_MUTATION;
}

static void stygian_mark_repaint_reason(StygianContext *ctx, uint32_t reason) {
  if (!ctx || reason == STYGIAN_REPAINT_REASON_NONE)
    return;
  ctx->repaint.reason_flags |= reason;
}

static int32_t stygian_scope_ensure_index(StygianContext *ctx,
                                          StygianScopeId id) {
  int32_t idx;
  if (!ctx || id == 0u)
    return -1;
  idx = stygian_scope_find_index(ctx, id);
  if (idx >= 0)
    return idx;
  if (ctx->scope_count >= STYGIAN_SCOPE_CACHE_CAPACITY)
    return -1;
  idx = (int32_t)ctx->scope_count++;
  memset(&ctx->scope_cache[idx], 0, sizeof(ctx->scope_cache[idx]));
  ctx->scope_cache[idx].id = id;
  ctx->scope_cache[idx].dirty = true;
  ctx->scope_cache[idx].generation = 1u;
  return idx;
}

static bool stygian_profiles_equal(const StygianColorProfile *a,
                                   const StygianColorProfile *b) {
  size_t i;
  if (!a || !b || !a->valid || !b->valid)
    return false;
  if (a->srgb_transfer != b->srgb_transfer)
    return false;
  if (a->gamma != b->gamma)
    return false;
  for (i = 0; i < 9; i++) {
    if (a->rgb_to_xyz[i] != b->rgb_to_xyz[i])
      return false;
  }
  return true;
}

static void stygian_update_color_transform_state(StygianContext *ctx) {
  if (!ctx)
    return;
  ctx->glyph_color_transform_enabled = !stygian_profiles_equal(
      &ctx->glyph_source_color_profile, &ctx->output_color_profile);
}

static void stygian_mul3x3(const float a[9], const float b[9], float out[9]) {
  out[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
  out[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
  out[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];

  out[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
  out[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
  out[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];

  out[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
  out[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
  out[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];
}

static void stygian_push_output_color_transform(StygianContext *ctx) {
  static const float identity[9] = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  StygianColorProfile src_profile;
  float rgb3x3[9];
  bool enabled = false;

  if (!ctx || !ctx->ap)
    return;

  stygian_color_profile_init_builtin(&src_profile, STYGIAN_COLOR_SPACE_SRGB);
  memcpy(rgb3x3, identity, sizeof(rgb3x3));

  if (ctx->output_color_profile.valid && src_profile.valid &&
      !stygian_profiles_equal(&src_profile, &ctx->output_color_profile)) {
    // Source linear RGB -> XYZ -> output linear RGB.
    stygian_mul3x3(ctx->output_color_profile.xyz_to_rgb, src_profile.rgb_to_xyz,
                   rgb3x3);
    enabled = true;
  }

  stygian_ap_set_output_color_transform(
      ctx->ap, enabled, rgb3x3, src_profile.srgb_transfer, src_profile.gamma,
      ctx->output_color_profile.srgb_transfer, ctx->output_color_profile.gamma);
}

static uint32_t stygian_hash_u32(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7feb352du;
  v ^= v >> 15;
  v *= 0x846ca68bu;
  v ^= v >> 16;
  return v;
}

#define STYGIAN_HANDLE_INDEX_BITS 20u
#define STYGIAN_HANDLE_INDEX_MASK ((1u << STYGIAN_HANDLE_INDEX_BITS) - 1u)
#define STYGIAN_HANDLE_GENERATION_MASK (0xFFFFFFFFu ^ STYGIAN_HANDLE_INDEX_MASK)
#define STYGIAN_HANDLE_MAX_GENERATION 4095u

static uint32_t stygian_make_handle(uint32_t slot, uint16_t generation) {
  uint32_t index_part = slot + 1u;
  uint32_t generation_part = ((uint32_t)generation) << STYGIAN_HANDLE_INDEX_BITS;
  return (generation_part & STYGIAN_HANDLE_GENERATION_MASK) |
         (index_part & STYGIAN_HANDLE_INDEX_MASK);
}

static bool stygian_decode_handle(uint32_t handle, uint32_t capacity,
                                  uint32_t *out_slot,
                                  uint16_t *out_generation) {
  uint32_t index_part;
  uint32_t slot;
  uint32_t generation_part;
  if (handle == 0u)
    return false;
  index_part = handle & STYGIAN_HANDLE_INDEX_MASK;
  if (index_part == 0u)
    return false;
  slot = index_part - 1u;
  if (slot >= capacity)
    return false;
  generation_part = (handle >> STYGIAN_HANDLE_INDEX_BITS);
  if (generation_part == 0u)
    return false;
  if (out_slot)
    *out_slot = slot;
  if (out_generation)
    *out_generation = (uint16_t)generation_part;
  return true;
}

static uint16_t stygian_bump_generation(uint16_t generation) {
  uint32_t next = (uint32_t)generation + 1u;
  if (next == 0u || next > STYGIAN_HANDLE_MAX_GENERATION)
    next = 1u;
  return (uint16_t)next;
}

static bool stygian_resolve_element_slot(const StygianContext *ctx,
                                         StygianElement element,
                                         uint32_t *out_slot) {
  uint32_t slot;
  uint16_t generation;
  if (!ctx || !ctx->element_generations)
    return false;
  if (!stygian_decode_handle((uint32_t)element, ctx->config.max_elements, &slot,
                             &generation))
    return false;
  if (ctx->element_generations[slot] != generation)
    return false;
  if (!(ctx->soa.hot[slot].flags & STYGIAN_FLAG_ALLOCATED))
    return false;
  if (out_slot)
    *out_slot = slot;
  return true;
}

static bool stygian_resolve_texture_slot(const StygianContext *ctx,
                                         StygianTexture texture,
                                         uint32_t *out_slot,
                                         uint32_t *out_backend_id) {
  uint32_t slot;
  uint16_t generation;
  if (!ctx || !ctx->texture_generations || !ctx->texture_backend_ids)
    return false;
  if (!stygian_decode_handle((uint32_t)texture, ctx->config.max_textures, &slot,
                             &generation))
    return false;
  if (ctx->texture_generations[slot] != generation)
    return false;
  if (ctx->texture_backend_ids[slot] == 0u)
    return false;
  if (out_slot)
    *out_slot = slot;
  if (out_backend_id)
    *out_backend_id = ctx->texture_backend_ids[slot];
  return true;
}

static bool stygian_resolve_font_slot(const StygianContext *ctx, StygianFont font,
                                      uint32_t *out_slot) {
  uint32_t slot;
  uint16_t generation;
  if (!ctx || !ctx->font_generations || !ctx->font_alive)
    return false;
  if (!stygian_decode_handle((uint32_t)font, STYGIAN_MAX_FONTS, &slot,
                             &generation))
    return false;
  if (ctx->font_generations[slot] != generation)
    return false;
  if (!ctx->font_alive[slot])
    return false;
  if (out_slot)
    *out_slot = slot;
  return true;
}

static void stygian_reset_element_pool(StygianContext *ctx) {
  uint32_t i;
  if (!ctx || !ctx->free_list || !ctx->element_generations)
    return;
  for (i = 0u; i < ctx->config.max_elements; i++) {
    if (ctx->soa.hot[i].flags & STYGIAN_FLAG_ALLOCATED) {
      ctx->element_generations[i] =
          stygian_bump_generation(ctx->element_generations[i]);
      ctx->soa.hot[i].flags = 0u;
    }
    ctx->free_list[i] = ctx->config.max_elements - 1u - i;
  }
  ctx->free_count = ctx->config.max_elements;
}

static uint32_t stygian_hash_cstr(const char *str) {
  uint32_t hash = 2166136261u;
  const unsigned char *p = (const unsigned char *)str;
  if (!p)
    return 0u;
  while (*p) {
    hash ^= (uint32_t)(*p++);
    hash *= 16777619u;
  }
  return hash;
}

static uint32_t stygian_thread_id_u32(void) {
#ifdef _WIN32
  return (uint32_t)GetCurrentThreadId();
#else
  return stygian_hash_u32((uint32_t)(uintptr_t)&g_stygian_debug_in_frame);
#endif
}

static uint32_t stygian_current_source_tag(const StygianContext *ctx) {
  const char *source = stygian_get_repaint_source(ctx);
  if (!source || source[0] == '\0')
    return 0u;
  return stygian_hash_cstr(source);
}

static void stygian_context_log_error(StygianContext *ctx, uint32_t code,
                                      StygianScopeId scope_id,
                                      uint32_t source_tag,
                                      const char *message) {
  StygianContextErrorRecord record;
  uint32_t slot;
  const char *msg = message ? message : "";
  if (!ctx)
    return;

  memset(&record, 0, sizeof(record));
  record.timestamp_ms = stygian_now_ms();
  record.frame_index = ctx->frame_index;
  record.thread_id = stygian_thread_id_u32();
  record.code = code;
  record.scope_id = scope_id;
  record.source_tag = source_tag;
  record.message_hash = stygian_hash_cstr(msg);
  stygian_cpystr(record.message, sizeof(record.message), msg);

  slot = ctx->error_ring_head;
  ctx->error_ring[slot] = record;
  ctx->error_ring_head = (slot + 1u) % STYGIAN_ERROR_RING_CAPACITY;
  if (ctx->error_ring_count < STYGIAN_ERROR_RING_CAPACITY) {
    ctx->error_ring_count++;
  } else {
    ctx->error_ring_dropped++;
  }

  if (ctx->error_callback) {
    ctx->error_callback(ctx, code, msg, ctx->error_callback_user_data);
  } else if (g_default_context_error_callback) {
    g_default_context_error_callback(ctx, code, msg,
                                     g_default_context_error_callback_user_data);
  }
}

static void stygian_scope_dirty_reason(StygianContext *ctx, StygianScopeId id,
                                       bool next_frame, uint32_t reason,
                                       uint32_t source_tag) {
  int32_t idx;
  if (!ctx || id == 0u)
    return;
  idx = stygian_scope_ensure_index(ctx, id);
  if (idx < 0)
    return;
  if (next_frame)
    ctx->scope_cache[idx].dirty_next = true;
  else {
    ctx->scope_cache[idx].dirty = true;
    ctx->scope_cache[idx].dirty_next = false;
  }
  ctx->scope_cache[idx].generation++;
  ctx->scope_cache[idx].last_dirty_reason = reason;
  ctx->scope_cache[idx].last_source_tag = source_tag;
  ctx->scope_cache[idx].last_frame_index = ctx->frame_index;
}

static int stygian_cmd_compare(const void *lhs, const void *rhs) {
  const StygianCmdRecord *a = (const StygianCmdRecord *)lhs;
  const StygianCmdRecord *b = (const StygianCmdRecord *)rhs;
  // Deterministic per-property merge key:
  // (scope, element, property, priority, submit_seq, cmd_index).
  if (a->scope_id < b->scope_id)
    return -1;
  if (a->scope_id > b->scope_id)
    return 1;
  if (a->element_id < b->element_id)
    return -1;
  if (a->element_id > b->element_id)
    return 1;
  if (a->property_id < b->property_id)
    return -1;
  if (a->property_id > b->property_id)
    return 1;
  if (a->op_priority < b->op_priority)
    return -1;
  if (a->op_priority > b->op_priority)
    return 1;
  if (a->submit_seq < b->submit_seq)
    return -1;
  if (a->submit_seq > b->submit_seq)
    return 1;
  if (a->cmd_index < b->cmd_index)
    return -1;
  if (a->cmd_index > b->cmd_index)
    return 1;
  return 0;
}

static void stygian_record_winner(StygianContext *ctx,
                                  const StygianCmdRecord *record) {
  uint32_t slot;
  if (!ctx || !record)
    return;
  slot = ctx->winner_ring_head % STYGIAN_WINNER_RING_CAPACITY;
  ctx->winner_ring[slot].scope_id = record->scope_id;
  ctx->winner_ring[slot].winner_submit_seq = record->submit_seq;
  ctx->winner_ring[slot].frame_index = ctx->frame_index;
  ctx->winner_ring[slot].element_id = record->element_id;
  ctx->winner_ring[slot].property_id = record->property_id;
  ctx->winner_ring[slot].winner_source_tag = record->source_tag;
  ctx->winner_ring[slot].winner_cmd_index = record->cmd_index;
  ctx->winner_ring_head = (slot + 1u) % STYGIAN_WINNER_RING_CAPACITY;
}

static int32_t stygian_cmd_find_queue(StygianContext *ctx, uint32_t thread_id,
                                      bool create_if_missing) {
  uint32_t i;
  if (!ctx)
    return -1;
  for (i = 0u; i < ctx->cmd_queue_count; i++) {
    if (ctx->cmd_queues[i].owner_thread_id == thread_id)
      return (int32_t)i;
  }
  if (!create_if_missing || ctx->cmd_queue_count >= STYGIAN_CMD_MAX_PRODUCERS)
    return -1;
  i = ctx->cmd_queue_count++;
  ctx->cmd_queues[i].owner_thread_id = thread_id;
  ctx->cmd_queues[i].registered_order = i;
  ctx->cmd_buffers[i].ctx = ctx;
  ctx->cmd_buffers[i].queue_index = i;
  ctx->cmd_buffers[i].active = false;
  return (int32_t)i;
}

static bool stygian_cmd_apply_one(StygianContext *ctx,
                                  const StygianCmdRecord *record) {
  StygianElement element;
  if (!ctx || !record)
    return false;
  if (record->element_id == UINT32_MAX || record->element_handle == 0u)
    return false;
  element = (StygianElement)record->element_handle;

  switch (record->property_id) {
  case STYGIAN_CMD_PROP_BOUNDS:
    stygian_set_bounds(ctx, element, record->payload.bounds.x,
                       record->payload.bounds.y, record->payload.bounds.w,
                       record->payload.bounds.h);
    break;
  case STYGIAN_CMD_PROP_COLOR:
    stygian_set_color(ctx, element, record->payload.color.r,
                      record->payload.color.g, record->payload.color.b,
                      record->payload.color.a);
    break;
  case STYGIAN_CMD_PROP_BORDER:
    stygian_set_border(ctx, element, record->payload.color.r,
                       record->payload.color.g, record->payload.color.b,
                       record->payload.color.a);
    break;
  case STYGIAN_CMD_PROP_RADIUS:
    stygian_set_radius(ctx, element, record->payload.radius.tl,
                       record->payload.radius.tr, record->payload.radius.br,
                       record->payload.radius.bl);
    break;
  case STYGIAN_CMD_PROP_TYPE:
    stygian_set_type(ctx, element, (StygianType)record->payload.type.type);
    break;
  case STYGIAN_CMD_PROP_VISIBLE:
    stygian_set_visible(ctx, element, record->payload.visible.visible != 0u);
    break;
  case STYGIAN_CMD_PROP_Z:
    stygian_set_z(ctx, element, record->payload.depth.z);
    break;
  case STYGIAN_CMD_PROP_TEXTURE:
    stygian_set_texture(ctx, element, (StygianTexture)record->payload.texture.texture,
                        record->payload.texture.u0, record->payload.texture.v0,
                        record->payload.texture.u1, record->payload.texture.v1);
    break;
  case STYGIAN_CMD_PROP_SHADOW:
    stygian_set_shadow(ctx, element, record->payload.shadow.offset_x,
                       record->payload.shadow.offset_y, record->payload.shadow.blur,
                       record->payload.shadow.spread, record->payload.shadow.r,
                       record->payload.shadow.g, record->payload.shadow.b,
                       record->payload.shadow.a);
    break;
  case STYGIAN_CMD_PROP_GRADIENT:
    stygian_set_gradient(ctx, element, record->payload.gradient.angle,
                         record->payload.gradient.r1, record->payload.gradient.g1,
                         record->payload.gradient.b1, record->payload.gradient.a1,
                         record->payload.gradient.r2, record->payload.gradient.g2,
                         record->payload.gradient.b2, record->payload.gradient.a2);
    break;
  case STYGIAN_CMD_PROP_HOVER:
    stygian_set_hover(ctx, element, record->payload.scalar.value);
    break;
  case STYGIAN_CMD_PROP_BLEND:
    stygian_set_blend(ctx, element, record->payload.scalar.value);
    break;
  case STYGIAN_CMD_PROP_BLUR:
    stygian_set_blur(ctx, element, record->payload.scalar.value);
    break;
  case STYGIAN_CMD_PROP_GLOW:
    stygian_set_glow(ctx, element, record->payload.scalar.value);
    break;
  default:
    return false;
  }

  if (record->scope_id != 0u) {
    stygian_scope_dirty_reason(ctx, record->scope_id, false,
                               STYGIAN_REPAINT_REASON_EVENT_MUTATION,
                               record->source_tag);
  }
  stygian_record_winner(ctx, record);
  return true;
}

static uint32_t stygian_commit_pending_commands(StygianContext *ctx) {
  uint32_t frozen_epoch;
  uint32_t merge_count = 0u;
  uint32_t applied = 0u;
  uint32_t i, j;
  if (!ctx || !ctx->cmd_merge_records || ctx->cmd_merge_capacity == 0u)
    return 0u;

  // Freeze current producer epoch and flip publishers to the other epoch so
  // commit reads a stable snapshot without producer-side locking.
  frozen_epoch = ctx->cmd_publish_epoch;
  ctx->cmd_committing = true;
  ctx->cmd_publish_epoch = frozen_epoch ^ 1u;

  for (i = 0u; i < ctx->cmd_queue_count; i++) {
    StygianCmdQueueEpoch *slot = &ctx->cmd_queues[i].epoch[frozen_epoch];
    if (slot->dropped > 0u) {
      ctx->total_command_drops += slot->dropped;
      stygian_context_log_error(ctx, STYGIAN_ERROR_COMMAND_BUFFER_FULL, 0u, 0u,
                                "stygian command queue overflow");
      slot->dropped = 0u;
    }
    if (slot->count == 0u)
      continue;
    if (merge_count + slot->count > ctx->cmd_merge_capacity) {
      uint32_t room = ctx->cmd_merge_capacity - merge_count;
      for (j = 0u; j < room; j++) {
        ctx->cmd_merge_records[merge_count + j] = slot->records[j];
      }
      ctx->total_command_drops += (slot->count - room);
      stygian_context_log_error(ctx, STYGIAN_ERROR_COMMAND_BUFFER_FULL, 0u, 0u,
                                "stygian command merge overflow");
      merge_count = ctx->cmd_merge_capacity;
      slot->count = 0u;
      break;
    }
    for (j = 0u; j < slot->count; j++) {
      ctx->cmd_merge_records[merge_count + j] = slot->records[j];
    }
    merge_count += slot->count;
    slot->count = 0u;
  }

  if (merge_count > 1u) {
    // Stable ordering removes producer timing variance across runs.
    qsort(ctx->cmd_merge_records, merge_count, sizeof(StygianCmdRecord),
          stygian_cmd_compare);
  }

  for (i = 0u; i < merge_count; i++) {
    if (stygian_cmd_apply_one(ctx, &ctx->cmd_merge_records[i]))
      applied++;
  }

  ctx->cmd_committing = false;
  ctx->last_commit_applied = applied;
  if (applied > 0u) {
    stygian_mark_repaint_reason(ctx, STYGIAN_REPAINT_REASON_EVENT_MUTATION);
    stygian_set_repaint_source(ctx, "mutation-commit");
    stygian_request_repaint_after_ms(ctx, 0u);
  }
  return applied;
}

static uint32_t stygian_next_pow2_u32(uint32_t v) {
  uint32_t p = 1u;
  while (p < v && p != 0u)
    p <<= 1;
  return p ? p : v;
}

static int stygian_font_rebuild_glyph_hash(StygianContext *ctx,
                                           StygianFontAtlas *font,
                                           uint32_t min_capacity) {
  uint32_t cap, i;
  int32_t *hash;
  StygianAllocator *allocator =
      ctx ? ctx->allocator : &g_stygian_system_allocator;
  if (!font)
    return 0;
  cap = stygian_next_pow2_u32(min_capacity < 16u ? 16u : min_capacity);
  hash = (int32_t *)stygian_alloc_array(allocator, (size_t)cap, sizeof(int32_t),
                                        _Alignof(int32_t), false);
  if (!hash)
    return 0;
  for (i = 0; i < cap; i++)
    hash[i] = -1;
  for (i = 0; i < font->glyph_count; i++) {
    uint32_t cp = font->glyph_entries[i].codepoint;
    uint32_t slot = stygian_hash_u32(cp) & (cap - 1u);
    while (hash[slot] != -1)
      slot = (slot + 1u) & (cap - 1u);
    hash[slot] = (int32_t)i;
  }
  if (font->glyph_hash)
    stygian_free_raw(allocator, font->glyph_hash);
  font->glyph_hash = hash;
  font->glyph_hash_capacity = cap;
  return 1;
}

static int stygian_font_find_glyph_index(const StygianFontAtlas *font,
                                         uint32_t codepoint) {
  uint32_t slot;
  if (!font || !font->glyph_hash || font->glyph_hash_capacity == 0u)
    return -1;
  slot = stygian_hash_u32(codepoint) & (font->glyph_hash_capacity - 1u);
  while (font->glyph_hash[slot] != -1) {
    int idx = font->glyph_hash[slot];
    if (idx >= 0 && (uint32_t)idx < font->glyph_count &&
        font->glyph_entries[idx].codepoint == codepoint)
      return idx;
    slot = (slot + 1u) & (font->glyph_hash_capacity - 1u);
  }
  return -1;
}

static const StygianFontGlyph *
stygian_font_get_glyph(const StygianFontAtlas *font, uint32_t codepoint) {
  int idx;
  if (!font)
    return NULL;
  if (codepoint < 256u) {
    if (font->glyphs[codepoint].has_glyph)
      return &font->glyphs[codepoint];
    return NULL;
  }
  idx = stygian_font_find_glyph_index(font, codepoint);
  if (idx < 0 || (uint32_t)idx >= font->glyph_count)
    return NULL;
  return &font->glyph_entries[idx].glyph;
}

static float stygian_font_get_kerning(const StygianFontAtlas *font,
                                      uint32_t left, uint32_t right) {
  uint32_t i;
  if (!font)
    return 0.0f;
  if (left < 256u && right < 256u && font->kerning_ready &&
      font->kerning_has[left][right]) {
    return font->kerning_table[left][right];
  }
  for (i = 0; i < font->kerning_pair_count; i++) {
    if (font->kerning_pairs[i].left == left &&
        font->kerning_pairs[i].right == right) {
      return font->kerning_pairs[i].advance;
    }
  }
  return 0.0f;
}

static void stygian_font_free_dynamic(StygianContext *ctx,
                                      StygianFontAtlas *font) {
  StygianAllocator *allocator =
      ctx ? ctx->allocator : &g_stygian_system_allocator;
  if (!font)
    return;
  if (font->glyph_entries) {
    stygian_free_raw(allocator, font->glyph_entries);
    font->glyph_entries = NULL;
  }
  if (font->glyph_hash) {
    stygian_free_raw(allocator, font->glyph_hash);
    font->glyph_hash = NULL;
  }
  if (font->kerning_pairs) {
    stygian_free_raw(allocator, font->kerning_pairs);
    font->kerning_pairs = NULL;
  }
  font->glyph_count = 0u;
  font->glyph_capacity = 0u;
  font->glyph_hash_capacity = 0u;
  font->kerning_pair_count = 0u;
}

static uint64_t stygian_hash_str64(const char *s) {
  uint64_t h = 1469598103934665603ull;
  size_t i;
  if (!s)
    return 0ull;
  for (i = 0; s[i] != '\0'; i++) {
    h ^= (uint64_t)(uint8_t)s[i];
    h *= 1099511628211ull;
  }
  return h;
}

static int stygian_inline_emoji_cache_find(const StygianContext *ctx,
                                           uint64_t key_hash) {
  int i;
  if (!ctx || key_hash == 0ull)
    return -1;
  for (i = 0; i < STYGIAN_INLINE_EMOJI_CACHE_SIZE; i++) {
    if (ctx->inline_emoji_cache[i].used &&
        ctx->inline_emoji_cache[i].glyph_hash == key_hash) {
      return i;
    }
  }
  return -1;
}

static int stygian_inline_emoji_cache_choose_slot(const StygianContext *ctx) {
  int i;
  int oldest_idx = -1;
  uint32_t oldest_tick = UINT_MAX;
  if (!ctx)
    return -1;
  for (i = 0; i < STYGIAN_INLINE_EMOJI_CACHE_SIZE; i++) {
    if (!ctx->inline_emoji_cache[i].used)
      return i;
    if (ctx->inline_emoji_cache[i].last_used < oldest_tick) {
      oldest_tick = ctx->inline_emoji_cache[i].last_used;
      oldest_idx = i;
    }
  }
  return oldest_idx;
}

static void stygian_inline_emoji_cache_touch(StygianContext *ctx, int idx) {
  if (!ctx || idx < 0 || idx >= STYGIAN_INLINE_EMOJI_CACHE_SIZE)
    return;
  ctx->inline_emoji_clock++;
  if (ctx->inline_emoji_clock == 0u)
    ctx->inline_emoji_clock = 1u;
  ctx->inline_emoji_cache[idx].last_used = ctx->inline_emoji_clock;
}

static int stygian_try_parse_shortcode(const char *str, size_t text_len,
                                       size_t start, char *out_norm,
                                       size_t out_norm_size,
                                       size_t *out_after) {
  size_t i;
  size_t n;
  char raw[160];

  if (!str || !out_norm || !out_after || start >= text_len || str[start] != ':')
    return 0;

  for (i = start + 1u; i < text_len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (str[i] == '\r' || str[i] == '\n' || isspace(c))
      return 0;
    if (str[i] == ':')
      break;
    if (i - start >= 150u)
      return 0;
  }
  if (i >= text_len || str[i] != ':')
    return 0;

  n = i - start + 1u;
  if (n >= sizeof(raw))
    return 0;
  memcpy(raw, str + start, n);
  raw[n] = '\0';

  if (!stygian_shortcode_normalize(raw, out_norm, out_norm_size))
    return 0;

  *out_after = i + 1u;
  return 1;
}

static int stygian_inline_emoji_has_entry(const StygianContext *ctx,
                                          const char *normalized_id) {
  StygianTriadEntryInfo entry;
  if (!ctx || !normalized_id || !normalized_id[0] ||
      !stygian_triad_is_mounted(ctx))
    return 0;
  return stygian_triad_lookup_glyph_id(ctx, normalized_id, &entry) ? 1 : 0;
}

static int stygian_inline_emoji_resolve_texture(StygianContext *ctx,
                                                const char *normalized_id,
                                                uint32_t *out_texture) {
  StygianTriadEntryInfo entry;
  uint8_t *rgba = NULL;
  uint32_t w = 0, h = 0;
  uint64_t key_hash;
  int slot;
  uint32_t tex = 0;

  if (!ctx || !normalized_id || !normalized_id[0] || !out_texture)
    return 0;
  *out_texture = 0;

  key_hash = stygian_hash_str64(normalized_id);
  slot = stygian_inline_emoji_cache_find(ctx, key_hash);
  if (slot >= 0 && ctx->inline_emoji_cache[slot].texture_id != 0u) {
    stygian_inline_emoji_cache_touch(ctx, slot);
    *out_texture = ctx->inline_emoji_cache[slot].texture_id;
    return 1;
  }

  if (!stygian_triad_is_mounted(ctx) ||
      !stygian_triad_lookup_glyph_id(ctx, normalized_id, &entry)) {
    return 0;
  }
  if (!stygian_triad_decode_rgba(ctx, entry.glyph_hash, &rgba, &w, &h) ||
      !rgba || w == 0u || h == 0u) {
    return 0;
  }

  slot = stygian_inline_emoji_cache_choose_slot(ctx);
  if (slot < 0) {
    stygian_triad_free_blob(rgba);
    return 0;
  }

  if (ctx->inline_emoji_cache[slot].used &&
      ctx->inline_emoji_cache[slot].texture_id != 0u) {
    stygian_texture_destroy(ctx, ctx->inline_emoji_cache[slot].texture_id);
    ctx->inline_emoji_cache[slot].texture_id = 0u;
  }

  tex = stygian_texture_create(ctx, (int)w, (int)h, rgba);
  stygian_triad_free_blob(rgba);
  if (!tex)
    return 0;

  memset(&ctx->inline_emoji_cache[slot], 0,
         sizeof(ctx->inline_emoji_cache[slot]));
  ctx->inline_emoji_cache[slot].used = true;
  ctx->inline_emoji_cache[slot].glyph_hash = key_hash;
  ctx->inline_emoji_cache[slot].texture_id = tex;
  ctx->inline_emoji_cache[slot].width =
      (uint16_t)(w > UINT16_MAX ? UINT16_MAX : w);
  ctx->inline_emoji_cache[slot].height =
      (uint16_t)(h > UINT16_MAX ? UINT16_MAX : h);
  stygian_inline_emoji_cache_touch(ctx, slot);

  *out_texture = tex;
  return 1;
}

// ============================================================================
// Path Resolution (shared utility for all backends)
// ============================================================================

// Resolve file/directory path - tries CWD first, then exe-relative
static void resolve_path(const char *input_path, const char *default_path,
                         char *out_path, size_t out_size) {
  const char *rel_dir = input_path ? input_path : default_path;

  // If absolute path given, use it directly
  if (input_path && input_path[0] &&
      (input_path[0] == '/' || input_path[0] == '\\' ||
       (input_path[1] == ':'))) {
    stygian_cpystr(out_path, out_size, input_path);
    return;
  }

  // First try CWD-relative (most common during development)
  if (stygian_path_exists(rel_dir)) {
    stygian_cpystr(out_path, out_size, rel_dir);
    return;
  }

  // Try exe-relative path (for deployed builds)
  char bin_dir[512];
  if (stygian_get_binary_dir(bin_dir, sizeof(bin_dir))) {
    // Normalize backslashes to forward slashes for consistency
    for (char *p = bin_dir; *p; ++p) {
      if (*p == '\\')
        *p = '/';
    }

    // Check: bin_dir/shaders
    snprintf(out_path, out_size, "%s/%s", bin_dir, rel_dir);
    if (stygian_path_exists(out_path)) {
      return;
    }

    // Check: bin_dir/../shaders
    snprintf(out_path, out_size, "%s/../%s", bin_dir, rel_dir);
    if (stygian_path_exists(out_path)) {
      return;
    }

    // Check: bin_dir/../../shaders
    snprintf(out_path, out_size, "%s/../../%s", bin_dir, rel_dir);
    if (stygian_path_exists(out_path)) {
      return;
    }
  }

  // Fall back to relative path (will likely fail with clear error)
  stygian_cpystr(out_path, out_size, rel_dir);
}

// ============================================================================
// Context Lifecycle
// ============================================================================

void stygian_wait_for_events(StygianContext *ctx) {
  if (ctx && ctx->window) {
    StygianEvent evt;
    stygian_window_wait_event(ctx->window, &evt);
  }
}

void stygian_request_repaint_hz(StygianContext *ctx, uint32_t hz) {
  uint32_t interval_ms;
  uint64_t now_ms;
  uint64_t req_due;
  if (!ctx || hz == 0u)
    return;
  if (ctx->repaint.requested_hz_max < hz) {
    ctx->repaint.requested_hz_max = hz;
  }
  interval_ms = 1000u / hz;
  if (interval_ms < 1u)
    interval_ms = 1u;
  now_ms = stygian_now_ms();
  req_due = now_ms + (uint64_t)interval_ms;
  if (!ctx->repaint.has_pending || ctx->repaint.due_ms == 0ull ||
      req_due < ctx->repaint.due_ms) {
    ctx->repaint.due_ms = req_due;
  }
  ctx->repaint.has_pending = true;
  stygian_mark_repaint_reason(ctx, STYGIAN_REPAINT_REASON_TIMER);
  if (ctx->repaint.source[0] == '\0') {
    stygian_cpystr(ctx->repaint.source, sizeof(ctx->repaint.source), "timer");
  }
}

void stygian_request_repaint_after_ms(StygianContext *ctx, uint32_t ms) {
  uint64_t due_ms;
  if (!ctx)
    return;
  due_ms = stygian_now_ms() + (uint64_t)ms;
  if (ctx->repaint.deferred_due_ms == 0ull ||
      due_ms < ctx->repaint.deferred_due_ms) {
    ctx->repaint.deferred_due_ms = due_ms;
  }
  if (!ctx->repaint.has_pending || ctx->repaint.due_ms == 0ull ||
      due_ms < ctx->repaint.due_ms) {
    ctx->repaint.due_ms = due_ms;
  }
  ctx->repaint.has_pending = true;
  stygian_mark_repaint_reason(ctx, STYGIAN_REPAINT_REASON_TIMER);
  if (ctx->repaint.source[0] == '\0') {
    stygian_cpystr(ctx->repaint.source, sizeof(ctx->repaint.source),
                   "deferred");
  }
}

bool stygian_has_pending_repaint(const StygianContext *ctx) {
  uint64_t now_ms;
  if (!ctx)
    return false;
  if (!ctx->repaint.has_pending)
    return false;
  if (ctx->repaint.due_ms == 0ull)
    return true;
  now_ms = stygian_now_ms();
  return now_ms >= ctx->repaint.due_ms;
}

uint32_t stygian_next_repaint_wait_ms(const StygianContext *ctx,
                                      uint32_t idle_wait_ms) {
  uint64_t now_ms;
  uint64_t due_ms = 0ull;
  uint32_t wait_ms;

  if (!ctx)
    return idle_wait_ms;
  if (idle_wait_ms == 0u)
    idle_wait_ms = 1u;

  if (ctx->repaint.has_pending && ctx->repaint.due_ms > 0ull) {
    due_ms = ctx->repaint.due_ms;
  } else if (ctx->repaint.deferred_due_ms > 0ull) {
    due_ms = ctx->repaint.deferred_due_ms;
  } else {
    return idle_wait_ms;
  }

  now_ms = stygian_now_ms();
  if (due_ms <= now_ms)
    return 1u;

  wait_ms = (uint32_t)(due_ms - now_ms);
  if (wait_ms < 1u)
    wait_ms = 1u;
  if (wait_ms > idle_wait_ms)
    wait_ms = idle_wait_ms;
  return wait_ms;
}

void stygian_set_repaint_source(StygianContext *ctx, const char *source) {
  if (!ctx)
    return;
  if (!source || source[0] == '\0') {
    ctx->repaint.source[0] = '\0';
    return;
  }
  stygian_cpystr(ctx->repaint.source, sizeof(ctx->repaint.source), source);
  stygian_mark_repaint_reason(ctx, stygian_repaint_reason_from_source(source));
}

const char *stygian_get_repaint_source(const StygianContext *ctx) {
  if (!ctx)
    return "none";
  if (ctx->repaint.last_source[0] != '\0')
    return ctx->repaint.last_source;
  if (ctx->repaint.source[0] != '\0')
    return ctx->repaint.source;
  return "none";
}

uint32_t stygian_get_repaint_reason_flags(const StygianContext *ctx) {
  if (!ctx)
    return STYGIAN_REPAINT_REASON_NONE;
  if (ctx->repaint.last_reason_flags != STYGIAN_REPAINT_REASON_NONE)
    return ctx->repaint.last_reason_flags;
  return ctx->repaint.reason_flags;
}

void stygian_repaint_begin_frame(StygianContext *ctx) {
  if (!ctx)
    return;
  ctx->repaint.last_frame_ms = stygian_now_ms();
  ctx->repaint.requested_hz_max = 0u;
  ctx->repaint.deferred_due_ms = 0ull;
  ctx->repaint.reason_flags = STYGIAN_REPAINT_REASON_NONE;
}

void stygian_repaint_end_frame(StygianContext *ctx) {
  uint64_t now_ms;
  uint64_t due_ms = 0ull;

  if (!ctx)
    return;
  now_ms = stygian_now_ms();

  if (ctx->repaint.has_pending && ctx->repaint.due_ms > now_ms) {
    due_ms = ctx->repaint.due_ms;
  }

  if (ctx->repaint.requested_hz_max > 0u) {
    uint32_t interval_ms = 1000u / ctx->repaint.requested_hz_max;
    if (interval_ms < 1u)
      interval_ms = 1u;
    {
      uint64_t req_due = now_ms + (uint64_t)interval_ms;
      if (due_ms == 0ull || req_due < due_ms)
        due_ms = req_due;
    }
  }

  if (ctx->repaint.deferred_due_ms > 0ull &&
      (due_ms == 0ull || ctx->repaint.deferred_due_ms < due_ms)) {
    due_ms = ctx->repaint.deferred_due_ms;
  }

  if (due_ms > 0ull) {
    ctx->repaint.due_ms = due_ms;
    ctx->repaint.has_pending = true;
    if (ctx->repaint.source[0] != '\0') {
      stygian_cpystr(ctx->repaint.last_source, sizeof(ctx->repaint.last_source),
                     ctx->repaint.source);
    }
    ctx->repaint.last_reason_flags = ctx->repaint.reason_flags;
  } else {
    ctx->repaint.due_ms = 0ull;
    ctx->repaint.has_pending = false;
    ctx->repaint.last_source[0] = '\0';
    ctx->repaint.last_reason_flags = STYGIAN_REPAINT_REASON_NONE;
  }
  ctx->repaint.requested_hz_max = 0u;
  ctx->repaint.deferred_due_ms = 0ull;
  ctx->repaint.reason_flags = STYGIAN_REPAINT_REASON_NONE;
  ctx->repaint.source[0] = '\0';
  ctx->repaint.last_frame_ms = now_ms;
}

void stygian_scope_begin(StygianContext *ctx, StygianScopeId id) {
  int32_t idx;
  StygianScopeCacheEntry *entry;
  bool can_replay = false;
  uint32_t i;
  if (!ctx || id == 0u)
    return;

  idx = stygian_scope_ensure_index(ctx, id);
  if (idx < 0)
    return;
  entry = &ctx->scope_cache[idx];

  if (entry->dirty_next) {
    entry->dirty = true;
    entry->dirty_next = false;
  }

  if (ctx->active_scope_stack_top <
      (uint8_t)(sizeof(ctx->active_scope_stack) /
                sizeof(ctx->active_scope_stack[0]))) {
    ctx->active_scope_stack[ctx->active_scope_stack_top++] = (uint32_t)idx;
  }

  if (!entry->dirty && entry->range_count > 0u && !ctx->scope_replay_active &&
      entry->range_start == ctx->element_count &&
      ctx->free_count >= entry->range_count) {
    can_replay = true;
    for (i = 0u; i < entry->range_count; i++) {
      uint32_t expected = entry->range_start + i;
      uint32_t slot = ctx->free_list[ctx->free_count - 1u - i];
      if (slot != expected) {
        can_replay = false;
        break;
      }
    }
  }

  if (can_replay) {
    ctx->frame_scope_replay_hits++;
    ctx->free_count -= entry->range_count;
    ctx->element_count += entry->range_count;
    ctx->scope_replay_active = true;
    ctx->scope_replay_cursor = entry->range_start;
    ctx->scope_replay_end = entry->range_start + entry->range_count;
    ctx->suppress_element_writes = true;
    ctx->next_scope_dirty = false;
  } else {
    if (!entry->dirty && entry->range_count > 0u) {
      ctx->frame_scope_replay_misses++;
    }
    entry->dirty = true;
    entry->range_start = ctx->element_count;
    entry->range_count = 0u;
    entry->clip_snapshot = ctx->clip_stack_top > 0u
                               ? ctx->clip_stack[ctx->clip_stack_top - 1u]
                               : 0u;
    entry->z_snapshot = 0.0f;
    ctx->scope_replay_active = false;
    ctx->scope_replay_cursor = 0u;
    ctx->scope_replay_end = 0u;
    ctx->suppress_element_writes = false;
    ctx->next_scope_dirty = true;
  }
  ctx->active_scope_index = idx;
}

void stygian_scope_end(StygianContext *ctx) {
  uint32_t idx;
  StygianScopeCacheEntry *entry;
  if (!ctx)
    return;
  if (ctx->active_scope_stack_top > 0u) {
    idx = ctx->active_scope_stack[ctx->active_scope_stack_top - 1u];
    entry = &ctx->scope_cache[idx];
    if (ctx->scope_replay_active) {
      if (ctx->scope_replay_cursor != ctx->scope_replay_end) {
        // Call-site mismatch: rebuild next frame.
        ctx->frame_scope_forced_rebuilds++;
        entry->dirty = true;
        stygian_request_repaint_hz(ctx, 60u);
      } else {
        entry->dirty = false;
      }
    } else {
      if (ctx->element_count >= entry->range_start) {
        entry->range_count = ctx->element_count - entry->range_start;
      } else {
        entry->range_count = 0u;
      }
      entry->dirty = false;
    }
    ctx->active_scope_stack_top--;
  }

  ctx->scope_replay_active = false;
  ctx->scope_replay_cursor = 0u;
  ctx->scope_replay_end = 0u;
  ctx->suppress_element_writes = false;

  if (ctx->active_scope_stack_top > 0u) {
    ctx->active_scope_index =
        (int32_t)ctx->active_scope_stack[ctx->active_scope_stack_top - 1u];
    ctx->next_scope_dirty = ctx->scope_cache[ctx->active_scope_index].dirty;
  } else {
    ctx->active_scope_index = -1;
    ctx->next_scope_dirty = true;
  }
}

void stygian_scope_invalidate(StygianContext *ctx, StygianScopeId id) {
  stygian_scope_invalidate_now(ctx, id);
}

void stygian_scope_invalidate_now(StygianContext *ctx, StygianScopeId id) {
  uint32_t reason;
  uint32_t source_tag;
  if (!ctx || id == 0u)
    return;
  reason = ctx->repaint.reason_flags;
  if (reason == STYGIAN_REPAINT_REASON_NONE)
    reason = STYGIAN_REPAINT_REASON_EVENT_MUTATION;
  source_tag = stygian_current_source_tag(ctx);
  stygian_scope_dirty_reason(ctx, id, false, reason, source_tag);
}

void stygian_scope_invalidate_next(StygianContext *ctx, StygianScopeId id) {
  uint32_t reason;
  uint32_t source_tag;
  if (!ctx || id == 0u)
    return;
  reason = ctx->repaint.reason_flags;
  if (reason == STYGIAN_REPAINT_REASON_NONE)
    reason = STYGIAN_REPAINT_REASON_EVENT_MUTATION;
  source_tag = stygian_current_source_tag(ctx);
  stygian_scope_dirty_reason(ctx, id, true, reason, source_tag);
}

bool stygian_scope_is_dirty(StygianContext *ctx, StygianScopeId id) {
  int32_t idx;
  if (!ctx || id == 0u)
    return true;
  idx = stygian_scope_find_index(ctx, id);
  if (idx < 0)
    return true;
  return ctx->scope_cache[idx].dirty || ctx->scope_cache[idx].dirty_next;
}

// DDI: Overlay scope convenience APIs
void stygian_overlay_scope_begin(StygianContext *ctx, uint32_t overlay_id) {
  if (!ctx)
    return;
  StygianScopeId id = STYGIAN_OVERLAY_SCOPE_BASE | (uint64_t)overlay_id;
  stygian_scope_begin(ctx, id);
}

void stygian_overlay_scope_end(StygianContext *ctx) {
  // Just delegates to regular scope_end
  stygian_scope_end(ctx);
}

void stygian_request_overlay_hz(StygianContext *ctx, uint32_t hz) {
  if (!ctx || hz == 0)
    return;
  // Only invalidate overlay scopes, not base UI scopes
  stygian_invalidate_overlay_scopes(ctx);
  // Request repaint at overlay tick rate
  stygian_request_repaint_hz(ctx, hz);
}

void stygian_invalidate_overlay_scopes(StygianContext *ctx) {
  uint32_t source_tag;
  if (!ctx)
    return;
  source_tag = stygian_current_source_tag(ctx);
  // Only mark overlay scopes as dirty, preserve base UI scopes
  for (uint32_t i = 0; i < ctx->scope_count; i++) {
    if (STYGIAN_IS_OVERLAY_SCOPE(ctx->scope_cache[i].id)) {
      ctx->scope_cache[i].dirty_next = true;
      ctx->scope_cache[i].generation++;
      ctx->scope_cache[i].last_dirty_reason = STYGIAN_REPAINT_REASON_TIMER;
      ctx->scope_cache[i].last_source_tag = source_tag;
      ctx->scope_cache[i].last_frame_index = ctx->frame_index;
    }
  }
}

StygianContext *stygian_create(const StygianConfig *config) {
  if (!config)
    return NULL;
  bool auto_profile = (config->glyph_feature_flags == 0);
  StygianAllocator *allocator = stygian_resolve_allocator(config);
  StygianContext *ctx = (StygianContext *)stygian_alloc_raw(
      allocator, sizeof(StygianContext), _Alignof(StygianContext), true);
  if (!ctx)
    return NULL;

  // Copy config with defaults
  ctx->config = *config;
  if (ctx->config.max_elements == 0)
    ctx->config.max_elements = STYGIAN_MAX_ELEMENTS;
  if (ctx->config.max_textures == 0)
    ctx->config.max_textures = STYGIAN_MAX_TEXTURES;
  if (ctx->config.glyph_feature_flags == 0) {
    ctx->config.glyph_feature_flags = STYGIAN_GLYPH_FEATURE_DEFAULT;
  }
  ctx->allocator = allocator;
  ctx->glyph_feature_flags = ctx->config.glyph_feature_flags;
  ctx->repaint.requested_hz_max = 0u;
  ctx->repaint.deferred_due_ms = 0ull;
  ctx->repaint.due_ms = 0ull;
  ctx->repaint.last_frame_ms = stygian_now_ms();
  ctx->repaint.has_pending = false;
  ctx->repaint.reason_flags = STYGIAN_REPAINT_REASON_NONE;
  ctx->repaint.last_reason_flags = STYGIAN_REPAINT_REASON_NONE;
  ctx->active_scope_index = -1;
  ctx->next_scope_dirty = true;
  ctx->scope_replay_active = false;
  ctx->scope_replay_cursor = 0u;
  ctx->scope_replay_end = 0u;
  ctx->suppress_element_writes = false;
  ctx->frame_scope_replay_hits = 0u;
  ctx->frame_scope_replay_misses = 0u;
  ctx->frame_scope_forced_rebuilds = 0u;
  ctx->stats_log_interval_ms = 10000u; // Default: log every 10 seconds
  ctx->stats_last_log_ms = stygian_now_ms();
  ctx->last_frame_scope_replay_hits = 0u;
  ctx->last_frame_scope_replay_misses = 0u;
  ctx->last_frame_scope_forced_rebuilds = 0u;
  ctx->last_frame_reason_flags = STYGIAN_REPAINT_REASON_NONE;
  ctx->last_frame_eval_only = 0u;
  ctx->eval_only_frame = false;
  ctx->frame_intent = STYGIAN_FRAME_RENDER;
  ctx->cmd_queue_count = 0u;
  ctx->cmd_publish_epoch = 0u;
  ctx->cmd_submit_seq_next = 0u;
  ctx->cmd_committing = false;
  ctx->last_commit_applied = 0u;
  ctx->total_command_drops = 0u;
  ctx->cmd_merge_records = NULL;
  ctx->cmd_merge_capacity = 0u;
  ctx->winner_ring_head = 0u;
  ctx->error_callback = g_default_context_error_callback;
  ctx->error_callback_user_data = g_default_context_error_callback_user_data;
  ctx->error_ring_head = 0u;
  ctx->error_ring_count = 0u;
  ctx->error_ring_dropped = 0u;

  // Create per-frame scratch arena (4MB default)
  ctx->frame_arena = stygian_arena_create(4 * 1024 * 1024);

  // Allocate stable ID/free-list storage for SoA pools.
  ctx->free_list = (uint32_t *)stygian_alloc_array(
      allocator, ctx->config.max_elements, sizeof(uint32_t), _Alignof(uint32_t),
      true);
  ctx->element_generations = (uint16_t *)stygian_alloc_array(
      allocator, ctx->config.max_elements, sizeof(uint16_t),
      _Alignof(uint16_t), true);

  ctx->texture_free_list = (uint32_t *)stygian_alloc_array(
      allocator, ctx->config.max_textures, sizeof(uint32_t),
      _Alignof(uint32_t), true);
  ctx->texture_generations = (uint16_t *)stygian_alloc_array(
      allocator, ctx->config.max_textures, sizeof(uint16_t),
      _Alignof(uint16_t), true);
  ctx->texture_backend_ids = (uint32_t *)stygian_alloc_array(
      allocator, ctx->config.max_textures, sizeof(uint32_t),
      _Alignof(uint32_t), true);

  if (!ctx->free_list || !ctx->element_generations || !ctx->texture_free_list ||
      !ctx->texture_generations || !ctx->texture_backend_ids) {
    stygian_destroy(ctx);
    return NULL;
  }

  // Initialize free list (all elements free)
  for (uint32_t i = 0; i < ctx->config.max_elements; i++) {
    ctx->free_list[i] = ctx->config.max_elements - 1 - i; // Reverse order
    ctx->element_generations[i] = 1u;
  }
  ctx->free_count = ctx->config.max_elements;
  for (uint32_t i = 0; i < ctx->config.max_textures; i++) {
    ctx->texture_free_list[i] = ctx->config.max_textures - 1 - i;
    ctx->texture_generations[i] = 1u;
    ctx->texture_backend_ids[i] = 0u;
  }
  ctx->texture_free_count = ctx->config.max_textures;
  ctx->texture_count = 0u;

  // Allocate SoA arrays; zero-fill keeps untouched cold fields deterministic.
  {
    uint32_t max_el = ctx->config.max_elements;
    ctx->soa.hot = (StygianSoAHot *)stygian_alloc_array(
        allocator, max_el, sizeof(StygianSoAHot), _Alignof(StygianSoAHot),
        true);
    ctx->soa.appearance = (StygianSoAAppearance *)stygian_alloc_array(
        allocator, max_el, sizeof(StygianSoAAppearance),
        _Alignof(StygianSoAAppearance), true);
    ctx->soa.effects = (StygianSoAEffects *)stygian_alloc_array(
        allocator, max_el, sizeof(StygianSoAEffects),
        _Alignof(StygianSoAEffects), true);
    ctx->soa.capacity = max_el;
    ctx->soa.element_count = 0;

    if (!ctx->soa.hot || !ctx->soa.appearance || !ctx->soa.effects) {
      stygian_destroy(ctx);
      return NULL;
    }

    // Allocate chunk tracking
    ctx->chunk_size = STYGIAN_DEFAULT_CHUNK_SIZE;
    ctx->chunk_count = (max_el + ctx->chunk_size - 1) / ctx->chunk_size;
    ctx->chunks = (StygianBufferChunk *)stygian_alloc_array(
        allocator, ctx->chunk_count, sizeof(StygianBufferChunk),
        _Alignof(StygianBufferChunk), true);
    if (!ctx->chunks) {
      stygian_destroy(ctx);
      return NULL;
    }
    // Init all chunks: dirty_min > dirty_max means "no dirty range"
    for (uint32_t ci = 0; ci < ctx->chunk_count; ci++) {
      ctx->chunks[ci].hot_dirty_min = UINT32_MAX;
      ctx->chunks[ci].appearance_dirty_min = UINT32_MAX;
      ctx->chunks[ci].effects_dirty_min = UINT32_MAX;
    }

    for (uint32_t qi = 0u; qi < STYGIAN_CMD_MAX_PRODUCERS; qi++) {
      for (uint32_t epoch = 0u; epoch < 2u; epoch++) {
        ctx->cmd_queues[qi].epoch[epoch].records =
            (StygianCmdRecord *)stygian_alloc_array(
                allocator, STYGIAN_CMD_QUEUE_CAPACITY,
                sizeof(StygianCmdRecord), _Alignof(StygianCmdRecord), true);
        if (!ctx->cmd_queues[qi].epoch[epoch].records) {
          stygian_destroy(ctx);
          return NULL;
        }
      }
      ctx->cmd_buffers[qi].ctx = ctx;
      ctx->cmd_buffers[qi].queue_index = qi;
      ctx->cmd_buffers[qi].active = false;
    }

    ctx->cmd_merge_capacity = STYGIAN_CMD_MAX_PRODUCERS *
                              STYGIAN_CMD_QUEUE_CAPACITY;
    ctx->cmd_merge_records = (StygianCmdRecord *)stygian_alloc_array(
        allocator, ctx->cmd_merge_capacity, sizeof(StygianCmdRecord),
        _Alignof(StygianCmdRecord), true);
    if (!ctx->cmd_merge_records) {
      stygian_destroy(ctx);
      return NULL;
    }
  }

  // Allocate clip regions
  ctx->clips = (StygianClipRect *)stygian_alloc_array(
      allocator, STYGIAN_MAX_CLIPS, sizeof(StygianClipRect),
      _Alignof(StygianClipRect), true);

  // Allocate font storage
  ctx->fonts = (StygianFontAtlas *)stygian_alloc_array(
      allocator, STYGIAN_MAX_FONTS, sizeof(StygianFontAtlas),
      _Alignof(StygianFontAtlas), true);
  ctx->font_free_list = (uint32_t *)stygian_alloc_array(
      allocator, STYGIAN_MAX_FONTS, sizeof(uint32_t), _Alignof(uint32_t), true);
  ctx->font_generations = (uint16_t *)stygian_alloc_array(
      allocator, STYGIAN_MAX_FONTS, sizeof(uint16_t), _Alignof(uint16_t), true);
  ctx->font_alive = (uint8_t *)stygian_alloc_array(
      allocator, STYGIAN_MAX_FONTS, sizeof(uint8_t), _Alignof(uint8_t), true);
  ctx->triad_runtime = stygian_triad_runtime_create();
  if (!ctx->fonts || !ctx->font_free_list || !ctx->font_generations ||
      !ctx->font_alive || !ctx->triad_runtime) {
    stygian_destroy(ctx);
    return NULL;
  }
  for (uint32_t i = 0u; i < STYGIAN_MAX_FONTS; i++) {
    ctx->font_free_list[i] = STYGIAN_MAX_FONTS - 1u - i;
    ctx->font_generations[i] = 1u;
    ctx->font_alive[i] = 0u;
  }
  ctx->font_free_count = STYGIAN_MAX_FONTS;
  ctx->font_count = 0u;

  // Store window pointer (required)
  if (!config->window) {
    fprintf(stderr, "[Stygian] Error: StygianWindow is required\n");
    stygian_destroy(ctx);
    return NULL;
  }
  ctx->window = config->window;

  // Resolve shader directory path (core responsibility, not backend)
  char resolved_shader_dir[256];
  resolve_path(config->shader_dir, "shaders", resolved_shader_dir,
               sizeof(resolved_shader_dir));

  // Create graphics access point
  StygianAPType ap_type = STYGIAN_AP_OPENGL;
  switch (ctx->config.backend) {
  case STYGIAN_BACKEND_VULKAN:
    ap_type = STYGIAN_AP_VULKAN;
    break;
  case STYGIAN_BACKEND_DX12:
    ap_type = STYGIAN_AP_DX12;
    break;
  case STYGIAN_BACKEND_METAL:
    ap_type = STYGIAN_AP_METAL;
    break;
  case STYGIAN_BACKEND_OPENGL:
  default:
    ap_type = STYGIAN_AP_OPENGL;
    break;
  }

  StygianAPConfig ap_config = {
      .type = ap_type,
      .window = config->window,
      .max_elements = ctx->config.max_elements,
      .max_textures = ctx->config.max_textures,
      .shader_dir = resolved_shader_dir,
      .allocator = allocator,
  };

  ctx->ap = stygian_ap_create(&ap_config);
  if (!ctx->ap) {
    fprintf(stderr, "[Stygian] Error: Failed to create graphics AP\n");
    stygian_destroy(ctx);
    return NULL;
  }

  if (auto_profile) {
    StygianAPAdapterClass cls = stygian_ap_get_adapter_class(ctx->ap);
    if (cls == STYGIAN_AP_ADAPTER_IGPU) {
      stygian_set_glyph_profile(ctx, STYGIAN_GLYPH_PROFILE_IGPU_BG_DECODE);
    } else {
      stygian_set_glyph_profile(ctx, STYGIAN_GLYPH_PROFILE_DGPU_INTERACTIVE);
    }
  }

  stygian_color_profile_init_builtin(&ctx->output_color_profile,
                                     STYGIAN_COLOR_SPACE_SRGB);
  stygian_color_profile_init_builtin(&ctx->glyph_source_color_profile,
                                     STYGIAN_COLOR_SPACE_SRGB);
  stygian_update_color_transform_state(ctx);
  stygian_push_output_color_transform(ctx);

  // Load default font atlas if present
  {
    StygianFont font_id =
        stygian_font_load(ctx, "assets/atlas.png", "assets/atlas.json");
    if (!font_id) {
      fprintf(stderr, "[Stygian] Warning: Default font atlas not loaded "
                      "(assets/atlas.png, assets/atlas.json)\n");
    }
  }

  ctx->initialized = true;
  return ctx;
}

void stygian_destroy(StygianContext *ctx) {
  int i;
  StygianAllocator *allocator;
  if (!ctx)
    return;
  allocator = ctx->allocator ? ctx->allocator : &g_stygian_system_allocator;
  // Free dynamic font-side allocations before releasing owning arrays.
  if (ctx->fonts) {
    uint32_t i;
    for (i = 0; i < STYGIAN_MAX_FONTS; i++) {
      if (ctx->font_alive && ctx->font_alive[i]) {
        if (ctx->fonts[i].texture_handle) {
          stygian_texture_destroy(ctx, ctx->fonts[i].texture_handle);
          ctx->fonts[i].texture_handle = 0u;
          ctx->fonts[i].texture_backend_id = 0u;
        }
        stygian_font_free_dynamic(ctx, &ctx->fonts[i]);
      }
    }
  }

  for (i = 0; i < STYGIAN_INLINE_EMOJI_CACHE_SIZE; i++) {
    if (ctx->inline_emoji_cache[i].used &&
        ctx->inline_emoji_cache[i].texture_id != 0u) {
      stygian_texture_destroy(ctx, ctx->inline_emoji_cache[i].texture_id);
      ctx->inline_emoji_cache[i].texture_id = 0u;
    }
    ctx->inline_emoji_cache[i].used = false;
  }

  // Destroy graphics access point
  if (ctx->ap) {
    stygian_ap_destroy(ctx->ap);
    ctx->ap = NULL;
  }

  // Window lifetime is external to the context.
  for (uint32_t qi = 0u; qi < STYGIAN_CMD_MAX_PRODUCERS; qi++) {
    for (uint32_t epoch = 0u; epoch < 2u; epoch++) {
      stygian_free_raw(allocator, ctx->cmd_queues[qi].epoch[epoch].records);
      ctx->cmd_queues[qi].epoch[epoch].records = NULL;
      ctx->cmd_queues[qi].epoch[epoch].count = 0u;
      ctx->cmd_queues[qi].epoch[epoch].dropped = 0u;
    }
  }
  stygian_free_raw(allocator, ctx->cmd_merge_records);
  ctx->cmd_merge_records = NULL;
  stygian_free_raw(allocator, ctx->free_list);
  stygian_free_raw(allocator, ctx->element_generations);
  stygian_free_raw(allocator, ctx->texture_free_list);
  stygian_free_raw(allocator, ctx->texture_generations);
  stygian_free_raw(allocator, ctx->texture_backend_ids);
  stygian_free_raw(allocator, ctx->soa.hot);
  stygian_free_raw(allocator, ctx->soa.appearance);
  stygian_free_raw(allocator, ctx->soa.effects);
  stygian_free_raw(allocator, ctx->chunks);
  stygian_free_raw(allocator, ctx->clips);
  stygian_free_raw(allocator, ctx->fonts);
  stygian_free_raw(allocator, ctx->font_free_list);
  stygian_free_raw(allocator, ctx->font_generations);
  stygian_free_raw(allocator, ctx->font_alive);
  stygian_triad_runtime_destroy(ctx->triad_runtime);
  ctx->triad_runtime = NULL;
  if (ctx->frame_arena) {
    stygian_arena_destroy(ctx->frame_arena);
    ctx->frame_arena = NULL;
  }
  stygian_free_raw(allocator, ctx);
}

StygianAP *stygian_get_ap(StygianContext *ctx) { return ctx ? ctx->ap : NULL; }

// ============================================================================
// Frame Management
// ============================================================================

void stygian_begin_frame_intent(StygianContext *ctx, int width, int height,
                                StygianFrameIntent intent) {
  if (!ctx)
    return;

#ifndef NDEBUG
  {
    extern int g_stygian_debug_in_frame;
    g_stygian_debug_in_frame = 1;
  }
#endif

  ctx->frame_intent = intent;
  ctx->eval_only_frame = (intent == STYGIAN_FRAME_EVAL_ONLY);

  // Reset per-frame scratch arena
  if (ctx->frame_arena) {
    stygian_arena_reset(ctx->frame_arena);
  }

  stygian_repaint_begin_frame(ctx);
  stygian_commit_pending_commands(ctx);

  ctx->width = width;
  ctx->height = height;

  // Frame routing uses scope dirtiness plus repaint scheduler state.
  bool has_dirty_overlay_scopes = false;
  bool has_dirty_non_overlay_scopes = false;
  bool repaint_due = false;
  uint32_t overlay_trim_start = ctx->element_count;
  for (uint32_t i = 0; i < ctx->scope_count; i++) {
    if (ctx->scope_cache[i].dirty || ctx->scope_cache[i].dirty_next) {
      if (STYGIAN_IS_OVERLAY_SCOPE(ctx->scope_cache[i].id)) {
        has_dirty_overlay_scopes = true;
        if (ctx->scope_cache[i].range_start < overlay_trim_start) {
          overlay_trim_start = ctx->scope_cache[i].range_start;
        }
      } else {
        has_dirty_non_overlay_scopes = true;
      }
    }
  }
  repaint_due = stygian_has_pending_repaint(ctx);

  // Non-overlay dirtiness rebuilds frame output from scratch.
  if (ctx->scope_count == 0 || has_dirty_non_overlay_scopes) {
    ctx->element_count = 0;
    ctx->transient_start = 0;
    ctx->transient_count = 0;

    // Rebuild allocator cursor so subsequent writes replay deterministically.
    stygian_reset_element_pool(ctx);
    ctx->skip_frame = false;
  } else if (has_dirty_overlay_scopes) {
    // Overlay-only dirty frame: start replay from slot 0 so clean non-overlay
    // scopes can replay in-order. Starting at overlay_trim_start breaks replay
    // invariants and forces expensive rebuilds.
    (void)overlay_trim_start;
    ctx->element_count = 0;
    ctx->transient_start = 0;
    ctx->transient_count = 0;
    stygian_reset_element_pool(ctx);
    ctx->skip_frame = false;
  } else if (repaint_due) {
    // Timer/forced repaint with clean scopes still starts replay from slot 0;
    // this keeps evaluate/replay deterministic without dirtying scopes.
    ctx->element_count = 0;
    ctx->transient_start = 0;
    ctx->transient_count = 0;
    stygian_reset_element_pool(ctx);
    ctx->skip_frame = false;
  } else {
    // Fully clean frame: preserve cached data and skip GPU work.
    ctx->skip_frame = true;
  }

  // Reset clip stack
  ctx->clip_count = 1;
  ctx->clips[0] = (StygianClipRect){0.0f, 0.0f, (float)width, (float)height};
  ctx->clip_stack_top = 0;

  // Reset render layers
  ctx->layer_count = 0;
  ctx->layer_active = false;
  ctx->layer_start = 0;
  ctx->frame_draw_calls = 0;
  ctx->frame_begin_cpu_ms = stygian_now_ms();
  ctx->active_scope_stack_top = 0u;
  ctx->active_scope_index = -1;
  ctx->next_scope_dirty = true;
  ctx->scope_replay_active = false;
  ctx->scope_replay_cursor = 0u;
  ctx->scope_replay_end = 0u;
  ctx->suppress_element_writes = false;
  ctx->frame_scope_replay_hits = 0u;
  ctx->frame_scope_replay_misses = 0u;
  ctx->frame_scope_forced_rebuilds = 0u;

  // Eval-only frames run all bookkeeping/state paths but never touch AP submit.
  // Render intent only begins AP frame when there is actual GPU work to do.
  if (!ctx->skip_frame && !ctx->eval_only_frame) {
    stygian_ap_begin_frame(ctx->ap, width, height);
  }
}

void stygian_begin_frame(StygianContext *ctx, int width, int height) {
  stygian_begin_frame_intent(ctx, width, height, STYGIAN_FRAME_RENDER);
}

void stygian_set_glyph_feature_flags(StygianContext *ctx, uint32_t flags) {
  if (!ctx)
    return;
  ctx->glyph_feature_flags = flags;
  ctx->config.glyph_feature_flags = flags;
}

uint32_t stygian_get_glyph_feature_flags(const StygianContext *ctx) {
  if (!ctx)
    return 0u;
  return ctx->glyph_feature_flags;
}

bool stygian_glyph_feature_enabled(const StygianContext *ctx,
                                   uint32_t feature_flag) {
  if (!ctx)
    return false;
  return (ctx->glyph_feature_flags & feature_flag) != 0u;
}

void stygian_set_glyph_profile(StygianContext *ctx,
                               StygianGlyphProfile profile) {
  if (!ctx)
    return;
  stygian_set_glyph_feature_flags(ctx, stygian_profile_to_flags(profile));
}

uint32_t stygian_glyph_profile_flags(StygianGlyphProfile profile) {
  return stygian_profile_to_flags(profile);
}

StygianGlyphPath stygian_select_glyph_path(const StygianContext *ctx,
                                           bool triad_data_available,
                                           bool bc4_supported) {
  uint32_t flags =
      ctx ? ctx->glyph_feature_flags : STYGIAN_GLYPH_FEATURE_DEFAULT;

  if (triad_data_available && bc4_supported &&
      (flags & STYGIAN_GLYPH_TRIAD_PRIMARY)) {
    return STYGIAN_GLYPH_PATH_TRIAD_BC4;
  }
  if (triad_data_available && (flags & STYGIAN_GLYPH_TRIAD_FALLBACK_R8)) {
    return STYGIAN_GLYPH_PATH_TRIAD_R8;
  }
  if (flags & STYGIAN_GLYPH_FALLBACK_MTSDF) {
    return STYGIAN_GLYPH_PATH_MTSDF;
  }
  return STYGIAN_GLYPH_PATH_DISABLED;
}

bool stygian_set_output_color_space(StygianContext *ctx,
                                    StygianColorSpace color_space) {
  if (!ctx)
    return false;
  stygian_color_profile_init_builtin(&ctx->output_color_profile, color_space);
  stygian_update_color_transform_state(ctx);
  stygian_push_output_color_transform(ctx);
  return ctx->output_color_profile.valid;
}

bool stygian_set_output_icc_profile(StygianContext *ctx, const char *icc_path,
                                    StygianICCInfo *out_info) {
  if (!ctx || !icc_path || !icc_path[0])
    return false;
  if (!stygian_icc_load_profile(icc_path, &ctx->output_color_profile,
                                out_info)) {
    return false;
  }
  stygian_update_color_transform_state(ctx);
  stygian_push_output_color_transform(ctx);
  return true;
}

bool stygian_get_output_color_profile(const StygianContext *ctx,
                                      StygianColorProfile *out_profile) {
  if (!ctx || !out_profile)
    return false;
  return stygian_color_profile_copy(out_profile, &ctx->output_color_profile);
}

bool stygian_set_glyph_source_color_space(StygianContext *ctx,
                                          StygianColorSpace color_space) {
  if (!ctx)
    return false;
  stygian_color_profile_init_builtin(&ctx->glyph_source_color_profile,
                                     color_space);
  stygian_update_color_transform_state(ctx);
  return ctx->glyph_source_color_profile.valid;
}

bool stygian_get_glyph_source_color_profile(const StygianContext *ctx,
                                            StygianColorProfile *out_profile) {
  if (!ctx || !out_profile)
    return false;
  return stygian_color_profile_copy(out_profile,
                                    &ctx->glyph_source_color_profile);
}

bool stygian_triad_mount(StygianContext *ctx, const char *triad_path) {
  if (!ctx || !ctx->triad_runtime)
    return false;
  return stygian_triad_runtime_mount(ctx->triad_runtime, triad_path);
}

void stygian_triad_unmount(StygianContext *ctx) {
  if (!ctx || !ctx->triad_runtime)
    return;
  stygian_triad_runtime_unmount(ctx->triad_runtime);
}

bool stygian_triad_is_mounted(const StygianContext *ctx) {
  if (!ctx || !ctx->triad_runtime)
    return false;
  return stygian_triad_runtime_is_mounted(ctx->triad_runtime);
}

bool stygian_triad_get_pack_info(const StygianContext *ctx,
                                 StygianTriadPackInfo *out_info) {
  if (!ctx || !ctx->triad_runtime)
    return false;
  return stygian_triad_runtime_get_pack_info(ctx->triad_runtime, out_info);
}

bool stygian_triad_lookup(const StygianContext *ctx, uint64_t glyph_hash,
                          StygianTriadEntryInfo *out_entry) {
  if (!ctx || !ctx->triad_runtime)
    return false;
  return stygian_triad_runtime_lookup(ctx->triad_runtime, glyph_hash,
                                      out_entry);
}

uint64_t stygian_triad_hash_key(const char *glyph_id, const char *source_tag) {
  return stygian_triad_runtime_hash_key(glyph_id, source_tag);
}

bool stygian_triad_lookup_glyph_id(const StygianContext *ctx,
                                   const char *glyph_id,
                                   StygianTriadEntryInfo *out_entry) {
  if (!ctx || !ctx->triad_runtime)
    return false;
  return stygian_triad_runtime_lookup_glyph_id(ctx->triad_runtime, glyph_id,
                                               out_entry);
}

bool stygian_triad_read_svg_blob(const StygianContext *ctx, uint64_t glyph_hash,
                                 uint8_t **out_svg_data,
                                 uint32_t *out_svg_size) {
  if (!ctx || !ctx->triad_runtime)
    return false;
  return stygian_triad_runtime_read_svg_blob(ctx->triad_runtime, glyph_hash,
                                             out_svg_data, out_svg_size);
}

bool stygian_triad_decode_rgba(const StygianContext *ctx, uint64_t glyph_hash,
                               uint8_t **out_rgba_data, uint32_t *out_width,
                               uint32_t *out_height) {
  if (!ctx || !ctx->triad_runtime)
    return false;
  return stygian_triad_runtime_decode_rgba(
      ctx->triad_runtime, glyph_hash, out_rgba_data, out_width, out_height);
}

void stygian_triad_free_blob(void *ptr) {
  stygian_triad_runtime_free_blob(ptr);
}

void stygian_layer_begin(StygianContext *ctx) {
  if (!ctx || ctx->layer_active)
    return;
  ctx->layer_active = true;
  ctx->layer_start = ctx->element_count;
}

void stygian_layer_end(StygianContext *ctx) {
  if (!ctx || !ctx->layer_active)
    return;
  if (ctx->layer_count <
      (uint16_t)(sizeof(ctx->layers) / sizeof(ctx->layers[0]))) {
    uint32_t start = ctx->layer_start;
    uint32_t count = ctx->element_count - start;
    ctx->layers[ctx->layer_count].start = start;
    ctx->layers[ctx->layer_count].count = count;
    ctx->layer_count++;
  }
  ctx->layer_active = false;
}

void stygian_end_frame(StygianContext *ctx) {

  uint64_t t_build_end;
  uint64_t t_submit_end;
  uint64_t t_present_end;
  if (!ctx)
    return;

#ifndef NDEBUG
  {
    extern int g_stygian_debug_in_frame;
    g_stygian_debug_in_frame = 0;
  }
#endif

  if (ctx->layer_active) {
    stygian_layer_end(ctx);
  }

  t_build_end = stygian_now_ms();

  // Eval-only and fully clean frames keep state fresh with zero AP work.
  if (ctx->skip_frame || ctx->eval_only_frame) {
    ctx->frames_skipped++;
    ctx->last_frame_element_count = ctx->element_count;
    ctx->last_frame_clip_count = ctx->clip_count;
    ctx->last_frame_draw_calls = 0;
    ctx->last_frame_upload_bytes = 0;
    ctx->last_frame_upload_ranges = 0;
    ctx->last_frame_scope_replay_hits = ctx->frame_scope_replay_hits;
    ctx->last_frame_scope_replay_misses = ctx->frame_scope_replay_misses;
    ctx->last_frame_scope_forced_rebuilds = ctx->frame_scope_forced_rebuilds;
    ctx->last_frame_build_ms = (float)(t_build_end - ctx->frame_begin_cpu_ms);
    ctx->last_frame_submit_ms = 0.0f;
    ctx->last_frame_present_ms = 0.0f;
    ctx->last_frame_gpu_ms = 0.0f;
    ctx->last_frame_reason_flags = ctx->repaint.reason_flags;
    ctx->last_frame_eval_only = ctx->eval_only_frame ? 1u : 0u;
    ctx->frame_index++;
    stygian_repaint_end_frame(ctx);
    if (ctx->eval_only_frame)
      ctx->stats_frames_eval_only++;
    else
      ctx->stats_frames_skipped++;
    if (ctx->last_frame_reason_flags & STYGIAN_REPAINT_REASON_EVENT_MUTATION)
      ctx->stats_reason_mutation++;
    if (ctx->last_frame_reason_flags &
        (STYGIAN_REPAINT_REASON_TIMER | STYGIAN_REPAINT_REASON_ANIMATION))
      ctx->stats_reason_timer++;
    if (ctx->last_frame_reason_flags & STYGIAN_REPAINT_REASON_ASYNC)
      ctx->stats_reason_async++;
    if (ctx->last_frame_reason_flags & STYGIAN_REPAINT_REASON_FORCED)
      ctx->stats_reason_forced++;
    return;
  }

  stygian_ap_gpu_timer_begin(ctx->ap);
  stygian_ap_set_clips(ctx->ap, (const float *)ctx->clips, ctx->clip_count);
  stygian_ap_submit(ctx->ap, ctx->soa.hot, ctx->element_count);
  stygian_ap_submit_soa(ctx->ap, ctx->soa.hot, ctx->soa.appearance,
                        ctx->soa.effects, ctx->soa.element_count, ctx->chunks,
                        ctx->chunk_count, ctx->chunk_size);

  if (ctx->layer_count == 0) {
    // Single pass when no layered ordering is requested.
    stygian_ap_draw(ctx->ap);
    ctx->frame_draw_calls++;
  } else {
    // Layered draws preserve ordering while keeping contiguous gap ranges valid.
    uint32_t prev_end = 0;
    for (uint16_t i = 0; i < ctx->layer_count; i++) {
      uint32_t layer_start = ctx->layers[i].start;
      uint32_t layer_count = ctx->layers[i].count;

      if (layer_start > prev_end) {
        uint32_t gap_count = layer_start - prev_end;
        stygian_ap_draw_range(ctx->ap, prev_end, gap_count);
        ctx->frame_draw_calls++;
      }

      if (layer_count > 0) {
        stygian_ap_draw_range(ctx->ap, layer_start, layer_count);
        ctx->frame_draw_calls++;
      }

      prev_end = layer_start + layer_count;
    }

    if (ctx->element_count > prev_end) {
      uint32_t gap_count = ctx->element_count - prev_end;
      stygian_ap_draw_range(ctx->ap, prev_end, gap_count);
      ctx->frame_draw_calls++;
    }
  }
  t_submit_end = stygian_now_ms();
  stygian_ap_gpu_timer_end(ctx->ap);

  ctx->last_frame_element_count = ctx->element_count;
  ctx->last_frame_clip_count = ctx->clip_count;
  ctx->last_frame_draw_calls = ctx->frame_draw_calls;
  ctx->last_frame_upload_bytes = stygian_ap_get_last_upload_bytes(ctx->ap);
  ctx->last_frame_upload_ranges = stygian_ap_get_last_upload_ranges(ctx->ap);
  ctx->last_frame_gpu_ms = stygian_ap_get_last_gpu_ms(ctx->ap);
  ctx->last_frame_scope_replay_hits = ctx->frame_scope_replay_hits;
  ctx->last_frame_scope_replay_misses = ctx->frame_scope_replay_misses;
  ctx->last_frame_scope_forced_rebuilds = ctx->frame_scope_forced_rebuilds;
  ctx->last_frame_build_ms = (float)(t_build_end - ctx->frame_begin_cpu_ms);
  ctx->last_frame_submit_ms = (float)(t_submit_end - t_build_end);
  ctx->last_frame_reason_flags = ctx->repaint.reason_flags;
  ctx->last_frame_eval_only = 0u;
  ctx->frame_index++;

  // Finalize backend frame state before present.
  stygian_ap_end_frame(ctx->ap);

  // Present only on render-intent frames.
  stygian_ap_swap(ctx->ap);
  t_present_end = stygian_now_ms();
  ctx->last_frame_present_ms = (float)(t_present_end - t_submit_end);

  stygian_repaint_end_frame(ctx);

  // Accumulate rolling diagnostics for periodic logs.
  ctx->stats_frames_rendered++;
  ctx->stats_total_upload_bytes += ctx->last_frame_upload_bytes;
  ctx->stats_scope_replay_hits += ctx->last_frame_scope_replay_hits;
  ctx->stats_scope_replay_misses += ctx->last_frame_scope_replay_misses;
  ctx->stats_scope_forced_rebuilds += ctx->last_frame_scope_forced_rebuilds;
  ctx->stats_total_build_ms += ctx->last_frame_build_ms;
  ctx->stats_total_submit_ms += ctx->last_frame_submit_ms;
  ctx->stats_total_present_ms += ctx->last_frame_present_ms;
  if (ctx->last_frame_reason_flags & STYGIAN_REPAINT_REASON_EVENT_MUTATION)
    ctx->stats_reason_mutation++;
  if (ctx->last_frame_reason_flags &
      (STYGIAN_REPAINT_REASON_TIMER | STYGIAN_REPAINT_REASON_ANIMATION))
    ctx->stats_reason_timer++;
  if (ctx->last_frame_reason_flags & STYGIAN_REPAINT_REASON_ASYNC)
    ctx->stats_reason_async++;
  if (ctx->last_frame_reason_flags & STYGIAN_REPAINT_REASON_FORCED)
    ctx->stats_reason_forced++;

  // Periodic machine-readable telemetry for perf gates.
  if (ctx->stats_log_interval_ms > 0u) {
    uint64_t now_ms = stygian_now_ms();
    uint64_t elapsed = now_ms - ctx->stats_last_log_ms;
    if (elapsed >= ctx->stats_log_interval_ms) {
      uint32_t n = ctx->stats_frames_rendered;
      uint32_t total_scopes =
          ctx->stats_scope_replay_hits + ctx->stats_scope_replay_misses;
      float hit_pct = total_scopes > 0 ? (float)ctx->stats_scope_replay_hits /
                                             (float)total_scopes * 100.f
                                       : 0.f;
      printf("[Stygian] %u frames (%.1fs) | "
             "avg build=%.2fms submit=%.2fms present=%.2fms | "
             "upload=%lluKB scope_hit=%.0f%% skipped=%u forced=%u\n",
             n, (float)elapsed / 1000.f,
             n > 0 ? ctx->stats_total_build_ms / (float)n : 0.f,
             n > 0 ? ctx->stats_total_submit_ms / (float)n : 0.f,
             n > 0 ? ctx->stats_total_present_ms / (float)n : 0.f,
             (unsigned long long)(ctx->stats_total_upload_bytes / 1024ull),
             hit_pct, ctx->stats_frames_skipped,
             ctx->stats_scope_forced_rebuilds);
      printf("STYGIAN_METRIC sample_ms=%llu render=%u eval=%u skipped=%u "
             "reason_mut=%u reason_timer=%u reason_async=%u reason_forced=%u "
             "upload_bytes=%llu replay_hit=%u replay_miss=%u cmd_applied=%u "
             "cmd_drops=%u\n",
             (unsigned long long)elapsed, ctx->stats_frames_rendered,
             ctx->stats_frames_eval_only, ctx->stats_frames_skipped,
             ctx->stats_reason_mutation, ctx->stats_reason_timer,
             ctx->stats_reason_async, ctx->stats_reason_forced,
             (unsigned long long)ctx->stats_total_upload_bytes,
             ctx->stats_scope_replay_hits, ctx->stats_scope_replay_misses,
             ctx->last_commit_applied, ctx->total_command_drops);
      // Reset accumulators
      ctx->stats_frames_rendered = 0u;
      ctx->stats_frames_skipped = 0u;
      ctx->stats_frames_eval_only = 0u;
      ctx->stats_total_upload_bytes = 0ull;
      ctx->stats_scope_replay_hits = 0u;
      ctx->stats_scope_replay_misses = 0u;
      ctx->stats_scope_forced_rebuilds = 0u;
      ctx->stats_total_build_ms = 0.f;
      ctx->stats_total_submit_ms = 0.f;
      ctx->stats_total_present_ms = 0.f;
      ctx->stats_reason_mutation = 0u;
      ctx->stats_reason_timer = 0u;
      ctx->stats_reason_async = 0u;
      ctx->stats_reason_forced = 0u;
      ctx->stats_last_log_ms = now_ms;
    }
  }
}

uint32_t stygian_get_frame_draw_calls(const StygianContext *ctx) {
  return ctx ? ctx->frame_draw_calls : 0u;
}

uint32_t stygian_get_last_frame_draw_calls(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_draw_calls : 0u;
}

uint32_t stygian_get_last_frame_element_count(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_element_count : 0u;
}

uint32_t stygian_get_last_frame_clip_count(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_clip_count : 0u;
}

uint32_t stygian_get_last_frame_upload_bytes(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_upload_bytes : 0u;
}

uint32_t stygian_get_last_frame_upload_ranges(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_upload_ranges : 0u;
}

uint32_t stygian_get_last_frame_scope_replay_hits(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_scope_replay_hits : 0u;
}

uint32_t stygian_get_last_frame_scope_replay_misses(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_scope_replay_misses : 0u;
}

uint32_t
stygian_get_last_frame_scope_forced_rebuilds(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_scope_forced_rebuilds : 0u;
}

float stygian_get_last_frame_build_ms(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_build_ms : 0.0f;
}

float stygian_get_last_frame_submit_ms(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_submit_ms : 0.0f;
}

float stygian_get_last_frame_present_ms(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_present_ms : 0.0f;
}

float stygian_get_last_frame_gpu_ms(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_gpu_ms : 0.0f;
}

uint32_t stygian_get_last_frame_reason_flags(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_reason_flags : STYGIAN_REPAINT_REASON_NONE;
}

uint32_t stygian_get_last_frame_eval_only(const StygianContext *ctx) {
  return ctx ? ctx->last_frame_eval_only : 0u;
}

bool stygian_is_eval_only_frame(const StygianContext *ctx) {
  return ctx ? ctx->eval_only_frame : false;
}

uint32_t stygian_get_frames_skipped(const StygianContext *ctx) {
  return ctx ? ctx->frames_skipped : 0u;
}

uint32_t stygian_get_active_element_count(const StygianContext *ctx) {
  return ctx ? ctx->element_count : 0u;
}

uint32_t stygian_get_element_capacity(const StygianContext *ctx) {
  return ctx ? ctx->config.max_elements : 0u;
}

uint32_t stygian_get_free_element_count(const StygianContext *ctx) {
  return ctx ? ctx->free_count : 0u;
}

uint32_t stygian_get_font_count(const StygianContext *ctx) {
  return ctx ? ctx->font_count : 0u;
}

uint32_t stygian_get_inline_emoji_cache_count(const StygianContext *ctx) {
  uint32_t count = 0u;
  uint32_t i = 0u;
  if (!ctx)
    return 0u;
  for (i = 0u; i < STYGIAN_INLINE_EMOJI_CACHE_SIZE; ++i) {
    if (ctx->inline_emoji_cache[i].used)
      count++;
  }
  return count;
}

uint16_t stygian_get_clip_capacity(const StygianContext *ctx) {
  (void)ctx;
  return STYGIAN_MAX_CLIPS;
}

uint32_t stygian_get_last_commit_applied(const StygianContext *ctx) {
  return ctx ? ctx->last_commit_applied : 0u;
}

uint32_t stygian_get_total_command_drops(const StygianContext *ctx) {
  return ctx ? ctx->total_command_drops : 0u;
}

bool stygian_element_is_valid(const StygianContext *ctx, StygianElement e) {
  return stygian_resolve_element_slot(ctx, e, NULL);
}

bool stygian_texture_is_valid(const StygianContext *ctx, StygianTexture tex) {
  return stygian_resolve_texture_slot(ctx, tex, NULL, NULL);
}

bool stygian_font_is_valid(const StygianContext *ctx, StygianFont font) {
  return stygian_resolve_font_slot(ctx, font, NULL);
}

bool stygian_scope_get_last_dirty_info(const StygianContext *ctx,
                                       StygianScopeId id, uint32_t *out_reason,
                                       uint32_t *out_source_tag,
                                       uint32_t *out_frame_index) {
  int32_t idx;
  if (!ctx || id == 0u)
    return false;
  idx = stygian_scope_find_index(ctx, id);
  if (idx < 0)
    return false;
  if (out_reason)
    *out_reason = ctx->scope_cache[idx].last_dirty_reason;
  if (out_source_tag)
    *out_source_tag = ctx->scope_cache[idx].last_source_tag;
  if (out_frame_index)
    *out_frame_index = ctx->scope_cache[idx].last_frame_index;
  return true;
}

void stygian_context_set_error_callback(StygianContext *ctx,
                                        StygianContextErrorCallback callback,
                                        void *user_data) {
  if (!ctx)
    return;
  ctx->error_callback = callback;
  ctx->error_callback_user_data = user_data;
}

void stygian_set_default_context_error_callback(
    StygianContextErrorCallback callback, void *user_data) {
  g_default_context_error_callback = callback;
  g_default_context_error_callback_user_data = user_data;
}

uint32_t stygian_context_get_recent_errors(const StygianContext *ctx,
                                           StygianContextErrorRecord *out,
                                           uint32_t max_count) {
  uint32_t available;
  uint32_t count;
  uint32_t i;
  if (!ctx || !out || max_count == 0u)
    return 0u;
  available = ctx->error_ring_count;
  count = available < max_count ? available : max_count;
  for (i = 0u; i < count; i++) {
    uint32_t idx =
        (ctx->error_ring_head + STYGIAN_ERROR_RING_CAPACITY - 1u - i) %
        STYGIAN_ERROR_RING_CAPACITY;
    out[i] = ctx->error_ring[idx];
  }
  return count;
}

uint32_t stygian_context_get_error_drop_count(const StygianContext *ctx) {
  return ctx ? ctx->error_ring_dropped : 0u;
}

// ============================================================================
// Element Allocation
// ============================================================================

StygianElement stygian_element(StygianContext *ctx) {
  uint32_t id;
  if (!ctx)
    return 0;

  if (ctx->scope_replay_active) {
    if (ctx->scope_replay_cursor >= ctx->scope_replay_end) {
      if (ctx->active_scope_index >= 0) {
        ctx->scope_cache[ctx->active_scope_index].dirty = true;
      }
      return 0;
    }
    id = ctx->scope_replay_cursor++;
    return (StygianElement)stygian_make_handle(id, ctx->element_generations[id]);
  }

  if (ctx->free_count == 0)
    return 0;

  id = ctx->free_list[--ctx->free_count];

  // Initialize SoA (zero-fill cold, set hot defaults)
  memset(&ctx->soa.hot[id], 0, sizeof(StygianSoAHot));
  memset(&ctx->soa.appearance[id], 0, sizeof(StygianSoAAppearance));
  memset(&ctx->soa.effects[id], 0, sizeof(StygianSoAEffects));

  uint32_t flags = STYGIAN_FLAG_ALLOCATED | STYGIAN_FLAG_VISIBLE;
  if (ctx->clip_stack_top > 0) {
    uint8_t active_clip = ctx->clip_stack[ctx->clip_stack_top - 1];
    flags |= ((uint32_t)active_clip << STYGIAN_CLIP_SHIFT);
  }
  ctx->soa.hot[id].flags = flags;
  ctx->soa.hot[id].color[3] = 1.0f;
  ctx->soa.effects[id].blend = 1.0f;
  stygian_mark_soa_hot_dirty(ctx, id);
  stygian_mark_soa_appearance_dirty(ctx, id);
  stygian_mark_soa_effects_dirty(ctx, id);

  // Track max used
  if (id >= ctx->element_count) {
    ctx->element_count = id + 1;
  }
  if (id >= ctx->soa.element_count) {
    ctx->soa.element_count = id + 1;
  }

  return (StygianElement)stygian_make_handle(id, ctx->element_generations[id]);
}

StygianElement stygian_element_transient(StygianContext *ctx) {
  uint32_t id;
  StygianElement e = stygian_element(ctx);
  if (e) {
    if (stygian_resolve_element_slot(ctx, e, &id))
      ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
    ctx->transient_count++;
  }
  return e;
}

uint32_t stygian_element_batch(StygianContext *ctx, uint32_t count,
                               StygianElement *out_ids) {
  if (!ctx || !out_ids || count == 0)
    return 0;

  // Scope replay path: hand out sequential IDs from cached range
  if (ctx->scope_replay_active) {
    uint32_t avail = (ctx->scope_replay_cursor < ctx->scope_replay_end)
                         ? (ctx->scope_replay_end - ctx->scope_replay_cursor)
                         : 0u;
    uint32_t n = count < avail ? count : avail;
    for (uint32_t i = 0; i < n; i++) {
      uint32_t id = ctx->scope_replay_cursor++;
      out_ids[i] = (StygianElement)stygian_make_handle(id, ctx->element_generations[id]);
    }
    if (n < count && ctx->active_scope_index >= 0) {
      ctx->scope_cache[ctx->active_scope_index].dirty = true;
    }
    return n;
  }

  // Clamp to available free slots
  uint32_t avail = ctx->free_count;
  uint32_t n = count < avail ? count : avail;
  if (n == 0)
    return 0;

  // Compute clip flag once for all elements
  uint32_t base_flags = STYGIAN_FLAG_ALLOCATED | STYGIAN_FLAG_VISIBLE;
  if (ctx->clip_stack_top > 0) {
    uint8_t active_clip = ctx->clip_stack[ctx->clip_stack_top - 1];
    base_flags |= ((uint32_t)active_clip << STYGIAN_CLIP_SHIFT);
  }

  uint32_t max_id = ctx->element_count;

  for (uint32_t i = 0; i < n; i++) {
    uint32_t id = ctx->free_list[--ctx->free_count];
    out_ids[i] = (StygianElement)stygian_make_handle(id, ctx->element_generations[id]);

    // SoA init
    memset(&ctx->soa.hot[id], 0, sizeof(StygianSoAHot));
    memset(&ctx->soa.appearance[id], 0, sizeof(StygianSoAAppearance));
    memset(&ctx->soa.effects[id], 0, sizeof(StygianSoAEffects));
    ctx->soa.hot[id].flags = base_flags;
    ctx->soa.hot[id].color[3] = 1.0f;
    ctx->soa.effects[id].blend = 1.0f;

    if (id >= max_id)
      max_id = id + 1;

    stygian_mark_soa_hot_dirty(ctx, id);
    stygian_mark_soa_appearance_dirty(ctx, id);
    stygian_mark_soa_effects_dirty(ctx, id);
  }

  if (max_id > ctx->element_count)
    ctx->element_count = max_id;
  if (max_id > ctx->soa.element_count)
    ctx->soa.element_count = max_id;

  return n;
}

void stygian_element_free(StygianContext *ctx, StygianElement e) {
  uint32_t id;
  if (!ctx)
    return;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  ctx->soa.hot[id].flags = 0;
  stygian_mark_soa_hot_dirty(ctx, id);
  ctx->element_generations[id] = stygian_bump_generation(ctx->element_generations[id]);
  ctx->free_list[ctx->free_count++] = id;
}

// ============================================================================
// Element Setters
// ============================================================================

void stygian_set_bounds(StygianContext *ctx, StygianElement e, float x, float y,
                        float w, float h) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  StygianSoAHot *hot = &ctx->soa.hot[id];
  hot->x = x;
  hot->y = y;
  hot->w = w;
  hot->h = h;
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_color(StygianContext *ctx, StygianElement e, float r, float g,
                       float b, float a) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  StygianSoAHot *hot = &ctx->soa.hot[id];
  hot->color[0] = r;
  hot->color[1] = g;
  hot->color[2] = b;
  hot->color[3] = a;
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_border(StygianContext *ctx, StygianElement e, float r, float g,
                        float b, float a) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  ctx->soa.appearance[id].border_color[0] = r;
  ctx->soa.appearance[id].border_color[1] = g;
  ctx->soa.appearance[id].border_color[2] = b;
  ctx->soa.appearance[id].border_color[3] = a;
  stygian_mark_soa_appearance_dirty(ctx, id);
  // Set render_mode bit: this element has appearance data
  // Simplified: just OR in the appearance mode
  ctx->soa.hot[id].type |= STYGIAN_MODE_APPEARANCE;
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_radius(StygianContext *ctx, StygianElement e, float tl,
                        float tr, float br, float bl) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  ctx->soa.appearance[id].radius[0] = tl;
  ctx->soa.appearance[id].radius[1] = tr;
  ctx->soa.appearance[id].radius[2] = br;
  ctx->soa.appearance[id].radius[3] = bl;
  stygian_mark_soa_appearance_dirty(ctx, id);
}

void stygian_set_type(StygianContext *ctx, StygianElement e, StygianType type) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write: preserve render_mode bits in upper 16
  ctx->soa.hot[id].type = (ctx->soa.hot[id].type & ~STYGIAN_TYPE_MASK) |
                          ((uint32_t)type & STYGIAN_TYPE_MASK);
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_visible(StygianContext *ctx, StygianElement e, bool visible) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  if (visible)
    ctx->soa.hot[id].flags |= STYGIAN_FLAG_VISIBLE;
  else
    ctx->soa.hot[id].flags &= ~STYGIAN_FLAG_VISIBLE;
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_z(StygianContext *ctx, StygianElement e, float z) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  ctx->soa.hot[id].z = z;
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_texture(StygianContext *ctx, StygianElement e,
                         StygianTexture tex, float u0, float v0, float u1,
                         float v1) {
  uint32_t id;
  uint32_t backend_tex = 0u;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;
  if (tex != 0u && !stygian_resolve_texture_slot(ctx, tex, NULL, &backend_tex))
    return;

  ctx->soa.hot[id].texture_id = backend_tex;
  stygian_mark_soa_hot_dirty(ctx, id);
  ctx->soa.appearance[id].uv[0] = u0;
  ctx->soa.appearance[id].uv[1] = v0;
  ctx->soa.appearance[id].uv[2] = u1;
  ctx->soa.appearance[id].uv[3] = v1;
  stygian_mark_soa_appearance_dirty(ctx, id);
}

void stygian_set_shadow(StygianContext *ctx, StygianElement e, float offset_x,
                        float offset_y, float blur, float spread, float r,
                        float g, float b, float a) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  StygianSoAEffects *fx = &ctx->soa.effects[id];
  fx->shadow_offset[0] = offset_x;
  fx->shadow_offset[1] = offset_y;
  fx->shadow_blur = blur;
  fx->shadow_spread = spread;
  fx->shadow_color[0] = r;
  fx->shadow_color[1] = g;
  fx->shadow_color[2] = b;
  fx->shadow_color[3] = a;
  stygian_mark_soa_effects_dirty(ctx, id);
  // Set render_mode bit
  ctx->soa.hot[id].type |= STYGIAN_MODE_EFFECTS;
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_gradient(StygianContext *ctx, StygianElement e, float angle,
                          float r1, float g1, float b1, float a1, float r2,
                          float g2, float b2, float a2) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  StygianSoAEffects *fx = &ctx->soa.effects[id];
  fx->gradient_angle = angle;
  fx->gradient_start[0] = r1;
  fx->gradient_start[1] = g1;
  fx->gradient_start[2] = b1;
  fx->gradient_start[3] = a1;
  fx->gradient_end[0] = r2;
  fx->gradient_end[1] = g2;
  fx->gradient_end[2] = b2;
  fx->gradient_end[3] = a2;
  stygian_mark_soa_effects_dirty(ctx, id);
  // Set render_mode bit
  ctx->soa.hot[id].type |= STYGIAN_MODE_EFFECTS;
  stygian_mark_soa_hot_dirty(ctx, id);
}

void stygian_set_hover(StygianContext *ctx, StygianElement e, float hover) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  ctx->soa.effects[id].hover = hover;
  stygian_mark_soa_effects_dirty(ctx, id);
}

void stygian_set_blend(StygianContext *ctx, StygianElement e, float blend) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  ctx->soa.effects[id].blend = blend;
  stygian_mark_soa_effects_dirty(ctx, id);
}

void stygian_set_blur(StygianContext *ctx, StygianElement e, float radius) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  ctx->soa.effects[id].blur_radius = radius;
  stygian_mark_soa_effects_dirty(ctx, id);
}

void stygian_set_glow(StygianContext *ctx, StygianElement e, float intensity) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  // SoA write
  ctx->soa.effects[id].glow_intensity = intensity;
  stygian_mark_soa_effects_dirty(ctx, id);
}

static bool stygian_cmd_append_record(StygianCmdBuffer *buffer,
                                      StygianCmdRecord *record) {
  StygianContext *ctx;
  StygianCmdProducerQueue *queue;
  StygianCmdQueueEpoch *slot;
  if (!buffer || !record || !buffer->active)
    return false;
  ctx = buffer->ctx;
  if (!ctx)
    return false;
  if (ctx->cmd_committing) {
    stygian_context_log_error(ctx, STYGIAN_ERROR_INVALID_STATE, 0u,
                              buffer->source_tag,
                              "submit attempted during commit");
    return false;
  }
  if (buffer->queue_index >= ctx->cmd_queue_count)
    return false;
  if (buffer->epoch != ctx->cmd_publish_epoch) {
    stygian_context_log_error(ctx, STYGIAN_ERROR_INVALID_STATE, 0u,
                              buffer->source_tag,
                              "submit attempted to frozen epoch");
    return false;
  }
  queue = &ctx->cmd_queues[buffer->queue_index];
  slot = &queue->epoch[buffer->epoch];
  if (slot->count >= STYGIAN_CMD_QUEUE_CAPACITY) {
    slot->dropped++;
    ctx->total_command_drops++;
    stygian_context_log_error(ctx, STYGIAN_ERROR_COMMAND_BUFFER_FULL, 0u,
                              buffer->source_tag,
                              "producer queue capacity reached");
    return false;
  }
  record->scope_id = buffer->scope_id;
  record->source_tag = buffer->source_tag;
  record->submit_seq = 0u;
  record->cmd_index = 0u;
  slot->records[slot->count++] = *record;
  buffer->count++;
  return true;
}

StygianCmdBuffer *stygian_cmd_begin(StygianContext *ctx, uint32_t source_tag) {
  uint32_t thread_id;
  int32_t queue_index;
  StygianCmdBuffer *buffer;
  StygianCmdQueueEpoch *slot;
  if (!ctx)
    return NULL;
  thread_id = stygian_thread_id_u32();
  queue_index = stygian_cmd_find_queue(ctx, thread_id, true);
  if (queue_index < 0) {
    stygian_context_log_error(ctx, STYGIAN_ERROR_COMMAND_BUFFER_FULL, 0u,
                              source_tag, "no command producer slot available");
    return NULL;
  }
  buffer = &ctx->cmd_buffers[queue_index];
  if (buffer->active) {
    stygian_context_log_error(ctx, STYGIAN_ERROR_INVALID_STATE, 0u, source_tag,
                              "nested command buffer begin on same thread");
    return NULL;
  }
  buffer->ctx = ctx;
  buffer->queue_index = (uint32_t)queue_index;
  buffer->epoch = ctx->cmd_publish_epoch;
  buffer->source_tag = source_tag;
  buffer->scope_id = ctx->active_scope_index >= 0
                         ? ctx->scope_cache[ctx->active_scope_index].id
                         : 0u;
  slot = &ctx->cmd_queues[queue_index].epoch[buffer->epoch];
  buffer->begin_index = slot->count;
  buffer->count = 0u;
  buffer->active = true;
  return buffer;
}

void stygian_cmd_discard(StygianCmdBuffer *buffer) {
  StygianContext *ctx;
  StygianCmdQueueEpoch *slot;
  if (!buffer || !buffer->active)
    return;
  ctx = buffer->ctx;
  if (!ctx || buffer->queue_index >= ctx->cmd_queue_count) {
    buffer->active = false;
    return;
  }
  slot = &ctx->cmd_queues[buffer->queue_index].epoch[buffer->epoch];
  if (buffer->begin_index <= slot->count) {
    slot->count = buffer->begin_index;
  }
  buffer->active = false;
  buffer->count = 0u;
}

bool stygian_cmd_submit(StygianContext *ctx, StygianCmdBuffer *buffer) {
  StygianCmdQueueEpoch *slot;
  uint64_t submit_seq;
  uint32_t i;
  if (!ctx || !buffer || !buffer->active || buffer->ctx != ctx)
    return false;
  if (ctx->cmd_committing) {
    stygian_context_log_error(ctx, STYGIAN_ERROR_INVALID_STATE, 0u,
                              buffer->source_tag,
                              "submit attempted while commit is active");
    return false;
  }
  if (buffer->queue_index >= ctx->cmd_queue_count)
    return false;
  slot = &ctx->cmd_queues[buffer->queue_index].epoch[buffer->epoch];
  if (buffer->begin_index > slot->count)
    return false;
  submit_seq = ++ctx->cmd_submit_seq_next;
  for (i = 0u; i < buffer->count; i++) {
    uint32_t idx = buffer->begin_index + i;
    if (idx >= slot->count)
      break;
    slot->records[idx].submit_seq = submit_seq;
    slot->records[idx].cmd_index = i;
  }
  buffer->active = false;
  buffer->count = 0u;
  return true;
}

static bool stygian_cmd_init_record(StygianCmdBuffer *buffer,
                                    StygianElement element,
                                    StygianCmdRecord *record) {
  uint32_t id;
  if (!buffer || !record || !buffer->ctx)
    return false;
  if (!stygian_resolve_element_slot(buffer->ctx, element, &id))
    return false;
  memset(record, 0, sizeof(*record));
  record->element_id = id;
  record->element_handle = (uint32_t)element;
  return true;
}

bool stygian_cmd_set_bounds(StygianCmdBuffer *buffer, StygianElement element,
                            float x, float y, float w, float h) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_BOUNDS;
  record.payload.bounds.x = x;
  record.payload.bounds.y = y;
  record.payload.bounds.w = w;
  record.payload.bounds.h = h;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_color(StygianCmdBuffer *buffer, StygianElement element,
                           float r, float g, float b, float a) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_COLOR;
  record.payload.color.r = r;
  record.payload.color.g = g;
  record.payload.color.b = b;
  record.payload.color.a = a;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_border(StygianCmdBuffer *buffer, StygianElement element,
                            float r, float g, float b, float a) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_BORDER;
  record.payload.color.r = r;
  record.payload.color.g = g;
  record.payload.color.b = b;
  record.payload.color.a = a;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_radius(StygianCmdBuffer *buffer, StygianElement element,
                            float tl, float tr, float br, float bl) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_RADIUS;
  record.payload.radius.tl = tl;
  record.payload.radius.tr = tr;
  record.payload.radius.br = br;
  record.payload.radius.bl = bl;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_type(StygianCmdBuffer *buffer, StygianElement element,
                          StygianType type) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_TYPE;
  record.payload.type.type = (uint32_t)type;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_visible(StygianCmdBuffer *buffer, StygianElement element,
                             bool visible) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_VISIBLE;
  record.payload.visible.visible = visible ? 1u : 0u;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_z(StygianCmdBuffer *buffer, StygianElement element,
                       float z) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_Z;
  record.payload.depth.z = z;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_texture(StygianCmdBuffer *buffer, StygianElement element,
                             StygianTexture texture, float u0, float v0,
                             float u1, float v1) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  if (texture != 0u && !stygian_resolve_texture_slot(buffer->ctx, texture, NULL, NULL))
    return false;
  record.property_id = STYGIAN_CMD_PROP_TEXTURE;
  record.payload.texture.texture = (uint32_t)texture;
  record.payload.texture.u0 = u0;
  record.payload.texture.v0 = v0;
  record.payload.texture.u1 = u1;
  record.payload.texture.v1 = v1;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_shadow(StygianCmdBuffer *buffer, StygianElement element,
                            float offset_x, float offset_y, float blur,
                            float spread, float r, float g, float b, float a) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_SHADOW;
  record.payload.shadow.offset_x = offset_x;
  record.payload.shadow.offset_y = offset_y;
  record.payload.shadow.blur = blur;
  record.payload.shadow.spread = spread;
  record.payload.shadow.r = r;
  record.payload.shadow.g = g;
  record.payload.shadow.b = b;
  record.payload.shadow.a = a;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_gradient(StygianCmdBuffer *buffer, StygianElement element,
                              float angle, float r1, float g1, float b1,
                              float a1, float r2, float g2, float b2,
                              float a2) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_GRADIENT;
  record.payload.gradient.angle = angle;
  record.payload.gradient.r1 = r1;
  record.payload.gradient.g1 = g1;
  record.payload.gradient.b1 = b1;
  record.payload.gradient.a1 = a1;
  record.payload.gradient.r2 = r2;
  record.payload.gradient.g2 = g2;
  record.payload.gradient.b2 = b2;
  record.payload.gradient.a2 = a2;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_hover(StygianCmdBuffer *buffer, StygianElement element,
                           float hover) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_HOVER;
  record.payload.scalar.value = hover;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_blend(StygianCmdBuffer *buffer, StygianElement element,
                           float blend) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_BLEND;
  record.payload.scalar.value = blend;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_blur(StygianCmdBuffer *buffer, StygianElement element,
                          float blur_radius) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_BLUR;
  record.payload.scalar.value = blur_radius;
  return stygian_cmd_append_record(buffer, &record);
}

bool stygian_cmd_set_glow(StygianCmdBuffer *buffer, StygianElement element,
                          float intensity) {
  StygianCmdRecord record;
  if (!stygian_cmd_init_record(buffer, element, &record))
    return false;
  record.property_id = STYGIAN_CMD_PROP_GLOW;
  record.payload.scalar.value = intensity;
  return stygian_cmd_append_record(buffer, &record);
}

void stygian_set_clip(StygianContext *ctx, StygianElement e, uint8_t clip_id) {
  uint32_t id;
  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;

  if (clip_id >= ctx->clip_count) {
    clip_id = 0;
  }
  // SoA write
  ctx->soa.hot[id].flags = (ctx->soa.hot[id].flags & ~STYGIAN_CLIP_MASK) |
                           ((uint32_t)clip_id << STYGIAN_CLIP_SHIFT);
  stygian_mark_soa_hot_dirty(ctx, id);
}

// ============================================================================
// Clip Stack
// ============================================================================

uint8_t stygian_clip_push(StygianContext *ctx, float x, float y, float w,
                          float h) {
  if (!ctx)
    return 0;

  if (ctx->clip_count == 0) {
    ctx->clip_count = 1;
  }

  // clip id 0 is reserved for "no clip".
  if (ctx->clip_count >= STYGIAN_MAX_CLIPS || ctx->clip_count > 0xFFu)
    return 0;

  uint8_t id = (uint8_t)ctx->clip_count;
  ctx->clips[ctx->clip_count++] = (StygianClipRect){x, y, w, h};

  if (ctx->clip_stack_top < 32) {
    ctx->clip_stack[ctx->clip_stack_top++] = id;
  }

  return id;
}

void stygian_clip_pop(StygianContext *ctx) {
  if (!ctx || ctx->clip_stack_top == 0)
    return;
  ctx->clip_stack_top--;
}

StygianElement stygian_begin_metaball_group(StygianContext *ctx) {
  StygianElement group = stygian_element(ctx);
  if (!group)
    return 0;

  stygian_set_type(ctx, group, STYGIAN_METABALL_GROUP);
  stygian_set_blend(ctx, group, 10.0f); // Default smoothness

  // Store start index of CHILDREN (next element) in reserved[0]
  // Used for auto-sizing the container later
  uint32_t id = group - 1;
  ctx->soa.appearance[id].control_points[0] = (float)ctx->element_count;
  ctx->soa.appearance[id].control_points[1] = 0.0f;
  ctx->soa.appearance[id].control_points[2] = 0.0f;
  ctx->soa.appearance[id].control_points[3] = 0.0f;
  stygian_mark_soa_appearance_dirty(ctx, id);

  return group;
}

void stygian_end_metaball_group(StygianContext *ctx, StygianElement group) {
  if (!ctx || !group)
    return;

  uint32_t group_id = group - 1;
  if (group_id >= ctx->element_count)
    return;

  // Read start_index from SoA appearance (was stored in control_points[0])
  uint32_t start_index =
      (uint32_t)ctx->soa.appearance[group_id].control_points[0];
  uint32_t current_count = ctx->element_count;

  if (current_count < start_index)
    return;

  uint32_t child_count = current_count - start_index;

  // Update child count in SoA
  ctx->soa.appearance[group_id].control_points[1] = (float)child_count;
  stygian_mark_soa_appearance_dirty(ctx, group_id);

  // Hide children
  for (uint32_t i = 0; i < child_count; i++) {
    uint32_t child_id = start_index + i;
    ctx->soa.hot[child_id].flags &= ~STYGIAN_FLAG_VISIBLE;
    stygian_mark_soa_hot_dirty(ctx, child_id);
  }

  // Auto-size container
  // Check if group size is 0 (auto-size mode)
  if (ctx->soa.hot[group_id].w == 0 || ctx->soa.hot[group_id].h == 0) {
    float min_x = 100000.0f, min_y = 100000.0f;
    float max_x = -100000.0f, max_y = -100000.0f;
    bool found = false;

    for (uint32_t i = 0; i < child_count; i++) {
      uint32_t child_id = start_index + i;
      // Read child bounds from SoA hot
      float cx = ctx->soa.hot[child_id].x;
      float cy = ctx->soa.hot[child_id].y;
      float cw = ctx->soa.hot[child_id].w;
      float ch = ctx->soa.hot[child_id].h;

      if (cx < min_x)
        min_x = cx;
      if (cy < min_y)
        min_y = cy;
      if (cx + cw > max_x)
        max_x = cx + cw;
      if (cy + ch > max_y)
        max_y = cy + ch;
      found = true;
    }

    if (found) {
      float pad = 60.0f;
      // Write to SoA hot directly
      ctx->soa.hot[group_id].x = min_x - pad;
      ctx->soa.hot[group_id].y = min_y - pad;
      ctx->soa.hot[group_id].w = (max_x - min_x) + pad * 2;
      ctx->soa.hot[group_id].h = (max_y - min_y) + pad * 2;
      stygian_mark_soa_hot_dirty(ctx, group_id);
    }
  }
}

// ============================================================================
// Convenience API (Transient)
// ============================================================================

StygianElement stygian_rect(StygianContext *ctx, float x, float y, float w,
                            float h, float r, float g, float b, float a) {
  StygianElement e = stygian_element(ctx);
  if (!e)
    return 0;

  stygian_set_bounds(ctx, e, x, y, w, h);
  stygian_set_color(ctx, e, r, g, b, a);
  stygian_set_type(ctx, e, STYGIAN_RECT);
  return e;
}

void stygian_rect_rounded(StygianContext *ctx, float x, float y, float w,
                          float h, float r, float g, float b, float a,
                          float radius) {
  StygianElement e = stygian_element(ctx);
  uint32_t id;
  if (!e)
    return;

  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;
  ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
  ctx->transient_count++;

  stygian_set_bounds(ctx, e, x, y, w, h);
  stygian_set_color(ctx, e, r, g, b, a);
  stygian_set_radius(ctx, e, radius, radius, radius, radius);
  stygian_set_type(ctx, e, STYGIAN_RECT);
}

// SDF line segment from (x1,y1) to (x2,y2) with given thickness
void stygian_line(StygianContext *ctx, float x1, float y1, float x2, float y2,
                  float thickness, float r, float g, float b, float a) {
  StygianElement e = stygian_element(ctx);
  uint32_t id;
  if (!e)
    return;

  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;
  ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
  ctx->transient_count++;

  // Compute bounding box - need large padding so consecutive segments overlap
  float pad =
      thickness + 20.0f; // Large padding ensures no gaps between segments
  float minx = (x1 < x2 ? x1 : x2) - pad;
  float miny = (y1 < y2 ? y1 : y2) - pad;
  float maxx = (x1 > x2 ? x1 : x2) + pad;
  float maxy = (y1 > y2 ? y1 : y2) + pad;

  stygian_set_bounds(ctx, e, minx, miny, maxx - minx, maxy - miny);
  stygian_set_color(ctx, e, r, g, b, a);
  stygian_set_type(ctx, e, STYGIAN_LINE);

  // Encode endpoints in Absolute World Coordinates
  // SoA write: UV → appearance, radius → appearance
  {
    ctx->soa.appearance[id].uv[0] = x1;
    ctx->soa.appearance[id].uv[1] = y1;
    ctx->soa.appearance[id].uv[2] = x2;
    ctx->soa.appearance[id].uv[3] = y2;
    ctx->soa.appearance[id].radius[0] = thickness * 0.5f;
    stygian_mark_soa_appearance_dirty(ctx, id);
  }
}

// SDF Quadratic Bezier curve: start (x1,y1), control (cx,cy), end (x2,y2)
// Renders entire curve as single element - no gaps, no subdivision
void stygian_bezier(StygianContext *ctx, float x1, float y1, float cx, float cy,
                    float x2, float y2, float thickness, float r, float g,
                    float b, float a) {
  StygianElement e = stygian_element(ctx);
  uint32_t id;
  if (!e)
    return;

  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;
  ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
  ctx->transient_count++;

  // Compute bounding box encompassing all control points + padding
  float pad = thickness + 4.0f;
  float min3f_x = (x1 < cx) ? (x1 < x2 ? x1 : x2) : (cx < x2 ? cx : x2);
  float min3f_y = (y1 < cy) ? (y1 < y2 ? y1 : y2) : (cy < y2 ? cy : y2);
  float max3f_x = (x1 > cx) ? (x1 > x2 ? x1 : x2) : (cx > x2 ? cx : x2);
  float max3f_y = (y1 > cy) ? (y1 > y2 ? y1 : y2) : (cy > y2 ? cy : y2);
  float minx = min3f_x - pad;
  float miny = min3f_y - pad;
  float maxx = max3f_x + pad;
  float maxy = max3f_y + pad;

  stygian_set_bounds(ctx, e, minx, miny, maxx - minx, maxy - miny);
  stygian_set_color(ctx, e, r, g, b, a);
  stygian_set_type(ctx, e, STYGIAN_BEZIER);

  // Pack control points in Absolute World Coordinates: UV = (start, end),
  // _reserved[0,1] = control point
  // SoA write
  {
    ctx->soa.appearance[id].uv[0] = x1;
    ctx->soa.appearance[id].uv[1] = y1;
    ctx->soa.appearance[id].uv[2] = x2;
    ctx->soa.appearance[id].uv[3] = y2;
    ctx->soa.appearance[id].control_points[0] = cx;
    ctx->soa.appearance[id].control_points[1] = cy;
    ctx->soa.appearance[id].control_points[2] = 0.0f;
    ctx->soa.appearance[id].control_points[3] = 0.0f;
    ctx->soa.appearance[id].radius[0] = thickness * 0.5f;
    stygian_mark_soa_appearance_dirty(ctx, id);
  }
}

// SDF Cubic Bezier wire: start (x1,y1), cp1 (cp1x,cp1y), cp2 (cp2x,cp2y), end
// (x2,y2) Renders entire cubic curve as a single element using SDF
// approximation
void stygian_wire(StygianContext *ctx, float x1, float y1, float cp1x,
                  float cp1y, float cp2x, float cp2y, float x2, float y2,
                  float thickness, float r, float g, float b, float a) {
  StygianElement e = stygian_element(ctx);
  uint32_t id;
  if (!e)
    return;

  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;
  ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
  ctx->transient_count++;

  // Compute bounding box encompassing all 4 control points + large padding
  float pad = thickness + 32.0f;
  float minx = x1;
  if (cp1x < minx)
    minx = cp1x;
  if (cp2x < minx)
    minx = cp2x;
  if (x2 < minx)
    minx = x2;
  minx -= pad;

  float miny = y1;
  if (cp1y < miny)
    miny = cp1y;
  if (cp2y < miny)
    miny = cp2y;
  if (y2 < miny)
    miny = y2;
  miny -= pad;

  float maxx = x1;
  if (cp1x > maxx)
    maxx = cp1x;
  if (cp2x > maxx)
    maxx = cp2x;
  if (x2 > maxx)
    maxx = x2;
  maxx += pad;

  float maxy = y1;
  if (cp1y > maxy)
    maxy = cp1y;
  if (cp2y > maxy)
    maxy = cp2y;
  if (y2 > maxy)
    maxy = y2;
  maxy += pad;

  stygian_set_bounds(ctx, e, minx, miny, maxx - minx, maxy - miny);
  stygian_set_color(ctx, e, r, g, b, a);
  stygian_set_type(ctx, e, STYGIAN_WIRE);

  // Pack control points in Absolute World Coordinates:
  // UV: (A, D), _reserved[0,1]: B, _reserved[2,3]: C
  // SoA write
  {
    ctx->soa.appearance[id].uv[0] = x1;
    ctx->soa.appearance[id].uv[1] = y1;
    ctx->soa.appearance[id].uv[2] = x2;
    ctx->soa.appearance[id].uv[3] = y2;
    ctx->soa.appearance[id].control_points[0] = cp1x;
    ctx->soa.appearance[id].control_points[1] = cp1y;
    ctx->soa.appearance[id].control_points[2] = cp2x;
    ctx->soa.appearance[id].control_points[3] = cp2y;
    ctx->soa.appearance[id].radius[0] = thickness * 0.5f;
    stygian_mark_soa_appearance_dirty(ctx, id);
  }
}

void stygian_image(StygianContext *ctx, StygianTexture tex, float x, float y,
                   float w, float h) {
  StygianElement e = stygian_element(ctx);
  uint32_t id;
  if (!e)
    return;

  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;
  ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
  ctx->transient_count++;

  stygian_set_bounds(ctx, e, x, y, w, h);
  stygian_set_color(ctx, e, 1, 1, 1, 1);
  stygian_set_texture(ctx, e, tex, 0, 0, 1, 1);
  stygian_set_type(ctx, e, STYGIAN_TEXTURE);
}

void stygian_image_uv(StygianContext *ctx, StygianTexture tex, float x, float y,
                      float w, float h, float u0, float v0, float u1,
                      float v1) {
  StygianElement e = stygian_element(ctx);
  uint32_t id;
  if (!e)
    return;

  if (!stygian_resolve_element_slot(ctx, e, &id))
    return;
  ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
  ctx->transient_count++;

  stygian_set_bounds(ctx, e, x, y, w, h);
  stygian_set_color(ctx, e, 1, 1, 1, 1);
  stygian_set_texture(ctx, e, tex, u0, v0, u1, v1);
  stygian_set_type(ctx, e, STYGIAN_TEXTURE);
}

// ============================================================================
// Utilities
// ============================================================================

void stygian_get_size(StygianContext *ctx, int *w, int *h) {
  if (!ctx)
    return;
  if (w)
    *w = ctx->width;
  if (h)
    *h = ctx->height;
}

void stygian_set_vsync(StygianContext *ctx, bool enable) {
  if (!ctx || !ctx->window)
    return;

  // Delegate to window layer
  stygian_window_set_vsync(ctx->window, enable);
}

StygianWindow *stygian_get_window(StygianContext *ctx) {
  if (!ctx)
    return NULL;
  return ctx->window;
}

// ============================================================================
// Texture API (delegates to AP)
// ============================================================================

StygianTexture stygian_texture_create(StygianContext *ctx, int w, int h,
                                      const void *rgba) {
  uint32_t backend_id;
  uint32_t slot;
  if (!ctx || !ctx->ap)
    return 0;
  backend_id = stygian_ap_texture_create(ctx->ap, w, h, rgba);
  if (backend_id == 0u)
    return 0u;
  if (ctx->texture_free_count == 0u) {
    stygian_ap_texture_destroy(ctx->ap, backend_id);
    return 0u;
  }
  slot = ctx->texture_free_list[--ctx->texture_free_count];
  ctx->texture_backend_ids[slot] = backend_id;
  ctx->texture_count++;
  return (StygianTexture)stygian_make_handle(slot, ctx->texture_generations[slot]);
}

bool stygian_texture_update(StygianContext *ctx, StygianTexture tex, int x,
                            int y, int w, int h, const void *rgba) {
  uint32_t backend_id;
  if (!ctx || !ctx->ap || !tex)
    return false;
  if (!stygian_resolve_texture_slot(ctx, tex, NULL, &backend_id))
    return false;
  return stygian_ap_texture_update(ctx->ap, backend_id, x, y, w, h, rgba);
}

void stygian_texture_destroy(StygianContext *ctx, StygianTexture tex) {
  uint32_t slot;
  uint32_t backend_id;
  if (!ctx || !ctx->ap || !tex)
    return;
  if (!stygian_resolve_texture_slot(ctx, tex, &slot, &backend_id))
    return;
  stygian_ap_texture_destroy(ctx->ap, backend_id);
  ctx->texture_backend_ids[slot] = 0u;
  ctx->texture_generations[slot] =
      stygian_bump_generation(ctx->texture_generations[slot]);
  ctx->texture_free_list[ctx->texture_free_count++] = slot;
  if (ctx->texture_count > 0u)
    ctx->texture_count--;
}

// ============================================================================
// Font API (loads MTSDF atlas via backend)
// ============================================================================

static StygianFont stygian_first_alive_font(const StygianContext *ctx) {
  uint32_t i;
  if (!ctx || !ctx->font_alive || !ctx->font_generations)
    return 0u;
  for (i = 0u; i < STYGIAN_MAX_FONTS; i++) {
    if (ctx->font_alive[i])
      return (StygianFont)stygian_make_handle(i, ctx->font_generations[i]);
  }
  return 0u;
}

StygianFont stygian_font_load(StygianContext *ctx, const char *atlas_png,
                              const char *atlas_json) {
  uint32_t font_slot;
  uint32_t tex_backend_id = 0u;
  StygianTexture tex_handle = 0u;
  StygianFontAtlas *font;
  if (!ctx || !atlas_png || !atlas_json)
    return 0;

  if (ctx->font_free_count == 0u)
    return 0;

  MTSDFAtlas mtsdf = {0};

  char resolved_png[256];
  resolve_path(atlas_png, NULL, resolved_png, sizeof(resolved_png));

  char resolved_json[256];
  resolve_path(atlas_json, NULL, resolved_json, sizeof(resolved_json));

  if (!mtsdf_load_atlas(&mtsdf, resolved_png, resolved_json)) {
    return 0;
  }

  if (ctx->glyph_color_transform_enabled && mtsdf.pixels &&
      mtsdf.atlas_width > 0 && mtsdf.atlas_height > 0) {
    size_t pixel_count = (size_t)mtsdf.atlas_width * (size_t)mtsdf.atlas_height;
    stygian_color_transform_rgba8(&ctx->glyph_source_color_profile,
                                  &ctx->output_color_profile, mtsdf.pixels,
                                  pixel_count);
  }

  // Create texture via backend (proper abstraction!)
  tex_handle = stygian_texture_create(ctx, mtsdf.atlas_width, mtsdf.atlas_height,
                                      mtsdf.pixels);
  if (!tex_handle) {
    mtsdf_free_atlas(&mtsdf);
    return 0;
  }
  if (!stygian_resolve_texture_slot(ctx, tex_handle, NULL, &tex_backend_id)) {
    stygian_texture_destroy(ctx, tex_handle);
    mtsdf_free_atlas(&mtsdf);
    return 0;
  }

  // Free raw pixels now that texture is uploaded
  if (mtsdf.pixels) {
    free(mtsdf.pixels);
    mtsdf.pixels = NULL;
  }

  // Store font data
  font_slot = ctx->font_free_list[--ctx->font_free_count];
  font = &ctx->fonts[font_slot];
  memset(font, 0, sizeof(*font));
  font->texture_handle = tex_handle;
  font->texture_backend_id = tex_backend_id;
  font->atlas_width = mtsdf.atlas_width;
  font->atlas_height = mtsdf.atlas_height;
  font->px_range = mtsdf.px_range;
  font->em_size = mtsdf.em_size;
  font->line_height = mtsdf.line_height;
  font->ascender = mtsdf.ascender;
  font->descender = mtsdf.descender;

  // Copy glyph data
  for (int i = 0; i < 256; i++) {
    font->glyphs[i].has_glyph = mtsdf.glyphs[i].has_glyph;
    font->glyphs[i].advance = mtsdf.glyphs[i].advance;
    font->glyphs[i].plane_left = mtsdf.glyphs[i].plane_left;
    font->glyphs[i].plane_bottom = mtsdf.glyphs[i].plane_bottom;
    font->glyphs[i].plane_right = mtsdf.glyphs[i].plane_right;
    font->glyphs[i].plane_top = mtsdf.glyphs[i].plane_top;
    font->glyphs[i].u0 = mtsdf.glyphs[i].u0;
    font->glyphs[i].v0 = mtsdf.glyphs[i].v0;
    font->glyphs[i].u1 = mtsdf.glyphs[i].u1;
    font->glyphs[i].v1 = mtsdf.glyphs[i].v1;
  }

  // Copy non-ASCII glyph entries into dynamic lookup table.
  if (mtsdf.glyph_count > 0 && mtsdf.glyph_entries) {
    uint32_t dyn_count = 0;
    for (int i = 0; i < mtsdf.glyph_count; i++) {
      if (mtsdf.glyph_entries[i].codepoint >= 256u)
        dyn_count++;
    }
    if (dyn_count > 0) {
      font->glyph_entries = (StygianFontGlyphEntry *)stygian_alloc_array(
          ctx->allocator, dyn_count, sizeof(StygianFontGlyphEntry),
          _Alignof(StygianFontGlyphEntry), true);
      if (!font->glyph_entries) {
        stygian_texture_destroy(ctx, tex_handle);
        ctx->font_free_list[ctx->font_free_count++] = font_slot;
        mtsdf_free_atlas(&mtsdf);
        return 0;
      }
      font->glyph_capacity = dyn_count;
      for (int i = 0; i < mtsdf.glyph_count; i++) {
        if (mtsdf.glyph_entries[i].codepoint >= 256u) {
          StygianFontGlyphEntry *dst =
              &font->glyph_entries[font->glyph_count++];
          dst->codepoint = mtsdf.glyph_entries[i].codepoint;
          dst->glyph.has_glyph = mtsdf.glyph_entries[i].glyph.has_glyph;
          dst->glyph.advance = mtsdf.glyph_entries[i].glyph.advance;
          dst->glyph.plane_left = mtsdf.glyph_entries[i].glyph.plane_left;
          dst->glyph.plane_bottom = mtsdf.glyph_entries[i].glyph.plane_bottom;
          dst->glyph.plane_right = mtsdf.glyph_entries[i].glyph.plane_right;
          dst->glyph.plane_top = mtsdf.glyph_entries[i].glyph.plane_top;
          dst->glyph.u0 = mtsdf.glyph_entries[i].glyph.u0;
          dst->glyph.v0 = mtsdf.glyph_entries[i].glyph.v0;
          dst->glyph.u1 = mtsdf.glyph_entries[i].glyph.u1;
          dst->glyph.v1 = mtsdf.glyph_entries[i].glyph.v1;
        }
      }
      if (!stygian_font_rebuild_glyph_hash(ctx, font, font->glyph_count * 2u)) {
        stygian_texture_destroy(ctx, tex_handle);
        stygian_font_free_dynamic(ctx, font);
        memset(font, 0, sizeof(*font));
        ctx->font_free_list[ctx->font_free_count++] = font_slot;
        mtsdf_free_atlas(&mtsdf);
        return 0;
      }
    }
  }

  // Copy kerning lookup table
  if (mtsdf.kerning_ready) {
    for (int a = 0; a < 256; a++) {
      for (int b = 0; b < 256; b++) {
        font->kerning_table[a][b] = mtsdf.kerning_table[a][b];
        font->kerning_has[a][b] = mtsdf.kerning_has[a][b];
      }
    }
    font->kerning_ready = true;
  } else {
    font->kerning_ready = false;
  }

  if (mtsdf.kerning && mtsdf.kerning_count > 0) {
    font->kerning_pairs = (StygianFontKernPair *)stygian_alloc_array(
        ctx->allocator, (size_t)mtsdf.kerning_count,
        sizeof(StygianFontKernPair), _Alignof(StygianFontKernPair), true);
    if (!font->kerning_pairs) {
      stygian_texture_destroy(ctx, tex_handle);
      stygian_font_free_dynamic(ctx, font);
      memset(font, 0, sizeof(*font));
      ctx->font_free_list[ctx->font_free_count++] = font_slot;
      mtsdf_free_atlas(&mtsdf);
      return 0;
    }
    font->kerning_pair_count = (uint32_t)mtsdf.kerning_count;
    for (int i = 0; i < mtsdf.kerning_count; i++) {
      font->kerning_pairs[i].left = (uint32_t)mtsdf.kerning[i].unicode1;
      font->kerning_pairs[i].right = (uint32_t)mtsdf.kerning[i].unicode2;
      font->kerning_pairs[i].advance = mtsdf.kerning[i].advance;
    }
  }

  // Bind font texture for text rendering
  stygian_ap_set_font_texture(ctx->ap, tex_backend_id, font->atlas_width,
                              font->atlas_height, font->px_range);
  ctx->font_alive[font_slot] = 1u;
  ctx->font_count++;

  mtsdf_free_atlas(&mtsdf);
  return (StygianFont)stygian_make_handle(font_slot,
                                          ctx->font_generations[font_slot]);
}

void stygian_font_destroy(StygianContext *ctx, StygianFont font) {
  uint32_t slot;
  StygianFontAtlas *f;
  if (!ctx)
    return;
  if (!stygian_resolve_font_slot(ctx, font, &slot))
    return;
  f = &ctx->fonts[slot];
  if (f->texture_handle) {
    stygian_texture_destroy(ctx, f->texture_handle);
    f->texture_handle = 0u;
    f->texture_backend_id = 0u;
  }
  stygian_font_free_dynamic(ctx, f);
  memset(f, 0, sizeof(*f));
  ctx->font_alive[slot] = 0u;
  ctx->font_generations[slot] = stygian_bump_generation(ctx->font_generations[slot]);
  ctx->font_free_list[ctx->font_free_count++] = slot;
  if (ctx->font_count > 0u)
    ctx->font_count--;
}

// ============================================================================
// Text Rendering
// ============================================================================

StygianElement stygian_text(StygianContext *ctx, StygianFont font,
                            const char *str, float x, float y, float size,
                            float r, float g, float b, float a) {
  uint32_t font_slot;
  StygianFontAtlas *f;
  if (!ctx || !str)
    return 0;

  if (font == 0) {
    font = stygian_first_alive_font(ctx);
  }
  if (!stygian_resolve_font_slot(ctx, font, &font_slot))
    return 0;
  f = &ctx->fonts[font_slot];
  size_t text_len = strlen(str);
  if (text_len == 0)
    return 0;

// Upper-bound element count: at most 1 element per byte (ASCII worst case).
// Cap to stack-friendly size; fall back to single alloc for huge strings.
#define STYGIAN_TEXT_MAX_BATCH 4096
  uint32_t max_glyphs =
      (uint32_t)(text_len < STYGIAN_TEXT_MAX_BATCH ? text_len
                                                   : STYGIAN_TEXT_MAX_BATCH);

  StygianElement batch_stack[STYGIAN_TEXT_MAX_BATCH];
  uint32_t allocated = stygian_element_batch(ctx, max_glyphs, batch_stack);
  if (allocated == 0)
    return 0;

  // Mark all batch elements as transient upfront
  for (uint32_t i = 0; i < allocated; i++) {
    uint32_t id;
    if (!stygian_resolve_element_slot(ctx, batch_stack[i], &id))
      continue;
    // SoA only
    ctx->soa.hot[id].flags |= STYGIAN_FLAG_TRANSIENT;
  }
  ctx->transient_count += allocated;

  // Fill loop — direct SoA writes, no setter calls
  uint32_t slot = 0; // next batch slot to consume
  size_t cursor = 0;
  float cursor_x = x;
  float cursor_y = y;
  StygianElement first = 0;

  for (;;) {
    if (slot >= allocated)
      break;

    size_t cp_start = cursor;
    uint32_t cp = 0;
    if (!stygian_utf8_next(str, text_len, &cursor, &cp))
      break;
    if (cp == '\r')
      continue;
    if (cp == '\n') {
      cursor_x = x;
      cursor_y += f->line_height * size;
      continue;
    }

    // Emoji shortcode inline
    if (cp == ':') {
      char emoji_id[128];
      size_t emoji_after = 0;
      uint32_t emoji_tex = 0;
      uint32_t emoji_backend_tex = 0;
      if (stygian_try_parse_shortcode(str, text_len, cp_start, emoji_id,
                                      sizeof(emoji_id), &emoji_after) &&
          stygian_inline_emoji_resolve_texture(ctx, emoji_id, &emoji_tex) &&
          emoji_tex != 0u &&
          stygian_resolve_texture_slot(ctx, emoji_tex, NULL,
                                       &emoji_backend_tex)) {
        float emoji_px = f->line_height * size;
        StygianElement e = batch_stack[slot++];
        uint32_t id;
        if (!stygian_resolve_element_slot(ctx, e, &id))
          continue;
        if (!first)
          first = e;

        // Direct SoA fill for emoji quad
        ctx->soa.hot[id].x = cursor_x;
        ctx->soa.hot[id].y = cursor_y;
        ctx->soa.hot[id].w = emoji_px;
        ctx->soa.hot[id].h = emoji_px;
        ctx->soa.hot[id].color[0] = 1.0f;
        ctx->soa.hot[id].color[1] = 1.0f;
        ctx->soa.hot[id].color[2] = 1.0f;
        ctx->soa.hot[id].color[3] = a;
        ctx->soa.hot[id].type = STYGIAN_TEXTURE;
        ctx->soa.hot[id].texture_id = emoji_backend_tex;
        stygian_mark_soa_hot_dirty(ctx, id);

        ctx->soa.appearance[id].uv[0] = 0.0f;
        ctx->soa.appearance[id].uv[1] = 0.0f;
        ctx->soa.appearance[id].uv[2] = 1.0f;
        ctx->soa.appearance[id].uv[3] = 1.0f;
        stygian_mark_soa_appearance_dirty(ctx, id);

        cursor = emoji_after;
        cursor_x += emoji_px;
        continue;
      }
    }

    const StygianFontGlyph *glyph = stygian_font_get_glyph(f, cp);
    if (!glyph && cp > 255u)
      glyph = stygian_font_get_glyph(f, (uint32_t)'?');
    if (!glyph || !glyph->has_glyph)
      continue;

    float glyph_w = (glyph->plane_right - glyph->plane_left) * size;
    float glyph_h = (glyph->plane_top - glyph->plane_bottom) * size;
    float offset_x = glyph->plane_left * size;
    float offset_y = (f->ascender - glyph->plane_top) * size;

    StygianElement e = batch_stack[slot++];
    uint32_t id;
    if (!stygian_resolve_element_slot(ctx, e, &id))
      continue;
    if (!first)
      first = e;

    // Direct SoA fill — hot
    ctx->soa.hot[id].x = cursor_x + offset_x;
    ctx->soa.hot[id].y = cursor_y + offset_y;
    ctx->soa.hot[id].w = glyph_w;
    ctx->soa.hot[id].h = glyph_h;
    ctx->soa.hot[id].color[0] = r;
    ctx->soa.hot[id].color[1] = g;
    ctx->soa.hot[id].color[2] = b;
    ctx->soa.hot[id].color[3] = a;
    ctx->soa.hot[id].type = STYGIAN_TEXT;
    ctx->soa.hot[id].texture_id = f->texture_backend_id;
    stygian_mark_soa_hot_dirty(ctx, id);

    // Direct SoA fill — appearance (glyph UVs)
    ctx->soa.appearance[id].uv[0] = glyph->u0;
    ctx->soa.appearance[id].uv[1] = glyph->v0;
    ctx->soa.appearance[id].uv[2] = glyph->u1;
    ctx->soa.appearance[id].uv[3] = glyph->v1;
    stygian_mark_soa_appearance_dirty(ctx, id);

    // Kerning lookahead
    float kern = 0.0f;
    if (cursor < text_len) {
      size_t next_pos = cursor;
      uint32_t next_cp = 0;
      if (stygian_utf8_next(str, text_len, &next_pos, &next_cp) &&
          next_cp != '\n' && next_cp != '\r') {
        kern = stygian_font_get_kerning(f, cp, next_cp);
      }
    }
    cursor_x += (glyph->advance + kern) * size;
  }

  // Free unused batch slots (we over-allocated)
  for (uint32_t i = slot; i < allocated; i++) {
    ctx->transient_count--;
    stygian_element_free(ctx, batch_stack[i]);
  }

  return first;
}

float stygian_text_width(StygianContext *ctx, StygianFont font, const char *str,
                         float size) {
  uint32_t font_slot;
  StygianFontAtlas *f;
  if (!ctx || !str)
    return 0;

  if (font == 0) {
    font = stygian_first_alive_font(ctx);
  }
  if (!stygian_resolve_font_slot(ctx, font, &font_slot))
    return 0;
  f = &ctx->fonts[font_slot];
  float width = 0.0f;
  float line_width = 0.0f;
  size_t text_len = strlen(str);
  size_t cursor = 0;

  for (;;) {
    size_t cp_start = cursor;
    uint32_t cp = 0;
    const StygianFontGlyph *glyph;
    float kern = 0.0f;
    if (!stygian_utf8_next(str, text_len, &cursor, &cp))
      break;
    if (cp == '\r')
      continue;
    if (cp == '\n') {
      if (line_width > width)
        width = line_width;
      line_width = 0.0f;
      continue;
    }

    if (cp == ':') {
      char emoji_id[128];
      size_t emoji_after = 0;
      if (stygian_try_parse_shortcode(str, text_len, cp_start, emoji_id,
                                      sizeof(emoji_id), &emoji_after) &&
          stygian_inline_emoji_has_entry(ctx, emoji_id)) {
        line_width += f->line_height * size;
        cursor = emoji_after;
        continue;
      }
    }

    glyph = stygian_font_get_glyph(f, cp);
    if (!glyph && cp > 255u)
      glyph = stygian_font_get_glyph(f, (uint32_t)'?');
    if (!glyph || !glyph->has_glyph)
      continue;
    if (cursor < text_len) {
      size_t next_pos = cursor;
      uint32_t next_cp = 0;
      if (stygian_utf8_next(str, text_len, &next_pos, &next_cp) &&
          next_cp != '\n' && next_cp != '\r') {
        kern = stygian_font_get_kerning(f, cp, next_cp);
      }
    }
    line_width += (glyph->advance + kern) * size;
  }

  if (line_width > width)
    width = line_width;
  return width;
}

// ============================================================================
// Debug Tools
// ============================================================================

void stygian_debug_overlay_draw(StygianContext *ctx) {
  if (!ctx)
    return;

  // Capture count before we start adding debug elements
  uint32_t count = ctx->element_count;

  // 1. Draw Element Bounds (Cyan)
  for (uint32_t i = 1; i <= count; i++) {
    uint32_t id = i - 1;
    uint32_t flags = ctx->soa.hot[id].flags;
    if (!(flags & STYGIAN_FLAG_VISIBLE))
      continue;

    float x = ctx->soa.hot[id].x;
    float y = ctx->soa.hot[id].y;
    float w = ctx->soa.hot[id].w;
    float h = ctx->soa.hot[id].h;

    // Filter out tiny/structural elements
    if (w < 1.0f || h < 1.0f)
      continue;

    // Filter out existing debug overlays (hacky check: if color is cyan)
    // if (el->border_color[1] == 1.0f && el->border_color[2] == 1.0f) continue;

    StygianElement dbg = stygian_rect(ctx, x, y, w, h, 0, 0, 0, 0);
    stygian_set_type(ctx, dbg, STYGIAN_RECT_OUTLINE);
    stygian_set_border(ctx, dbg, 0.0f, 1.0f, 1.0f, 0.5f); // Cyan
  }

  // 2. Draw Clip Rects (Red)
  for (int i = 1; i < ctx->clip_count; i++) {
    StygianClipRect *c = &ctx->clips[i];
    StygianElement clip_dbg =
        stygian_rect(ctx, c->x, c->y, c->w, c->h, 0, 0, 0, 0);
    stygian_set_type(ctx, clip_dbg, STYGIAN_RECT_OUTLINE);
    stygian_set_border(ctx, clip_dbg, 1.0f, 0.0f, 0.0f, 0.8f); // Red
  }
}
