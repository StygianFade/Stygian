// stygian_unicode.h - UTF-8, grapheme, and emoji shortcode helpers
#ifndef STYGIAN_UNICODE_H
#define STYGIAN_UNICODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct StygianGraphemeSpan {
  size_t byte_start;
  size_t byte_len;
  uint32_t first_codepoint;
  uint32_t flags;
} StygianGraphemeSpan;

enum {
  STYGIAN_GRAPHEME_HAS_ZWJ = 1u << 0,
  STYGIAN_GRAPHEME_HAS_VARIATION = 1u << 1,
  STYGIAN_GRAPHEME_HAS_SKIN_TONE = 1u << 2,
  STYGIAN_GRAPHEME_IS_REGIONAL_PAIR = 1u << 3,
};

// Decode next UTF-8 codepoint from text[*io_index...].
// Returns false only when end of buffer is reached.
// Invalid sequences advance by one byte and return U+FFFD.
bool stygian_utf8_next(const char *text, size_t text_len, size_t *io_index,
                       uint32_t *out_codepoint);

// Iterate one grapheme cluster-like span using a pragmatic rule set suitable
// for emoji + combining mark text flows.
bool stygian_grapheme_next(const char *text, size_t text_len, size_t *io_index,
                           StygianGraphemeSpan *out_span);

// Normalize user shortcode forms to canonical "emoji_u..." ids.
// Accepts forms like:
//   :emoji_u1f600:
//   emoji_u1f600
//   U+1F600
//   1f468-200d-1f4bb
bool stygian_shortcode_normalize(const char *input, char *out, size_t out_size);

#endif // STYGIAN_UNICODE_H
