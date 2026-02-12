#include "../include/stygian.h"
#include "../src/stygian_unicode.h"
#include "../tools/wavelet_bench/third_party/lz4.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_input.h"
#include "../window/stygian_window.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include <zstd.h>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "../tools/wavelet_bench/third_party/nanosvg.h"
#include "../tools/wavelet_bench/third_party/nanosvgrast.h"

#define MAX_MESSAGES 128
#define MAX_TEXT 512
#define MAX_EMOJI_CACHE 512
#define EMOJI_SIZE 96
#define MAX_MSG_EMOJI 64
#define EMOJI_ATLAS_W 2048
#define EMOJI_ATLAS_H 2048
#define SGC_MAGIC 0x31434753u
#define PERF_MAX_SAMPLES 4096

typedef struct ChatMessage {
  char text[MAX_TEXT];
  char emoji_id[256];
  bool has_emoji;
  bool emoji_failed;
  int emoji_count;
  char emoji_ids[MAX_MSG_EMOJI][64];
  uint64_t glyph_hashes[MAX_MSG_EMOJI];
} ChatMessage;

typedef struct EmojiCacheEntry {
  bool used;
  uint64_t glyph_hash;
  int slot;
  float u0, v0, u1, v1;
} EmojiCacheEntry;

typedef struct SgcHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t entry_count;
  uint32_t codec_id;
  uint64_t index_offset;
  uint64_t string_offset;
  uint64_t dict_offset;
  uint64_t dict_size;
  uint64_t data_offset;
  uint64_t file_size;
} SgcHeader;

typedef struct SgcEntry {
  uint64_t id_hash;
  uint32_t id_offset;
  uint32_t id_len;
  uint64_t data_offset;
  uint32_t comp_size;
  uint32_t raw_size;
  uint32_t flags;
  uint32_t reserved;
} SgcEntry;

typedef struct SgcPack {
  bool mounted;
  uint8_t *blob;
  size_t blob_size;
  const SgcHeader *header;
  const SgcEntry *entries;
  const char *strings;
  const uint8_t *dict_data;
  size_t dict_size;
  ZSTD_DDict *ddict;
  size_t index_cap;
  int *hash_index;
  char mounted_path[512];
} SgcPack;

typedef struct PerfSeries {
  uint64_t values[PERF_MAX_SAMPLES];
  size_t count;
  uint64_t sum_us;
} PerfSeries;

static ChatMessage g_messages[MAX_MESSAGES];
static int g_message_count = 0;
static EmojiCacheEntry g_emoji_cache[MAX_EMOJI_CACHE];
static StygianTexture g_emoji_atlas_tex = 0;
static int g_emoji_atlas_cols = 0;
static int g_emoji_atlas_rows = 0;
static int g_emoji_atlas_slots = 0;
static int g_emoji_next_slot = 0;
static char g_status_line[256] = "status: idle";
static bool g_picker_open = false;
static float g_picker_scroll_y = 0.0f;
static float g_msg_scroll_y = 0.0f;
static bool g_show_debug_widget = false;
static bool g_perf_widget_pos_init = false;
static const StygianScopeId k_scope_chat_base = 0x3001ull;
static const StygianScopeId k_scope_chat_perf =
    STYGIAN_OVERLAY_SCOPE_BASE | (StygianScopeId)0x3002ull;
static StygianPerfWidget g_perf_widget = {
    .x = 0.0f,
    .y = 0.0f,
    .w = 360.0f,
    .h = 164.0f,
    .renderer_name = "OpenGL",
    .enabled = true,
    .show_graph = true,
    .show_input = true,
    .auto_scale_graph = false,
    .history_window = 120u,
    .idle_hz = 30u,
    .active_hz = 30u,
    .text_hz = 5u,
    .max_stress_hz = 120u,
    .stress_mode = false,
    .compact_mode = true,
    .show_memory = true,
    .show_glyphs = true,
    .show_triad = true,
};
static SgcPack g_sgc = {0};
static uint64_t g_startup_mount_us = 0;
static PerfSeries g_lookup_perf = {0};
static PerfSeries g_decode_perf = {0};
static PerfSeries g_upload_perf = {0};

typedef struct EmojiPickerEntry {
  const char *id;
  const char *label;
} EmojiPickerEntry;

static const EmojiPickerEntry k_picker_entries[] = {
    {"emoji_u1f600", "1f600"}, {"emoji_u1f602", "1f602"},
    {"emoji_u1f603", "1f603"}, {"emoji_u1f604", "1f604"},
    {"emoji_u1f60a", "1f60a"}, {"emoji_u1f60d", "1f60d"},
    {"emoji_u1f618", "1f618"}, {"emoji_u1f621", "1f621"},
    {"emoji_u1f622", "1f622"}, {"emoji_u1f62d", "1f62d"},
    {"emoji_u1f44d", "1f44d"}, {"emoji_u1f44e", "1f44e"},
    {"emoji_u1f44f", "1f44f"}, {"emoji_u1f525", "1f525"},
    {"emoji_u1f4a8", "1f4a8"}, {"emoji_u1f4af", "1f4af"},
    {"emoji_u1f389", "1f389"}, {"emoji_u1f680", "1f680"},
    {"emoji_u1f64c", "1f64c"}, {"emoji_u1f914", "1f914"},
    {"emoji_u1f923", "1f923"}, {"emoji_u1f970", "1f970"},
    {"emoji_u1f973", "1f973"}, {"emoji_u1fae0", "1fae0"},
};

static const char *k_sgc_paths[] = {"assets/sgc/emoji_zstd_train.sgc",
                                    "../assets/sgc/emoji_zstd_train.sgc",
                                    "assets/sgc/emoji_zstd9.sgc",
                                    "../assets/sgc/emoji_zstd9.sgc",
                                    "assets/sgc/emoji_zlib6.sgc",
                                    "../assets/sgc/emoji_zlib6.sgc",
                                    "assets/sgc/emoji_lz4.sgc",
                                    "../assets/sgc/emoji_lz4.sgc",
                                    NULL};

