#ifndef STYGIAN_CLIPBOARD_H
#define STYGIAN_CLIPBOARD_H

#include "../include/stygian.h"

#ifdef __cplusplus
extern "C" {
#endif

// Advanced Clipboard API

typedef enum {
    STYGIAN_CLIP_TEXT,
    STYGIAN_CLIP_FILE_REF, // Just a file path
    STYGIAN_CLIP_OBJECT    // Complex object
} StygianClipType;

// Push content to Universal Clipboard (OS + History)
// text: The raw text representation
// metadata: Optional JSON metadata or file path if type is FILE_REF
void stygian_clipboard_push(StygianContext *ctx, const char *text, const char *metadata);

// Pop content from Universal Clipboard
// Returns heap string (must be freed)
char *stygian_clipboard_pop(StygianContext *ctx);

// Get internal history count
int stygian_clipboard_history_count(StygianContext *ctx);

// Get history item (read-only)
const char *stygian_clipboard_history_get(StygianContext *ctx, int index);

#ifdef __cplusplus
}
#endif

#endif // STYGIAN_CLIPBOARD_H
