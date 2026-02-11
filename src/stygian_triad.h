#ifndef STYGIAN_TRIAD_H
#define STYGIAN_TRIAD_H

#include "../include/stygian.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct StygianTriadRuntime StygianTriadRuntime;

StygianTriadRuntime *stygian_triad_runtime_create(void);
StygianTriadRuntime *
stygian_triad_runtime_create_ex(StygianAllocator *allocator);
void stygian_triad_runtime_destroy(StygianTriadRuntime *rt);

bool stygian_triad_runtime_mount(StygianTriadRuntime *rt, const char *path);
void stygian_triad_runtime_unmount(StygianTriadRuntime *rt);
bool stygian_triad_runtime_is_mounted(const StygianTriadRuntime *rt);
bool stygian_triad_runtime_get_pack_info(const StygianTriadRuntime *rt,
                                         StygianTriadPackInfo *out_info);
bool stygian_triad_runtime_lookup(const StygianTriadRuntime *rt,
                                  uint64_t glyph_hash,
                                  StygianTriadEntryInfo *out_entry);
bool stygian_triad_runtime_lookup_glyph_id(const StygianTriadRuntime *rt,
                                           const char *glyph_id,
                                           StygianTriadEntryInfo *out_entry);
bool stygian_triad_runtime_read_svg_blob(const StygianTriadRuntime *rt,
                                         uint64_t glyph_hash,
                                         uint8_t **out_svg_data,
                                         uint32_t *out_svg_size);
bool stygian_triad_runtime_decode_rgba(const StygianTriadRuntime *rt,
                                       uint64_t glyph_hash,
                                       uint8_t **out_rgba_data,
                                       uint32_t *out_width,
                                       uint32_t *out_height);
void stygian_triad_runtime_free_blob(void *ptr);
uint64_t stygian_triad_runtime_hash_key(const char *glyph_id,
                                        const char *source_tag);

#endif // STYGIAN_TRIAD_H