static uint64_t fnv1a64(const char *s) {
  uint64_t h = 1469598103934665603ull;
  while (*s) {
    h ^= (uint64_t)(uint8_t)(*s++);
    h *= 1099511628211ull;
  }
  return h;
}

static uint64_t now_us(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)(ts.tv_nsec / 1000ull);
}

static void perf_record(PerfSeries *p, uint64_t us) {
  if (!p)
    return;
  if (p->count < PERF_MAX_SAMPLES) {
    p->values[p->count++] = us;
  } else {
    size_t slot = p->count % PERF_MAX_SAMPLES;
    p->sum_us -= p->values[slot];
    p->values[slot] = us;
    p->count++;
  }
  p->sum_us += us;
}

static int cmp_u64(const void *a, const void *b) {
  const uint64_t aa = *(const uint64_t *)a;
  const uint64_t bb = *(const uint64_t *)b;
  if (aa < bb)
    return -1;
  if (aa > bb)
    return 1;
  return 0;
}

static double perf_avg_ms(const PerfSeries *p) {
  size_t n = 0;
  if (!p)
    return 0.0;
  n = p->count < PERF_MAX_SAMPLES ? p->count : PERF_MAX_SAMPLES;
  if (n == 0)
    return 0.0;
  return ((double)p->sum_us / (double)n) / 1000.0;
}

static double perf_p95_ms(const PerfSeries *p) {
  uint64_t tmp[PERF_MAX_SAMPLES];
  size_t i, n = 0, idx;
  if (!p)
    return 0.0;
  n = p->count < PERF_MAX_SAMPLES ? p->count : PERF_MAX_SAMPLES;
  if (n == 0)
    return 0.0;
  for (i = 0; i < n; ++i)
    tmp[i] = p->values[i];
  qsort(tmp, n, sizeof(uint64_t), cmp_u64);
  idx = (n * 95u) / 100u;
  if (idx >= n)
    idx = n - 1;
  return (double)tmp[idx] / 1000.0;
}

static int extract_shortcodes(const char *line, char out_ids[][256],
                              int max_ids) {
  int count = 0;
  const char *p = line;
  while (*p && count < max_ids) {
    const char *s = strchr(p, ':');
    const char *e;
    size_t n;
    if (!s)
      break;
    e = strchr(s + 1, ':');
    if (!e)
      break;
    if (e == s + 1) {
      p = e + 1;
      continue;
    }
    n = (size_t)(e - (s + 1));
    if (n > 0 && n < 255) {
      memcpy(out_ids[count], s + 1, n);
      out_ids[count][n] = '\0';
      count++;
    }
    p = e + 1;
  }
  return count;
}

static int sgc_codec_is_supported(uint32_t codec_id) { return codec_id <= 5u; }

static bool sgc_mount_one(const char *path) {
  FILE *f;
  long sz;
  size_t i;
  uint8_t *blob;
  const SgcHeader *h;
  size_t cap = 1;
  if (!path)
    return false;
  f = fopen(path, "rb");
  if (!f)
    return false;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  sz = ftell(f);
  if (sz <= 0) {
    fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }
  blob = (uint8_t *)malloc((size_t)sz);
  if (!blob) {
    fclose(f);
    return false;
  }
  if (fread(blob, 1, (size_t)sz, f) != (size_t)sz) {
    fclose(f);
    free(blob);
    return false;
  }
  fclose(f);
  h = (const SgcHeader *)blob;
  if (h->magic != SGC_MAGIC || h->version != 2u || h->entry_count == 0u ||
      h->file_size != (uint64_t)sz) {
    free(blob);
    return false;
  }
  if (!sgc_codec_is_supported(h->codec_id)) {
    free(blob);
    return false;
  }
  while (cap < (size_t)h->entry_count * 2u)
    cap <<= 1;
  g_sgc.hash_index = (int *)malloc(cap * sizeof(int));
  if (!g_sgc.hash_index) {
    free(blob);
    return false;
  }
  for (i = 0; i < cap; ++i)
    g_sgc.hash_index[i] = -1;

  g_sgc.mounted = true;
  g_sgc.blob = blob;
  g_sgc.blob_size = (size_t)sz;
  g_sgc.header = h;
  g_sgc.entries = (const SgcEntry *)(blob + h->index_offset);
  g_sgc.strings = (const char *)(blob + h->string_offset);
  g_sgc.dict_data =
      (h->dict_size > 0u) ? (const uint8_t *)(blob + h->dict_offset) : NULL;
  g_sgc.dict_size = (size_t)h->dict_size;
  if (h->codec_id == 5u && g_sgc.dict_data && g_sgc.dict_size > 0u) {
    g_sgc.ddict = ZSTD_createDDict(g_sgc.dict_data, g_sgc.dict_size);
    if (!g_sgc.ddict) {
      free(g_sgc.hash_index);
      free(blob);
      memset(&g_sgc, 0, sizeof(g_sgc));
      return false;
    }
  }
  g_sgc.index_cap = cap;
  strncpy(g_sgc.mounted_path, path, sizeof(g_sgc.mounted_path) - 1);
  g_sgc.mounted_path[sizeof(g_sgc.mounted_path) - 1] = '\0';

  for (i = 0; i < h->entry_count; ++i) {
    size_t slot = (size_t)(g_sgc.entries[i].id_hash & (uint64_t)(cap - 1));
    while (g_sgc.hash_index[slot] != -1) {
      slot = (slot + 1) & (cap - 1);
    }
    g_sgc.hash_index[slot] = (int)i;
  }
  printf("[chat_emoji_demo] sgc mount ok: %s entries=%u codec=%u\n", path,
         h->entry_count, h->codec_id);
  return true;
}

static bool sgc_mount_first(void) {
  int i;
  for (i = 0; k_sgc_paths[i] != NULL; ++i) {
    if (sgc_mount_one(k_sgc_paths[i]))
      return true;
  }
  return false;
}

