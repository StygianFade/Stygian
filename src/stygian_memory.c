// stygian_memory.c - Memory Management Implementation
// Part of Phase 5.5 - Advanced Features

#include "../include/stygian_memory.h"
#include "stygian_internal.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Arena Allocator Implementation
// ============================================================================

static void *arena_alloc_fn(StygianAllocator *allocator, size_t size,
                            size_t alignment) {
  StygianArena *arena = (StygianArena *)allocator;
  return stygian_arena_alloc(arena, size, alignment);
}

static void arena_free_fn(StygianAllocator *allocator, void *ptr) {
  // Arena doesn't support individual frees
  (void)allocator;
  (void)ptr;
}

static void arena_reset_fn(StygianAllocator *allocator) {
  StygianArena *arena = (StygianArena *)allocator;
  stygian_arena_reset(arena);
}

StygianArena *stygian_arena_create(size_t capacity) {
  StygianArena *arena = (StygianArena *)malloc(sizeof(StygianArena));
  if (!arena)
    return NULL;

  arena->buffer = (uint8_t *)malloc(capacity);
  if (!arena->buffer) {
    free(arena);
    return NULL;
  }

  arena->base.alloc = arena_alloc_fn;
  arena->base.free = arena_free_fn;
  arena->base.reset = arena_reset_fn;
  arena->base.user_data = arena;
  arena->capacity = capacity;
  arena->offset = 0;
  arena->owns_memory = true;

  return arena;
}

StygianArena *stygian_arena_create_from_buffer(void *buffer, size_t capacity) {
  StygianArena *arena = (StygianArena *)malloc(sizeof(StygianArena));
  if (!arena)
    return NULL;

  arena->base.alloc = arena_alloc_fn;
  arena->base.free = arena_free_fn;
  arena->base.reset = arena_reset_fn;
  arena->base.user_data = arena;
  arena->buffer = (uint8_t *)buffer;
  arena->capacity = capacity;
  arena->offset = 0;
  arena->owns_memory = false;

  return arena;
}

void stygian_arena_destroy(StygianArena *arena) {
  if (!arena)
    return;
  if (arena->owns_memory) {
    free(arena->buffer);
  }
  free(arena);
}

void stygian_arena_reset(StygianArena *arena) {
  if (!arena)
    return;
  arena->offset = 0;
}

void *stygian_arena_alloc(StygianArena *arena, size_t size, size_t alignment) {
  if (!arena || size == 0)
    return NULL;

  // Align offset
  size_t aligned_offset = (arena->offset + alignment - 1) & ~(alignment - 1);

  if (aligned_offset + size > arena->capacity) {
    // Debug trap: catch arena exhaustion early during development
    STYGIAN_ASSERT(0 && "stygian_arena_alloc: arena overflow");
    return NULL; // Out of memory
  }

  void *ptr = arena->buffer + aligned_offset;
  arena->offset = aligned_offset + size;

  return ptr;
}

// ============================================================================
// Pool Allocator Implementation
// ============================================================================

static void *pool_alloc_fn(StygianAllocator *allocator, size_t size,
                           size_t alignment) {
  StygianPool *pool = (StygianPool *)allocator;
  (void)size;
  (void)alignment;
  return stygian_pool_alloc(pool);
}

static void pool_free_fn(StygianAllocator *allocator, void *ptr) {
  StygianPool *pool = (StygianPool *)allocator;
  stygian_pool_free(pool, ptr);
}

static void pool_reset_fn(StygianAllocator *allocator) {
  StygianPool *pool = (StygianPool *)allocator;
  stygian_pool_reset(pool);
}

StygianPool *stygian_pool_create(size_t block_size, size_t block_count) {
  // Ensure block size can hold a pointer
  if (block_size < sizeof(StygianPoolBlock)) {
    block_size = sizeof(StygianPoolBlock);
  }

  size_t capacity = block_size * block_count;

  StygianPool *pool = (StygianPool *)malloc(sizeof(StygianPool));
  if (!pool)
    return NULL;

  pool->buffer = (uint8_t *)malloc(capacity);
  if (!pool->buffer) {
    free(pool);
    return NULL;
  }

  pool->base.alloc = pool_alloc_fn;
  pool->base.free = pool_free_fn;
  pool->base.reset = pool_reset_fn;
  pool->base.user_data = pool;
  pool->capacity = capacity;
  pool->block_size = block_size;
  pool->owns_memory = true;

  // Initialize free list
  stygian_pool_reset(pool);

  return pool;
}

StygianPool *stygian_pool_create_from_buffer(void *buffer, size_t capacity,
                                             size_t block_size) {
  if (block_size < sizeof(StygianPoolBlock)) {
    block_size = sizeof(StygianPoolBlock);
  }

  StygianPool *pool = (StygianPool *)malloc(sizeof(StygianPool));
  if (!pool)
    return NULL;

  pool->base.alloc = pool_alloc_fn;
  pool->base.free = pool_free_fn;
  pool->base.reset = pool_reset_fn;
  pool->base.user_data = pool;
  pool->buffer = (uint8_t *)buffer;
  pool->capacity = capacity;
  pool->block_size = block_size;
  pool->owns_memory = false;

  stygian_pool_reset(pool);

  return pool;
}

void stygian_pool_destroy(StygianPool *pool) {
  if (!pool)
    return;
  if (pool->owns_memory) {
    free(pool->buffer);
  }
  free(pool);
}

void stygian_pool_reset(StygianPool *pool) {
  if (!pool)
    return;

  pool->free_list = NULL;

  size_t block_count = pool->capacity / pool->block_size;
  for (size_t i = 0; i < block_count; i++) {
    StygianPoolBlock *block =
        (StygianPoolBlock *)(pool->buffer + i * pool->block_size);
    block->next = pool->free_list;
    pool->free_list = block;
  }
}

void *stygian_pool_alloc(StygianPool *pool) {
  if (!pool || !pool->free_list)
    return NULL;

  StygianPoolBlock *block = pool->free_list;
  pool->free_list = block->next;

  return (void *)block;
}

void stygian_pool_free(StygianPool *pool, void *ptr) {
  if (!pool || !ptr)
    return;

  StygianPoolBlock *block = (StygianPoolBlock *)ptr;
  block->next = pool->free_list;
  pool->free_list = block;
}
