#ifndef STYGIAN_LAYOUT_INTERNAL_H
#define STYGIAN_LAYOUT_INTERNAL_H

#include "stygian_layout.h"

// ============================================================================
// Internal Layout Types
// ============================================================================

struct StygianLayout {
  float x, y, w, h;             // Bounds
  StygianLayoutDir dir;         // Row or column
  StygianLayoutAlign align;     // Cross-axis alignment
  StygianLayoutJustify justify; // Main-axis distribution
  float gap;                    // Gap between children
  float padding;                // Inner padding

  // Internal state
  float cursor_x, cursor_y; // Current placement position
  int item_count;           // Number of children added
};

#endif // STYGIAN_LAYOUT_INTERNAL_H