static const SgcEntry *sgc_lookup_id(const char *id, uint64_t hash) {
  size_t slot;
  size_t cap;
  if (!g_sgc.mounted || !g_sgc.hash_index || !id)
    return NULL;
  cap = g_sgc.index_cap;
  slot = (size_t)(hash & (uint64_t)(cap - 1));
  for (;;) {
    int ei = g_sgc.hash_index[slot];
    if (ei < 0)
      return NULL;
    if ((uint64_t)g_sgc.entries[ei].id_hash == hash) {
      const char *sid = g_sgc.strings + g_sgc.entries[ei].id_offset;
      if (strcmp(sid, id) == 0)
        return &g_sgc.entries[ei];
    }
    slot = (slot + 1) & (cap - 1);
  }
}

static uint8_t *sgc_decompress_svg(const SgcEntry *e, size_t *out_sz) {
  const uint8_t *src;
  uint8_t *dst;
  if (!g_sgc.mounted || !e || !out_sz)
    return NULL;
  src = g_sgc.blob + e->data_offset;
  dst = (uint8_t *)malloc((size_t)e->raw_size + 1u);
  if (!dst)
    return NULL;
  if (!sgc_codec_is_supported(g_sgc.header->codec_id)) {
    free(dst);
    return NULL;
  }
  if (g_sgc.header->codec_id == 0u) {
    if (e->comp_size != e->raw_size) {
      free(dst);
      return NULL;
    }
    memcpy(dst, src, e->raw_size);
  } else if (g_sgc.header->codec_id == 1u) {
    int n = LZ4_decompress_safe((const char *)src, (char *)dst,
                                (int)e->comp_size, (int)e->raw_size);
    if (n != (int)e->raw_size) {
      free(dst);
      return NULL;
    }
  } else if (g_sgc.header->codec_id == 2u) {
    uLongf got = (uLongf)e->raw_size;
    int rc =
        uncompress((Bytef *)dst, &got, (const Bytef *)src, (uLong)e->comp_size);
    if (rc != Z_OK || got != (uLongf)e->raw_size) {
      free(dst);
      return NULL;
    }
  } else if (g_sgc.header->codec_id == 3u || g_sgc.header->codec_id == 4u) {
    size_t got =
        ZSTD_decompress(dst, (size_t)e->raw_size, src, (size_t)e->comp_size);
    if (ZSTD_isError(got) || got != (size_t)e->raw_size) {
      free(dst);
      return NULL;
    }
  } else if (g_sgc.header->codec_id == 5u) {
    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    size_t got;
    if (!dctx || !g_sgc.ddict) {
      ZSTD_freeDCtx(dctx);
      free(dst);
      return NULL;
    }
    got = ZSTD_decompress_usingDDict(dctx, dst, (size_t)e->raw_size, src,
                                     (size_t)e->comp_size, g_sgc.ddict);
    ZSTD_freeDCtx(dctx);
    if (ZSTD_isError(got) || got != (size_t)e->raw_size) {
      free(dst);
      return NULL;
    }
  } else {
    free(dst);
    return NULL;
  }
  dst[e->raw_size] = 0;
  *out_sz = e->raw_size;
  return dst;
}

static void append_shortcode_token(char *buffer, int buffer_size,
                                   const char *emoji_id) {
  char token[300];
  size_t cur, add, maxn;
  if (!buffer || buffer_size <= 1 || !emoji_id || !emoji_id[0])
    return;
  snprintf(token, sizeof(token), ":%s:", emoji_id);
  cur = strlen(buffer);
  add = strlen(token);
  maxn = (size_t)buffer_size - 1u;
  if (cur >= maxn)
    return;
  if (cur + add > maxn)
    add = maxn - cur;
  memcpy(buffer + cur, token, add);
  buffer[cur + add] = '\0';
}

static bool emoji_atlas_init(StygianContext *ctx) {
  uint8_t *blank = NULL;
  if (g_emoji_atlas_tex)
    return true;
  g_emoji_atlas_cols = EMOJI_ATLAS_W / EMOJI_SIZE;
  g_emoji_atlas_rows = EMOJI_ATLAS_H / EMOJI_SIZE;
  g_emoji_atlas_slots = g_emoji_atlas_cols * g_emoji_atlas_rows;
  if (g_emoji_atlas_slots <= 0)
    return false;
  blank =
      (uint8_t *)calloc((size_t)EMOJI_ATLAS_W * (size_t)EMOJI_ATLAS_H * 4u, 1u);
  if (!blank)
    return false;
  g_emoji_atlas_tex =
      stygian_texture_create(ctx, EMOJI_ATLAS_W, EMOJI_ATLAS_H, blank);
  free(blank);
  return g_emoji_atlas_tex != 0;
}

static EmojiCacheEntry *cache_find(uint64_t glyph_hash) {
  int i;
  for (i = 0; i < MAX_EMOJI_CACHE; i++) {
    if (g_emoji_cache[i].used && g_emoji_cache[i].glyph_hash == glyph_hash)
      return &g_emoji_cache[i];
  }
  return NULL;
}

static bool chat_has_pending_emoji_decode(void) {
  int mi, ei;
  for (mi = 0; mi < g_message_count; ++mi) {
    const ChatMessage *m = &g_messages[mi];
    if (!m->has_emoji || m->emoji_failed)
      continue;
    for (ei = 0; ei < m->emoji_count; ++ei) {
      if (m->glyph_hashes[ei] != 0u &&
          cache_find(m->glyph_hashes[ei]) == NULL) {
        return true;
      }
    }
  }
  return false;
}

static void cache_evict_slot(int slot) {
  int i;
  for (i = 0; i < MAX_EMOJI_CACHE; i++) {
    if (g_emoji_cache[i].used && g_emoji_cache[i].slot == slot) {
      g_emoji_cache[i].used = false;
    }
  }
}

