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
