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