static EmojiCacheEntry *cache_alloc_entry(void) {
  int i;
  for (i = 0; i < MAX_EMOJI_CACHE; i++) {
    if (!g_emoji_cache[i].used)
      return &g_emoji_cache[i];
  }
  return NULL;
}

static bool load_emoji_texture(StygianContext *ctx, const char *emoji_id,
                               uint64_t glyph_hash) {
  NSVGimage *image = NULL;
  NSVGrasterizer *rast = NULL;
  uint8_t *rgba = NULL;
  uint8_t *svg_text = NULL;
  size_t svg_size = 0;
  const SgcEntry *ent;
  EmojiCacheEntry *dst = NULL;
  int slot, slot_x, slot_y;
  float inv_w, inv_h;
  float sx, sy, scale;
  char norm[128];
  uint64_t t_lookup0, t_lookup1;
  uint64_t t_decode0, t_decode1;
  uint64_t t_upload0, t_upload1;

  if (!emoji_atlas_init(ctx))
    return false;

  t_lookup0 = now_us();
  if (!stygian_shortcode_normalize(emoji_id, norm, sizeof(norm))) {
    snprintf(g_status_line, sizeof(g_status_line), "status: invalid id %s",
             emoji_id ? emoji_id : "(null)");
    return false;
  }
  ent = sgc_lookup_id(norm, fnv1a64(norm));
  t_lookup1 = now_us();
  perf_record(&g_lookup_perf, t_lookup1 - t_lookup0);
  if (!ent) {
    snprintf(g_status_line, sizeof(g_status_line), "status: sgc miss for %s",
             emoji_id ? emoji_id : "(null)");
    return false;
  }

  t_decode0 = now_us();
  svg_text = sgc_decompress_svg(ent, &svg_size);
  (void)svg_size;
  if (!svg_text) {
    t_decode1 = now_us();
    perf_record(&g_decode_perf, t_decode1 - t_decode0);
    snprintf(g_status_line, sizeof(g_status_line), "status: sgc decode fail");
    return false;
  }

  image = nsvgParse((char *)svg_text, "px", 96.0f);
  if (!image) {
    t_decode1 = now_us();
    perf_record(&g_decode_perf, t_decode1 - t_decode0);
    free(svg_text);
    return false;
  }

  rgba = (uint8_t *)calloc((size_t)EMOJI_SIZE * (size_t)EMOJI_SIZE * 4u, 1u);
  if (!rgba) {
    t_decode1 = now_us();
    perf_record(&g_decode_perf, t_decode1 - t_decode0);
    nsvgDelete(image);
    return false;
  }

  rast = nsvgCreateRasterizer();
  if (!rast) {
    t_decode1 = now_us();
    perf_record(&g_decode_perf, t_decode1 - t_decode0);
    free(rgba);
    nsvgDelete(image);
    return false;
  }

  sx = (float)EMOJI_SIZE / image->width;
  sy = (float)EMOJI_SIZE / image->height;
  scale = sx < sy ? sx : sy;
  nsvgRasterize(rast, image, 0.0f, 0.0f, scale, rgba, EMOJI_SIZE, EMOJI_SIZE,
                EMOJI_SIZE * 4);
  t_decode1 = now_us();
  perf_record(&g_decode_perf, t_decode1 - t_decode0);

  if (g_emoji_atlas_slots <= 0) {
    nsvgDeleteRasterizer(rast);
    free(rgba);
    nsvgDelete(image);
    free(svg_text);
    return false;
  }

  slot = g_emoji_next_slot % g_emoji_atlas_slots;
  g_emoji_next_slot++;
  cache_evict_slot(slot);
  dst = cache_alloc_entry();
  if (!dst) {
    cache_evict_slot(0);
    dst = cache_alloc_entry();
    if (!dst) {
      nsvgDeleteRasterizer(rast);
      free(rgba);
      nsvgDelete(image);
      free(svg_text);
      return false;
    }
  }

  slot_x = (slot % g_emoji_atlas_cols) * EMOJI_SIZE;
  slot_y = (slot / g_emoji_atlas_cols) * EMOJI_SIZE;

  t_upload0 = now_us();
  if (!stygian_texture_update(ctx, g_emoji_atlas_tex, slot_x, slot_y,
                              EMOJI_SIZE, EMOJI_SIZE, rgba)) {
    nsvgDeleteRasterizer(rast);
    free(rgba);
    nsvgDelete(image);
    free(svg_text);
    return false;
  }
  t_upload1 = now_us();
  perf_record(&g_upload_perf, t_upload1 - t_upload0);
  printf("[chat_emoji_demo] sgc atlas id=%s hash=%llu slot=%d lookup=%.3fms "
         "decode=%.3fms upload=%.3fms\n",
         emoji_id ? emoji_id : "(null)", (unsigned long long)glyph_hash, slot,
         (double)(t_lookup1 - t_lookup0) / 1000.0,
         (double)(t_decode1 - t_decode0) / 1000.0,
         (double)(t_upload1 - t_upload0) / 1000.0);
  snprintf(g_status_line, sizeof(g_status_line), "status: sgc atlas slot=%d",
           slot);

  inv_w = 1.0f / (float)EMOJI_ATLAS_W;
  inv_h = 1.0f / (float)EMOJI_ATLAS_H;
  dst->used = true;
  dst->glyph_hash = glyph_hash;
  dst->slot = slot;
  dst->u0 = (float)slot_x * inv_w;
  dst->v0 = (float)slot_y * inv_h;
  dst->u1 = (float)(slot_x + EMOJI_SIZE) * inv_w;
  dst->v1 = (float)(slot_y + EMOJI_SIZE) * inv_h;

  nsvgDeleteRasterizer(rast);
  free(rgba);
  nsvgDelete(image);
  free(svg_text);
  return true;
}

