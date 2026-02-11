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
