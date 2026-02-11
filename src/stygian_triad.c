#include "stygian_triad.h"
#include "../include/stygian_memory.h"
#include "stygian_internal.h" // stygian_cpystr
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STYGIAN_TRIAD_MAGIC "TRIAD01"
#define STYGIAN_TRIAD_CODEC_RAW 0u
#define STYGIAN_TRIAD_CODEC_LZSS 1u
#define STYGIAN_TRIAD_CODEC_TRIAD_V34_RAW 2u
#define STYGIAN_TRIAD_CODEC_TRIAD_V34_LZSS 3u

typedef struct StygianTriadHeaderFile {
  char magic[8];
  uint32_t version;
  uint32_t encoding;
  uint32_t tier;
  uint32_t entry_count;
  uint64_t data_offset;
  uint8_t pad[32];
} StygianTriadHeaderFile;

typedef struct StygianTriadEntryFile {
  uint64_t glyph_hash;
  uint64_t blob_hash;
  uint64_t payload_offset;
  uint32_t payload_size;
  uint32_t raw_blob_size;
  uint32_t glyph_len;
  uint32_t codec;
} StygianTriadEntryFile;

typedef struct StygianTriadGlyphMapEntry {
  char *glyph_id;
  uint64_t glyph_hash;
} StygianTriadGlyphMapEntry;

typedef struct StygianTriadV34PayloadHeader {
  char magic[8];
  uint16_t tier_res;
  uint16_t ll_res;
  uint16_t vals_count;
  uint16_t quant_step;
  uint16_t thresh_q;
  uint16_t flags;
  uint32_t nnz_count;
  uint32_t ll_size;
  uint32_t aux_size;
} StygianTriadV34PayloadHeader;

struct StygianTriadRuntime {
  FILE *file;
  StygianTriadPackInfo pack;
  StygianTriadEntryFile *entries;
  StygianTriadGlyphMapEntry *glyph_map;
  StygianAllocator *allocator;
  char path[512];
};