static void push_message(const char *line) {
  int k;
  if (g_message_count < MAX_MESSAGES) {
    ChatMessage *m = &g_messages[g_message_count++];
    strncpy(m->text, line, sizeof(m->text) - 1);
    m->text[sizeof(m->text) - 1] = '\0';
    m->emoji_id[0] = '\0';
    m->has_emoji = false;
    m->emoji_failed = false;
    m->emoji_count = 0;
    for (k = 0; k < MAX_MSG_EMOJI; k++) {
      m->emoji_ids[k][0] = '\0';
      m->glyph_hashes[k] = 0;
    }
    return;
  }
  memmove(&g_messages[0], &g_messages[1],
          sizeof(ChatMessage) * (MAX_MESSAGES - 1));
  strncpy(g_messages[MAX_MESSAGES - 1].text, line,
          sizeof(g_messages[MAX_MESSAGES - 1].text) - 1);
  g_messages[MAX_MESSAGES - 1]
      .text[sizeof(g_messages[MAX_MESSAGES - 1].text) - 1] = '\0';
  g_messages[MAX_MESSAGES - 1].emoji_id[0] = '\0';
  g_messages[MAX_MESSAGES - 1].has_emoji = false;
  g_messages[MAX_MESSAGES - 1].emoji_failed = false;
  g_messages[MAX_MESSAGES - 1].emoji_count = 0;
  for (k = 0; k < MAX_MSG_EMOJI; k++) {
    g_messages[MAX_MESSAGES - 1].emoji_ids[k][0] = '\0';
    g_messages[MAX_MESSAGES - 1].glyph_hashes[k] = 0;
  }
}

int main(void) {
  StygianWindow *win;
  StygianConfig cfg = {0};
  StygianContext *ctx;
  StygianFont font = 0;
  char input[MAX_TEXT] = {0};
  bool prev_enter = false;
  int i;
  uint64_t t_mount0, t_mount1;
  uint64_t last_perf_log_us = 0;

  win = stygian_window_create_simple(1120, 760, "Stygian Chat Emoji Demo");
  if (!win)
    return 1;

  cfg.backend = STYGIAN_BACKEND_OPENGL;
  cfg.window = win;
  cfg.max_elements = 65536;
  ctx = stygian_create(&cfg);
  if (!ctx) {
    stygian_window_destroy(win);
    return 1;
  }

  if (!emoji_atlas_init(ctx)) {
    snprintf(g_status_line, sizeof(g_status_line), "status: atlas init failed");
    printf("[chat_emoji_demo] emoji atlas init failed\n");
  }

  t_mount0 = now_us();
  if (!sgc_mount_first()) {
    t_mount1 = now_us();
    g_startup_mount_us = t_mount1 - t_mount0;
    snprintf(g_status_line, sizeof(g_status_line), "status: no .sgc found");
    printf("[chat_emoji_demo] no .sgc file found\n");
  } else {
    t_mount1 = now_us();
    g_startup_mount_us = t_mount1 - t_mount0;
    snprintf(g_status_line, sizeof(g_status_line),
             "status: sgc mounted (%u entries)",
             g_sgc.header ? g_sgc.header->entry_count : 0u);
  }
  printf("[chat_emoji_demo] startup mount=%.3fms\n",
         (double)g_startup_mount_us / 1000.0);
  last_perf_log_us = now_us();

  while (!stygian_window_should_close(win)) {
    StygianEvent ev;
    int w, h;
    int y;
    int decode_budget = 4;
    bool event_mutated = false;
    bool event_requested = false;
    bool event_eval = false;
    bool ui_state_changed = false;
    bool pending_decode = false;
    static bool first_frame = true;
    uint32_t wait_ms = stygian_next_repaint_wait_ms(ctx, 250u);
    bool repaint_pending = stygian_has_pending_repaint(ctx);
    bool enter_down;
    bool enter_pressed = false;
    int mx = 0, my = 0;

    stygian_widgets_begin_frame(ctx);
    while (stygian_window_poll_event(win, &ev)) {
      StygianWidgetEventImpact impact =
          stygian_widgets_process_event_ex(ctx, &ev);
      if (impact & STYGIAN_IMPACT_MUTATED_STATE)
        event_mutated = true;
      if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
        event_requested = true;
      if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
        event_eval = true;
      if (ev.type == STYGIAN_EVENT_KEY_DOWN &&
          ev.key.key == STYGIAN_KEY_ENTER) {
        enter_pressed = true;
      }
    }

    pending_decode = chat_has_pending_emoji_decode();
    if (pending_decode) {
      stygian_set_repaint_source(ctx, "decode");
      stygian_request_repaint_hz(ctx, 60u);
    }
    if (!event_mutated && !event_requested && !event_eval && !pending_decode &&
        !first_frame) {
      if (stygian_window_wait_event_timeout(win, &ev, wait_ms)) {
        StygianWidgetEventImpact impact =
            stygian_widgets_process_event_ex(ctx, &ev);
        if (impact & STYGIAN_IMPACT_MUTATED_STATE)
          event_mutated = true;
        if (impact & STYGIAN_IMPACT_REQUEST_REPAINT)
          event_requested = true;
        if (impact & STYGIAN_IMPACT_REQUEST_EVAL)
          event_eval = true;
        if (ev.type == STYGIAN_EVENT_KEY_DOWN &&
            ev.key.key == STYGIAN_KEY_ENTER) {
          enter_pressed = true;
        }
        while (stygian_window_poll_event(win, &ev)) {
          StygianWidgetEventImpact queued_impact =
              stygian_widgets_process_event_ex(ctx, &ev);
          if (queued_impact & STYGIAN_IMPACT_MUTATED_STATE)
            event_mutated = true;
          if (queued_impact & STYGIAN_IMPACT_REQUEST_REPAINT)
            event_requested = true;
          if (queued_impact & STYGIAN_IMPACT_REQUEST_EVAL)
            event_eval = true;
          if (ev.type == STYGIAN_EVENT_KEY_DOWN &&
              ev.key.key == STYGIAN_KEY_ENTER) {
            enter_pressed = true;
          }
        }
      }
    }

    repaint_pending = stygian_has_pending_repaint(ctx);
    {
      bool render_frame =
          first_frame || event_mutated || pending_decode || repaint_pending;
      bool eval_only_frame = (!render_frame && (event_eval || event_requested));
      if (!render_frame && !eval_only_frame) {
        continue;
      }
      first_frame = false;

      stygian_mouse_pos(win, &mx, &my);
      {
        uint64_t now = now_us();
        if ((now - last_perf_log_us) >= 10000000ull) {
          printf("[chat_emoji_demo] perf loop lookup(avg/p95)=%.3f/%.3fms "
                 "decode(avg/p95)=%.3f/%.3fms upload(avg/p95)=%.3f/%.3fms "
                 "frame(draw=%u upload=%uB/%ur cpu=%.2f/%.2f/%.2f repaint=%s)\n",
                 perf_avg_ms(&g_lookup_perf), perf_p95_ms(&g_lookup_perf),
                 perf_avg_ms(&g_decode_perf), perf_p95_ms(&g_decode_perf),
                 perf_avg_ms(&g_upload_perf), perf_p95_ms(&g_upload_perf),
                 stygian_get_last_frame_draw_calls(ctx),
                 stygian_get_last_frame_upload_bytes(ctx),
                 stygian_get_last_frame_upload_ranges(ctx),
                 stygian_get_last_frame_build_ms(ctx),
                 stygian_get_last_frame_submit_ms(ctx),
                 stygian_get_last_frame_present_ms(ctx),
                 stygian_get_repaint_source(ctx));
          last_perf_log_us = now;
        }
      }

      stygian_window_get_size(win, &w, &h);
      if (pending_decode) {
        stygian_scope_invalidate_now(ctx, k_scope_chat_base);
        stygian_set_repaint_source(ctx, "decode");
      }
      if (!eval_only_frame && g_show_debug_widget &&
          (repaint_pending || pending_decode || event_requested)) {
        stygian_scope_invalidate_now(ctx, k_scope_chat_perf);
      }
      stygian_begin_frame_intent(
          ctx, w, h,
          eval_only_frame ? STYGIAN_FRAME_EVAL_ONLY : STYGIAN_FRAME_RENDER);
      stygian_scope_begin(ctx, k_scope_chat_base);

      stygian_rect(ctx, 0, 0, (float)w, (float)h, 0.08f, 0.08f, 0.09f, 1.0f);
      stygian_rect_rounded(ctx, 24, 24, (float)w - 48, (float)h - 120, 0.12f,
                           0.12f, 0.13f, 1.0f, 8.0f);
      stygian_rect_rounded(ctx, 24, (float)h - 84, (float)w - 48, 56, 0.14f,
                           0.14f, 0.16f, 1.0f, 8.0f);
    }

    {
      char perf_line[256];
      stygian_text(ctx, font, "SGC emoji source mode", 32, 30, 16, 0.85f, 0.9f,
                   0.95f, 1.0f);
      stygian_text(ctx, font, "Type :emoji_u1f301: then press Enter", 32, 52,
                   14, 0.7f, 0.75f, 0.8f, 1.0f);
      stygian_text(ctx, font, g_status_line, 32, 70, 12, 0.7f, 0.85f, 0.7f,
                   1.0f);
      snprintf(perf_line, sizeof(perf_line),
               "perf: mount=%.2fms lookup(avg)=%.3fms decode(avg)=%.3fms "
               "upload(avg)=%.3fms",
               (double)g_startup_mount_us / 1000.0, perf_avg_ms(&g_lookup_perf),
               perf_avg_ms(&g_decode_perf), perf_avg_ms(&g_upload_perf));
      stygian_text(ctx, font, perf_line, 32, 84, 12, 0.75f, 0.76f, 0.82f, 1.0f);
    }

    {
      float msg_x = 36.0f;
      float msg_y = 102.0f;
      float msg_w = (float)w - 72.0f;
      float msg_h = (float)h - 188.0f;
      float content_h = (float)g_message_count * 56.0f + 8.0f;
      float draw_w = msg_w - 10.0f;
      float wheel_dy = stygian_widgets_scroll_dy();
      float max_scroll;
      float prev_msg_scroll_y = g_msg_scroll_y;
      bool picker_covers_mouse = false;

      if (msg_h < 40.0f)
        msg_h = 40.0f;
      if (draw_w < 100.0f)
        draw_w = msg_w;
      max_scroll = content_h - msg_h;
      if (max_scroll < 0.0f)
        max_scroll = 0.0f;
      if (max_scroll > 0.0f) {
        stygian_widgets_register_region(msg_x, msg_y, msg_w, msg_h,
                                        STYGIAN_WIDGET_REGION_SCROLL);
      }
      if (g_picker_open) {
        float panel_w = 316.0f;
        float panel_h = 292.0f;
        float panel_x = (float)w - panel_w - 24.0f;
        float panel_y = (float)h - panel_h - 124.0f;
        stygian_widgets_register_region(
            0.0f, 0.0f, (float)w, (float)h,
            STYGIAN_WIDGET_REGION_POINTER_LEFT_MUTATES);
        stygian_widgets_register_region(panel_x, panel_y, panel_w, panel_h,
                                        STYGIAN_WIDGET_REGION_POINTER_LEFT);
        picker_covers_mouse =
            (mx >= (int)panel_x && mx <= (int)(panel_x + panel_w) &&
             my >= (int)panel_y && my <= (int)(panel_y + panel_h));
      }
      if (max_scroll > 0.0f && !picker_covers_mouse && wheel_dy != 0.0f &&
          mx >= (int)msg_x && mx <= (int)(msg_x + msg_w) && my >= (int)msg_y &&
          my <= (int)(msg_y + msg_h)) {
        g_msg_scroll_y -= wheel_dy * 24.0f;
        if (g_msg_scroll_y < 0.0f)
          g_msg_scroll_y = 0.0f;
        if (g_msg_scroll_y > max_scroll)
          g_msg_scroll_y = max_scroll;
      }
      stygian_clip_push(ctx, msg_x, msg_y, draw_w, msg_h);
      y = (int)(msg_y + 4.0f - g_msg_scroll_y);
      for (i = 0; i < g_message_count; i++) {
        float row_y = (float)y;
        if (row_y + 50.0f < msg_y) {
          y += 56;
          continue;
        }
        if (row_y > msg_y + msg_h) {
          break;
        }
        ChatMessage *m = &g_messages[i];
        stygian_rect_rounded(ctx, msg_x, row_y, draw_w - 2.0f, 50, 0.16f, 0.16f,
                             0.18f, 1.0f, 6.0f);
        if (m->has_emoji && !m->emoji_failed) {
          int ei;
          for (ei = 0; ei < m->emoji_count; ei++) {
            float ex = msg_x + 12.0f + (float)ei * 44.0f;
            EmojiCacheEntry *ce = cache_find(m->glyph_hashes[ei]);
            if (!ce && decode_budget > 0) {
              if (!load_emoji_texture(ctx, m->emoji_ids[ei],
                                      m->glyph_hashes[ei])) {
                m->emoji_failed = true;
              }
              decode_budget--;
              ce = cache_find(m->glyph_hashes[ei]);
            }
            if (ce && g_emoji_atlas_tex) {
              stygian_image_uv(ctx, g_emoji_atlas_tex, ex, row_y + 4, 42, 42,
                               ce->u0, ce->v0, ce->u1, ce->v1);
            }
          }
        }
        stygian_text(
            ctx, font, m->text,
            msg_x + 62.0f +
                (float)(m->emoji_count > 0 ? (m->emoji_count - 1) * 44 : 0),
            row_y + 14, 16, 0.92f, 0.92f, 0.95f, 1.0f);
        y += 56;
      }
      stygian_clip_pop(ctx);
      if (stygian_scrollbar_v(ctx, msg_x + msg_w - 8.0f, msg_y + 2.0f, 6.0f,
                              msg_h - 4.0f, content_h, &g_msg_scroll_y)) {
        ui_state_changed = true;
      }
      if (g_msg_scroll_y != prev_msg_scroll_y) {
        ui_state_changed = true;
      }
    }

    {
      float input_x = 36.0f;
      float input_y = (float)h - 72.0f;
      float picker_button_w = 34.0f;
      float debug_button_w = 64.0f;
      float gap = 8.0f;
      float input_w =
          (float)w - 72.0f - picker_button_w - debug_button_w - gap * 2.0f;
      float picker_button_x = input_x + input_w + gap;
      float debug_button_x = picker_button_x + picker_button_w + gap;
      int pr, pc;
      const int cols = 4;
      const int count =
          (int)(sizeof(k_picker_entries) / sizeof(k_picker_entries[0]));

      stygian_text_input(ctx, font, input_x, input_y, input_w, 32, input,
                         (int)sizeof(input));
      if (stygian_button(ctx, font, "+", picker_button_x, input_y,
                         picker_button_w, 32)) {
        g_picker_open = !g_picker_open;
        ui_state_changed = true;
      }
      if (stygian_button(ctx, font, "Debug", debug_button_x, input_y,
                         debug_button_w, 32)) {
        g_show_debug_widget = !g_show_debug_widget;
        ui_state_changed = true;
      }

      if (g_picker_open) {
        float panel_w = 316.0f;
        float panel_h = 292.0f;
        float panel_x = (float)w - panel_w - 24.0f;
        float panel_y = (float)h - panel_h - 124.0f;
        float grid_x = panel_x + 10.0f;
        float grid_y = panel_y + 32.0f;
        float grid_w = panel_w - 22.0f;
        float grid_h = panel_h - 38.0f;
        int rows = (count + cols - 1) / cols;
        float grid_content_h = (float)rows * 42.0f + 4.0f;
        float grid_max_scroll = grid_content_h - grid_h;
        float wheel_dy = stygian_widgets_scroll_dy();
        float prev_picker_scroll_y = g_picker_scroll_y;
        bool clicked_picker = false;
        stygian_rect_rounded(ctx, panel_x, panel_y, panel_w, panel_h, 0.13f,
                             0.13f, 0.15f, 0.98f, 8.0f);
        stygian_text(ctx, font, "Emoji picker", panel_x + 10.0f, panel_y + 8.0f,
                     14.0f, 0.9f, 0.92f, 0.95f, 1.0f);

        stygian_clip_push(ctx, grid_x, grid_y, grid_w, grid_h);
        for (pr = 0; pr < rows; pr++) {
          for (pc = 0; pc < cols; pc++) {
            int idx = pr * cols + pc;
            float bx, by;
            if (idx >= count)
              break;
            bx = grid_x + pc * 74.0f;
            by = grid_y + pr * 42.0f - g_picker_scroll_y;
            if (by + 34.0f < grid_y || by > grid_y + grid_h)
              continue;
            if (stygian_button(ctx, font, k_picker_entries[idx].label, bx, by,
                               66.0f, 34.0f)) {
              append_shortcode_token(input, (int)sizeof(input),
                                     k_picker_entries[idx].id);
              g_picker_open = false;
              clicked_picker = true;
              ui_state_changed = true;
              break;
            }
          }
          if (!g_picker_open)
            break;
        }
        stygian_clip_pop(ctx);
        if (grid_max_scroll > 0.0f && wheel_dy != 0.0f && mx >= (int)panel_x &&
            mx <= (int)(panel_x + panel_w) && my >= (int)panel_y &&
            my <= (int)(panel_y + panel_h)) {
          g_picker_scroll_y -= wheel_dy * 24.0f;
          if (g_picker_scroll_y < 0.0f)
            g_picker_scroll_y = 0.0f;
          if (g_picker_scroll_y > grid_max_scroll)
            g_picker_scroll_y = grid_max_scroll;
        }
        if (stygian_scrollbar_v(ctx, panel_x + panel_w - 8.0f, grid_y, 6.0f,
                                grid_h, grid_content_h, &g_picker_scroll_y)) {
          ui_state_changed = true;
        }
        if (g_picker_scroll_y != prev_picker_scroll_y) {
          ui_state_changed = true;
        }

        // Click outside picker closes it.
        if (!clicked_picker && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
          bool inside = (mx >= (int)panel_x && mx <= (int)(panel_x + panel_w) &&
                         my >= (int)panel_y && my <= (int)(panel_y + panel_h));
          if (!inside) {
            g_picker_open = false;
            ui_state_changed = true;
          }
        }
      }
    }
    stygian_text(ctx, font, "Enter = send", 36, (float)h - 98, 12, 0.65f, 0.65f,
                 0.7f, 1.0f);

    enter_down = stygian_key_down(win, STYGIAN_KEY_ENTER);
    if ((enter_pressed || (enter_down && !prev_enter)) && input[0]) {
      char tokens[MAX_MSG_EMOJI][256];
      int tcount = 0;
      int hit_count = 0;
      int msg_idx;
      tcount = extract_shortcodes(input, tokens, MAX_MSG_EMOJI);
      msg_idx =
          g_message_count < MAX_MESSAGES ? g_message_count : (MAX_MESSAGES - 1);
      push_message(input);

      for (i = 0; i < tcount; i++) {
        int k = g_messages[msg_idx].emoji_count;
        if (k < MAX_MSG_EMOJI) {
          char norm[128];
          if (!stygian_shortcode_normalize(tokens[i], norm, sizeof(norm)))
            continue;
          uint64_t h = fnv1a64(norm);
          g_messages[msg_idx].glyph_hashes[k] = h;
          strncpy(g_messages[msg_idx].emoji_ids[k], norm,
                  sizeof(g_messages[msg_idx].emoji_ids[k]) - 1);
          g_messages[msg_idx]
              .emoji_ids[k][sizeof(g_messages[msg_idx].emoji_ids[k]) - 1] =
              '\0';
          g_messages[msg_idx].emoji_count++;
          g_messages[msg_idx].has_emoji = true;
          hit_count++;
        }
      }
      if (hit_count > 0) {
        snprintf(g_status_line, sizeof(g_status_line),
                 "status: parsed %d/%d shortcode(s)", hit_count, tcount);
      } else if (tcount > 0) {
        snprintf(g_status_line, sizeof(g_status_line),
                 "status: parse miss for %d shortcode(s)", tcount);
      } else {
        snprintf(g_status_line, sizeof(g_status_line), "status: plain message");
      }

      if (g_messages[msg_idx].has_emoji) {
        strncpy(g_messages[msg_idx].emoji_id, tokens[0],
                sizeof(g_messages[msg_idx].emoji_id) - 1);
        g_messages[msg_idx].emoji_id[sizeof(g_messages[msg_idx].emoji_id) - 1] =
            '\0';
      }
      {
        float content_h = (float)g_message_count * 56.0f + 8.0f;
        float viewport_h = (float)h - 188.0f;
        float max_scroll = content_h - viewport_h;
        if (max_scroll < 0.0f)
          max_scroll = 0.0f;
        g_msg_scroll_y = max_scroll;
      }
      printf("[chat_emoji_demo] perf now lookup(avg/p95)=%.3f/%.3fms "
             "decode(avg/p95)=%.3f/%.3fms upload(avg/p95)=%.3f/%.3fms\n",
             perf_avg_ms(&g_lookup_perf), perf_p95_ms(&g_lookup_perf),
             perf_avg_ms(&g_decode_perf), perf_p95_ms(&g_decode_perf),
             perf_avg_ms(&g_upload_perf), perf_p95_ms(&g_upload_perf));
      input[0] = '\0';
      ui_state_changed = true;
    }
    prev_enter = enter_down;

    stygian_scope_end(ctx);
    if (ui_state_changed) {
      stygian_scope_invalidate_next(ctx, k_scope_chat_base);
      stygian_set_repaint_source(ctx, "mutation");
      stygian_request_repaint_after_ms(ctx, 0u);
    }

    if (g_show_debug_widget) {
      stygian_scope_begin(ctx, k_scope_chat_perf);
      if (!g_perf_widget_pos_init) {
        g_perf_widget.x = (float)w - g_perf_widget.w - 24.0f;
        g_perf_widget.y = 24.0f;
        g_perf_widget_pos_init = true;
      }
      if (g_perf_widget.x < 8.0f)
        g_perf_widget.x = 8.0f;
      if (g_perf_widget.y < 8.0f)
        g_perf_widget.y = 8.0f;
      if (g_perf_widget.x + g_perf_widget.w > (float)w - 8.0f)
        g_perf_widget.x = (float)w - g_perf_widget.w - 8.0f;
      if (g_perf_widget.y + g_perf_widget.h > (float)h - 8.0f)
        g_perf_widget.y = (float)h - g_perf_widget.h - 8.0f;
      stygian_perf_widget(ctx, font, &g_perf_widget);
      stygian_scope_end(ctx);
    }

    stygian_widgets_commit_regions();
    stygian_end_frame(ctx);
  }

  if (g_emoji_atlas_tex) {
    stygian_texture_destroy(ctx, g_emoji_atlas_tex);
    g_emoji_atlas_tex = 0;
  }
  free(g_sgc.hash_index);
  ZSTD_freeDDict(g_sgc.ddict);
  free(g_sgc.blob);
  printf(
      "[chat_emoji_demo] perf startup_mount=%.3fms lookup(avg/p95)=%.3f/%.3fms "
      "decode(avg/p95)=%.3f/%.3fms upload(avg/p95)=%.3f/%.3fms\n",
      (double)g_startup_mount_us / 1000.0, perf_avg_ms(&g_lookup_perf),
      perf_p95_ms(&g_lookup_perf), perf_avg_ms(&g_decode_perf),
      perf_p95_ms(&g_decode_perf), perf_avg_ms(&g_upload_perf),
      perf_p95_ms(&g_upload_perf));
  stygian_destroy(ctx);
  stygian_window_destroy(win);
  return 0;
}