// Allocator helpers: use runtime allocator when set, else CRT fallback
static void *triad_alloc(StygianTriadRuntime *rt, size_t size,
                         size_t alignment) {
  if (rt && rt->allocator && rt->allocator->alloc)
    return rt->allocator->alloc(rt->allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void *triad_calloc(StygianTriadRuntime *rt, size_t count, size_t size,
                          size_t alignment) {
  void *p = triad_alloc(rt, count * size, alignment);
  if (p)
    memset(p, 0, count * size);
  return p;
}
static void *triad_realloc(StygianTriadRuntime *rt, void *ptr, size_t old_size,
                           size_t new_size, size_t alignment) {
  // If using allocator, we need to alloc new + copy + free old
  if (rt && rt->allocator && rt->allocator->alloc) {
    void *np = rt->allocator->alloc(rt->allocator, new_size, alignment);
    if (!np)
      return NULL;
    if (ptr && old_size > 0) {
      memcpy(np, ptr, old_size < new_size ? old_size : new_size);
      if (rt->allocator->free)
        rt->allocator->free(rt->allocator, ptr);
    }
    return np;
  }
  (void)old_size;
  (void)alignment;
  return realloc(ptr, new_size);
}
static void triad_free(StygianTriadRuntime *rt, void *ptr) {
  if (!ptr)
    return;
  if (rt && rt->allocator && rt->allocator->free)
    rt->allocator->free(rt->allocator, ptr);
  else
    free(ptr);
}
// Config-based alloc for bootstrap (before runtime exists)
static void *triad_cfg_alloc(StygianAllocator *allocator, size_t size,
                             size_t alignment) {
  if (allocator && allocator->alloc)
    return allocator->alloc(allocator, size, alignment);
  (void)alignment;
  return malloc(size);
}
static void triad_cfg_free(StygianAllocator *allocator, void *ptr) {
  if (!ptr)
    return;
  if (allocator && allocator->free)
    allocator->free(allocator, ptr);
  else
    free(ptr);
}

static int stygian_triad_entry_cmp(const void *a, const void *b) {
  const StygianTriadEntryFile *ea = (const StygianTriadEntryFile *)a;
  const StygianTriadEntryFile *eb = (const StygianTriadEntryFile *)b;
  if (ea->glyph_hash < eb->glyph_hash)
    return -1;
  if (ea->glyph_hash > eb->glyph_hash)
    return 1;
  return 0;
}

static int stygian_triad_glyph_map_cmp(const void *a, const void *b) {
  const StygianTriadGlyphMapEntry *ga = (const StygianTriadGlyphMapEntry *)a;
  const StygianTriadGlyphMapEntry *gb = (const StygianTriadGlyphMapEntry *)b;
  if (!ga->glyph_id && !gb->glyph_id)
    return 0;
  if (!ga->glyph_id)
    return -1;
  if (!gb->glyph_id)
    return 1;
  return strcmp(ga->glyph_id, gb->glyph_id);
}

static void stygian_triad_runtime_reset(StygianTriadRuntime *rt) {
  uint32_t i;
  if (!rt)
    return;
  if (rt->file) {
    fclose(rt->file);
    rt->file = NULL;
  }
  triad_free(rt, rt->entries);
  rt->entries = NULL;
  if (rt->glyph_map) {
    for (i = 0; i < rt->pack.entry_count; i++) {
      triad_free(rt, rt->glyph_map[i].glyph_id);
    }
    triad_free(rt, rt->glyph_map);
    rt->glyph_map = NULL;
  }
  memset(&rt->pack, 0, sizeof(rt->pack));
  rt->path[0] = '\0';
}

static int stygian_lzss_decompress(const uint8_t *in, uint32_t in_n,
                                   uint8_t *out, uint32_t out_n) {
  uint32_t ip = 0;
  uint32_t op = 0;
  if (!in || !out)
    return 0;
  while (ip < in_n && op < out_n) {
    uint8_t flags = in[ip++];
    int bit;
    for (bit = 0; bit < 8 && ip < in_n && op < out_n; bit++) {
      if (flags & (1u << bit)) {
        out[op++] = in[ip++];
      } else {
        uint16_t token;
        uint32_t off;
        uint32_t len;
        uint32_t back;
        if (ip + 1 >= in_n)
          return 0;
        token = ((uint16_t)in[ip] << 8) | in[ip + 1];
        ip += 2;
        off = (token >> 4) & 0x0FFFu;
        len = (token & 0x0Fu) + 3u;
        if (off == 0 || off > op)
          return 0;
        back = op - off;
        while (len-- && op < out_n) {
          out[op++] = out[back++];
        }
      }
    }
  }
  return op == out_n;
}

static uint64_t stygian_fnv1a64_append(uint64_t h, const char *s) {
  size_t i;
  if (!s)
    return h;
  for (i = 0; s[i] != '\0'; i++) {
    h ^= (uint64_t)(uint8_t)s[i];
    h *= 1099511628211ull;
  }
  return h;
}

static float stygian_clamp01(float v) {
  if (v < 0.0f)
    return 0.0f;
  if (v > 1.0f)
    return 1.0f;
  return v;
}

static void stygian_upscale_ll_to_128(const uint8_t *src, int src_res,
                                      uint8_t *dst128) {
  int y, x;
  for (y = 0; y < 128; y++) {
    float fy = ((float)y + 0.5f) * ((float)src_res / 128.0f) - 0.5f;
    int y0 = (int)fy;
    int y1;
    float wy;
    if (y0 < 0)
      y0 = 0;
    y1 = y0 + 1;
    if (y1 >= src_res)
      y1 = src_res - 1;
    wy = fy - (float)y0;
    if (wy < 0.0f)
      wy = 0.0f;
    if (wy > 1.0f)
      wy = 1.0f;
    for (x = 0; x < 128; x++) {
      float fx = ((float)x + 0.5f) * ((float)src_res / 128.0f) - 0.5f;
      int x0 = (int)fx;
      int x1;
      float wx;
      float v00, v01, v10, v11, v0, v1, v;
      if (x0 < 0)
        x0 = 0;
      x1 = x0 + 1;
      if (x1 >= src_res)
        x1 = src_res - 1;
      wx = fx - (float)x0;
      if (wx < 0.0f)
        wx = 0.0f;
      if (wx > 1.0f)
        wx = 1.0f;
      v00 = (float)src[(size_t)y0 * (size_t)src_res + (size_t)x0];
      v01 = (float)src[(size_t)y0 * (size_t)src_res + (size_t)x1];
      v10 = (float)src[(size_t)y1 * (size_t)src_res + (size_t)x0];
      v11 = (float)src[(size_t)y1 * (size_t)src_res + (size_t)x1];
      v0 = v00 + (v01 - v00) * wx;
      v1 = v10 + (v11 - v10) * wx;
      v = v0 + (v1 - v0) * wy;
      if (v < 0.0f)
        v = 0.0f;
      if (v > 255.0f)
        v = 255.0f;
      dst128[(size_t)y * 128u + (size_t)x] = (uint8_t)(v + 0.5f);
    }
  }
}

static void stygian_upscale_sparse_to_256(const uint8_t *src, int src_res,
                                          uint8_t *dst256) {
  int y, x;
  for (y = 0; y < 256; y++) {
    float fy = ((float)y + 0.5f) * ((float)src_res / 256.0f) - 0.5f;
    int sy = (int)(fy + 0.5f);
    if (sy < 0)
      sy = 0;
    if (sy >= src_res)
      sy = src_res - 1;
    for (x = 0; x < 256; x++) {
      float fx = ((float)x + 0.5f) * ((float)src_res / 256.0f) - 0.5f;
      int sx = (int)(fx + 0.5f);
      if (sx < 0)
        sx = 0;
      if (sx >= src_res)
        sx = src_res - 1;
      dst256[(size_t)y * 256u + (size_t)x] =
          src[(size_t)sy * (size_t)src_res + (size_t)sx];
    }
  }
}

StygianTriadRuntime *stygian_triad_runtime_create(void) {
  return stygian_triad_runtime_create_ex(NULL);
}

StygianTriadRuntime *
stygian_triad_runtime_create_ex(StygianAllocator *allocator) {
  StygianTriadRuntime *rt = (StygianTriadRuntime *)triad_cfg_alloc(
      allocator, sizeof(StygianTriadRuntime), _Alignof(StygianTriadRuntime));
  if (!rt)
    return NULL;
  memset(rt, 0, sizeof(StygianTriadRuntime));
  rt->allocator = allocator;
  return rt;
}

void stygian_triad_runtime_destroy(StygianTriadRuntime *rt) {
  if (!rt)
    return;
  StygianAllocator *allocator = rt->allocator;
  stygian_triad_runtime_reset(rt);
  triad_cfg_free(allocator, rt);
}

bool stygian_triad_runtime_mount(StygianTriadRuntime *rt, const char *path) {
  StygianTriadHeaderFile h;
  FILE *f = NULL;
  StygianTriadEntryFile *entries = NULL;
  StygianTriadGlyphMapEntry *glyph_map = NULL;
  uint8_t *id_buf = NULL;
  uint32_t i;

  if (!rt || !path || !path[0])
    return false;

  f = fopen(path, "rb");
  if (!f)
    return false;

  if (fread(&h, sizeof(h), 1, f) != 1) {
    fclose(f);
    return false;
  }

  if (memcmp(h.magic, STYGIAN_TRIAD_MAGIC, 7) != 0 || h.entry_count == 0) {
    fclose(f);
    return false;
  }

  entries = (StygianTriadEntryFile *)triad_alloc(
      rt, (size_t)h.entry_count * sizeof(StygianTriadEntryFile),
      _Alignof(StygianTriadEntryFile));
  if (!entries) {
    fclose(f);
    return false;
  }

  if (fread(entries, sizeof(StygianTriadEntryFile), h.entry_count, f) !=
      h.entry_count) {
    triad_free(rt, entries);
    fclose(f);
    return false;
  }

  qsort(entries, (size_t)h.entry_count, sizeof(StygianTriadEntryFile),
        stygian_triad_entry_cmp);

  // Build in-memory glyph id map once to avoid per-lookup file scans.
  glyph_map = (StygianTriadGlyphMapEntry *)triad_calloc(
      rt, (size_t)h.entry_count, sizeof(StygianTriadGlyphMapEntry),
      _Alignof(StygianTriadGlyphMapEntry));
  if (!glyph_map) {
    triad_free(rt, entries);
    fclose(f);
    return false;
  }

  id_buf = (uint8_t *)triad_alloc(rt, 1024, 1);
  if (!id_buf) {
    triad_free(rt, glyph_map);
    triad_free(rt, entries);
    fclose(f);
    return false;
  }
  size_t id_buf_cap = 1024;

  for (i = 0; i < h.entry_count; i++) {
    size_t glen = entries[i].glyph_len;
    if (glen == 0)
      continue;
    if (glen >= id_buf_cap) {
      size_t new_cap = glen + 1u;
      uint8_t *nb =
          (uint8_t *)triad_realloc(rt, id_buf, id_buf_cap, new_cap, 1);
      if (!nb)
        continue;
      id_buf = nb;
      id_buf_cap = new_cap;
    }
    if (fseek(f, (long)entries[i].payload_offset, SEEK_SET) != 0)
      continue;
    if (fread(id_buf, 1, glen, f) != glen)
      continue;
    id_buf[glen] = 0;
    glyph_map[i].glyph_id = (char *)triad_alloc(rt, glen + 1u, 1);
    if (!glyph_map[i].glyph_id)
      continue;
    memcpy(glyph_map[i].glyph_id, id_buf, glen + 1u);
    glyph_map[i].glyph_hash = entries[i].glyph_hash;
  }
  triad_free(rt, id_buf);
  qsort(glyph_map, (size_t)h.entry_count, sizeof(StygianTriadGlyphMapEntry),
        stygian_triad_glyph_map_cmp);

  stygian_triad_runtime_reset(rt);
  rt->file = f;
  rt->entries = entries;
  rt->glyph_map = glyph_map;
  rt->pack.version = h.version;
  rt->pack.encoding = h.encoding;
  rt->pack.tier = h.tier;
  rt->pack.entry_count = h.entry_count;
  rt->pack.data_offset = h.data_offset;
  stygian_cpystr(rt->path, sizeof(rt->path), path);
  return true;
}

void stygian_triad_runtime_unmount(StygianTriadRuntime *rt) {
  stygian_triad_runtime_reset(rt);
}

bool stygian_triad_runtime_is_mounted(const StygianTriadRuntime *rt) {
  if (!rt)
    return false;
  return rt->file != NULL && rt->entries != NULL && rt->pack.entry_count > 0;
}

bool stygian_triad_runtime_get_pack_info(const StygianTriadRuntime *rt,
                                         StygianTriadPackInfo *out_info) {
  if (!rt || !out_info || !stygian_triad_runtime_is_mounted(rt))
    return false;
  *out_info = rt->pack;
  return true;
}

bool stygian_triad_runtime_lookup(const StygianTriadRuntime *rt,
                                  uint64_t glyph_hash,
                                  StygianTriadEntryInfo *out_entry) {
  size_t lo, hi;
  if (!rt || !out_entry || !stygian_triad_runtime_is_mounted(rt))
    return false;

  lo = 0;
  hi = (size_t)rt->pack.entry_count;
  while (lo < hi) {
    size_t mid = lo + ((hi - lo) / 2);
    uint64_t k = rt->entries[mid].glyph_hash;
    if (k < glyph_hash) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  if (lo >= (size_t)rt->pack.entry_count ||
      rt->entries[lo].glyph_hash != glyph_hash) {
    return false;
  }

  out_entry->glyph_hash = rt->entries[lo].glyph_hash;
  out_entry->blob_hash = rt->entries[lo].blob_hash;
  out_entry->payload_offset = rt->entries[lo].payload_offset;
  out_entry->payload_size = rt->entries[lo].payload_size;
  out_entry->raw_blob_size = rt->entries[lo].raw_blob_size;
  out_entry->glyph_len = rt->entries[lo].glyph_len;
  out_entry->codec = rt->entries[lo].codec;
  return true;
}

bool stygian_triad_runtime_lookup_glyph_id(const StygianTriadRuntime *rt,
                                           const char *glyph_id,
                                           StygianTriadEntryInfo *out_entry) {
  const char *base = NULL;
  char normalized[512];
  char normalized_ext[512];
  char uplus_norm[512];
  char uplus_norm_ext[512];
  char hex_norm[512];
  char hex_norm_ext[512];
  char with_ext[512];
  char base_with_ext[512];
  const char *candidates[12];
  int ccount = 0;
  int ci;
  if (!rt || !glyph_id || !out_entry || !stygian_triad_runtime_is_mounted(rt))
    return false;

  base = strrchr(glyph_id, '/');
  if (!base)
    base = strrchr(glyph_id, '\\');
  if (base)
    base++;
  else
    base = glyph_id;

  // Normalize user input:
  // - trim surrounding colons
  // - lowercase
  // - '-' to '_'
  // - strip optional ".svg"
  {
    const char *s = glyph_id;
    const char *e = glyph_id + strlen(glyph_id);
    size_t n;
    if (s < e && *s == ':')
      s++;
    if (e > s && *(e - 1) == ':')
      e--;
    n = (size_t)(e - s);
    if (n >= sizeof(normalized))
      n = sizeof(normalized) - 1;
    memcpy(normalized, s, n);
    normalized[n] = '\0';
    if (n >= 4 && strcmp(normalized + n - 4, ".svg") == 0) {
      normalized[n - 4] = '\0';
    }
    for (n = 0; normalized[n] != '\0'; n++) {
      unsigned char c = (unsigned char)normalized[n];
      if (c == '-')
        normalized[n] = '_';
      else
        normalized[n] = (char)tolower(c);
    }
    snprintf(normalized_ext, sizeof(normalized_ext), "%s.svg", normalized);
  }

  // Support U+XXXX -> emoji_uXXXX
  if ((normalized[0] == 'u' && normalized[1] == '+')) {
    snprintf(uplus_norm, sizeof(uplus_norm), "emoji_u%s", normalized + 2);
    snprintf(uplus_norm_ext, sizeof(uplus_norm_ext), "%s.svg", uplus_norm);
  } else {
    uplus_norm[0] = '\0';
    uplus_norm_ext[0] = '\0';
  }

  // Support raw hex -> emoji_uXXXX (e.g. 1f600)
  {
    size_t k;
    bool all_hex = normalized[0] != '\0';
    for (k = 0; normalized[k] != '\0'; k++) {
      unsigned char c = (unsigned char)normalized[k];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
        all_hex = false;
        break;
      }
    }
    if (all_hex) {
      snprintf(hex_norm, sizeof(hex_norm), "emoji_u%s", normalized);
      snprintf(hex_norm_ext, sizeof(hex_norm_ext), "%s.svg", hex_norm);
    } else {
      hex_norm[0] = '\0';
      hex_norm_ext[0] = '\0';
    }
  }

  snprintf(with_ext, sizeof(with_ext), "%s.svg", glyph_id);
  snprintf(base_with_ext, sizeof(base_with_ext), "%s.svg", base);
  candidates[ccount++] = glyph_id;
  candidates[ccount++] = base;
  candidates[ccount++] = with_ext;
  candidates[ccount++] = base_with_ext;
  candidates[ccount++] = normalized;
  candidates[ccount++] = normalized_ext;
  if (uplus_norm[0]) {
    candidates[ccount++] = uplus_norm;
    candidates[ccount++] = uplus_norm_ext;
  }
  if (hex_norm[0]) {
    candidates[ccount++] = hex_norm;
    candidates[ccount++] = hex_norm_ext;
  }

  for (ci = 0; ci < ccount; ci++) {
    size_t lo = 0, hi = (size_t)rt->pack.entry_count;
    const char *key = candidates[ci];
    while (lo < hi) {
      size_t mid = lo + ((hi - lo) / 2);
      const char *id = rt->glyph_map[mid].glyph_id;
      int cmp;
      if (!id)
        cmp = -1;
      else
        cmp = strcmp(id, key);
      if (cmp < 0)
        lo = mid + 1;
      else
        hi = mid;
    }
    if (lo < (size_t)rt->pack.entry_count && rt->glyph_map[lo].glyph_id &&
        strcmp(rt->glyph_map[lo].glyph_id, key) == 0) {
      return stygian_triad_runtime_lookup(rt, rt->glyph_map[lo].glyph_hash,
                                          out_entry);
    }
  }
  return false;
}

bool stygian_triad_runtime_read_svg_blob(const StygianTriadRuntime *rt,
                                         uint64_t glyph_hash,
                                         uint8_t **out_svg_data,
                                         uint32_t *out_svg_size) {
  StygianTriadEntryInfo e;
  uint8_t *inbuf = NULL;
  uint8_t *svg = NULL;
  uint32_t compressed_n;
  uint32_t svg_n;
  uint32_t raw_blob;
  if (!rt || !out_svg_data || !out_svg_size || !rt->file)
    return false;
  *out_svg_data = NULL;
  *out_svg_size = 0;
  if (!stygian_triad_runtime_lookup(rt, glyph_hash, &e))
    return false;
  if (e.raw_blob_size == 0 || e.payload_size == 0)
    return false;

  compressed_n = e.payload_size;
  raw_blob = e.raw_blob_size;
  svg_n = raw_blob;
  inbuf = (uint8_t *)malloc(compressed_n ? compressed_n : 1u);
  if (!inbuf)
    return false;
  if (fseek(rt->file, (long)(e.payload_offset + e.glyph_len), SEEK_SET) != 0 ||
      fread(inbuf, 1, compressed_n, rt->file) != compressed_n) {
    free(inbuf);
    return false;
  }

  svg = (uint8_t *)malloc((size_t)svg_n + 1u);
  if (!svg) {
    free(inbuf);
    return false;
  }

  if (e.codec == STYGIAN_TRIAD_CODEC_RAW) {
    if (compressed_n != svg_n) {
      free(inbuf);
      free(svg);
      return false;
    }
    memcpy(svg, inbuf, svg_n);
  } else if (e.codec == STYGIAN_TRIAD_CODEC_LZSS) {
    if (!stygian_lzss_decompress(inbuf, compressed_n, svg, svg_n)) {
      free(inbuf);
      free(svg);
      return false;
    }
  } else {
    free(inbuf);
    free(svg);
    return false;
  }
  svg[svg_n] = 0;
  free(inbuf);
  *out_svg_data = svg;
  *out_svg_size = svg_n;
  return true;
}

bool stygian_triad_runtime_decode_rgba(const StygianTriadRuntime *rt,
                                       uint64_t glyph_hash,
                                       uint8_t **out_rgba_data,
                                       uint32_t *out_width,
                                       uint32_t *out_height) {
  StygianTriadEntryInfo e;
  uint8_t *packed = NULL;
  uint8_t *raw = NULL;
  uint8_t *payload = NULL;
  uint32_t payload_n;
  uint32_t payload_storage_n;
  StygianTriadV34PayloadHeader ph;
  const uint8_t *llsrc;
  const uint8_t *valsrc;
  const uint8_t *aux;
  uint8_t *rgba = NULL;
  uint8_t ll_up[128 * 128];
  uint8_t *idx256 = NULL;
  uint8_t *idx_small = NULL;
  uint32_t i;
  int x, y;

  if (!rt || !rt->file || !out_rgba_data || !out_width || !out_height)
    return false;
  *out_rgba_data = NULL;
  *out_width = 0;
  *out_height = 0;

  if (!stygian_triad_runtime_lookup(rt, glyph_hash, &e))
    return false;
  if (e.payload_size == 0 || e.raw_blob_size == 0)
    return false;
  if (e.codec != STYGIAN_TRIAD_CODEC_TRIAD_V34_RAW &&
      e.codec != STYGIAN_TRIAD_CODEC_TRIAD_V34_LZSS) {
    return false;
  }

  payload_n = e.payload_size;
  payload_storage_n = e.payload_size;
  packed = (uint8_t *)malloc(payload_n ? payload_n : 1u);
  if (!packed)
    return false;
  if (fseek(rt->file, (long)(e.payload_offset + e.glyph_len), SEEK_SET) != 0 ||
      fread(packed, 1, payload_n, rt->file) != payload_n) {
    free(packed);
    return false;
  }

  if (e.codec == STYGIAN_TRIAD_CODEC_TRIAD_V34_LZSS) {
    raw = (uint8_t *)malloc(e.raw_blob_size ? e.raw_blob_size : 1u);
    if (!raw) {
      free(packed);
      return false;
    }
    if (!stygian_lzss_decompress(packed, payload_n, raw, e.raw_blob_size)) {
      free(packed);
      free(raw);
      return false;
    }
    payload = raw;
    payload_n = e.raw_blob_size;
  } else {
    payload = packed;
    payload_n = payload_storage_n;
  }

  if (payload_n < sizeof(ph)) {
    free(packed);
    free(raw);
    return false;
  }

  memcpy(&ph, payload, sizeof(ph));
  if (memcmp(ph.magic, "TRV34SP", 7) != 0 || ph.ll_res == 0 ||
      ph.tier_res == 0 || ph.tier_res > 256 || ph.vals_count > 255) {
    free(packed);
    free(raw);
    return false;
  }
  if ((size_t)sizeof(ph) + (size_t)ph.ll_size + (size_t)ph.vals_count +
          (size_t)ph.aux_size >
      payload_n) {
    free(packed);
    free(raw);
    return false;
  }

  llsrc = payload + sizeof(ph);
  valsrc = llsrc + ph.ll_size;
  aux = valsrc + ph.vals_count;

  stygian_upscale_ll_to_128(llsrc, (int)ph.ll_res, ll_up);

  idx_small = (uint8_t *)calloc((size_t)ph.tier_res * (size_t)ph.tier_res, 1u);
  idx256 = (uint8_t *)calloc(256u * 256u, 1u);
  rgba = (uint8_t *)malloc(256u * 256u * 4u);
  if (!idx_small || !idx256 || !rgba) {
    free(packed);
    free(raw);
    free(idx_small);
    free(idx256);
    free(rgba);
    return false;
  }

  for (i = 0; i < ph.nnz_count; i++) {
    uint32_t ai = i * 3u;
    uint16_t pos;
    uint8_t v;
    int sx, sy;
    if ((size_t)ai + 2u >= (size_t)ph.aux_size)
      break;
    pos = (uint16_t)(((uint16_t)aux[ai] << 8) | aux[ai + 1u]);
    v = aux[ai + 2u];
    sx = pos % ph.tier_res;
    sy = pos / ph.tier_res;
    if (sx >= 0 && sx < (int)ph.tier_res && sy >= 0 && sy < (int)ph.tier_res &&
        v > 0) {
      idx_small[(size_t)sy * (size_t)ph.tier_res + (size_t)sx] = v;
    }
  }

  stygian_upscale_sparse_to_256(idx_small, (int)ph.tier_res, idx256);

  for (y = 0; y < 256; y++) {
    for (x = 0; x < 256; x++) {
      size_t off = (size_t)y * 256u + (size_t)x;
      uint8_t ll = ll_up[(size_t)(y / 2) * 128u + (size_t)(x / 2)];
      uint8_t idx = idx256[off];
      float hf = 0.0f;
      float sign = ((x ^ y) & 1) ? -1.0f : 1.0f;
      float outv;
      if (idx > 0 && idx - 1u < ph.vals_count) {
        uint8_t raw_v = valsrc[idx - 1u];
        hf = ((float)raw_v / 127.5f) - 1.0f;
      }
      outv =
          stygian_clamp01(((float)ll / 255.0f) + hf * sign * 0.707f) * 255.0f;
      rgba[off * 4u + 0u] = (uint8_t)(outv + 0.5f);
      rgba[off * 4u + 1u] = (uint8_t)(outv + 0.5f);
      rgba[off * 4u + 2u] = (uint8_t)(outv + 0.5f);
      rgba[off * 4u + 3u] = 255u;
    }
  }

  free(packed);
  free(raw);
  free(idx_small);
  free(idx256);

  *out_rgba_data = rgba;
  *out_width = 256u;
  *out_height = 256u;
  return true;
}

void stygian_triad_runtime_free_blob(void *ptr) { free(ptr); }

uint64_t stygian_triad_runtime_hash_key(const char *glyph_id,
                                        const char *source_tag) {
  uint64_t h = 1469598103934665603ull;
  h = stygian_fnv1a64_append(h, glyph_id ? glyph_id : "");
  h ^= (uint64_t)'|';
  h *= 1099511628211ull;
  h = stygian_fnv1a64_append(h, source_tag ? source_tag : "");
  return h;
}
