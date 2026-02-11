#include "stygian_unicode.h"

#include <ctype.h>
#include <string.h>

#define STYGIAN_REPLACEMENT_CHAR 0xFFFDu

static bool unicode_is_cont(uint8_t b) { return (b & 0xC0u) == 0x80u; }

static bool unicode_is_regional_indicator(uint32_t cp) {
  return cp >= 0x1F1E6u && cp <= 0x1F1FFu;
}

static bool unicode_is_skin_tone(uint32_t cp) {
  return cp >= 0x1F3FBu && cp <= 0x1F3FFu;
}

static bool unicode_is_variation_selector(uint32_t cp) {
  return (cp >= 0xFE00u && cp <= 0xFE0Fu) ||
         (cp >= 0xE0100u && cp <= 0xE01EFu);
}

static bool unicode_is_combining_mark(uint32_t cp) {
  if (cp >= 0x0300u && cp <= 0x036Fu)
    return true;
  if (cp >= 0x1AB0u && cp <= 0x1AFFu)
    return true;
  if (cp >= 0x1DC0u && cp <= 0x1DFFu)
    return true;
  if (cp >= 0x20D0u && cp <= 0x20FFu)
    return true;
  return cp >= 0xFE20u && cp <= 0xFE2Fu;
}

static bool unicode_is_joiner(uint32_t cp) { return cp == 0x200Du; }

static bool unicode_decode_at(const char *text, size_t text_len, size_t index,
                              size_t *out_next, uint32_t *out_cp) {
  uint8_t b0, b1, b2, b3;
  uint32_t cp;

  if (!text || !out_next || !out_cp || index >= text_len)
    return false;

  b0 = (uint8_t)text[index];
  if (b0 < 0x80u) {
    *out_cp = b0;
    *out_next = index + 1u;
    return true;
  }

  if ((b0 & 0xE0u) == 0xC0u) {
    if (index + 1u >= text_len)
      goto invalid;
    b1 = (uint8_t)text[index + 1u];
    if (!unicode_is_cont(b1))
      goto invalid;
    cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
    if (cp < 0x80u)
      goto invalid;
    *out_cp = cp;
    *out_next = index + 2u;
    return true;
  }

  if ((b0 & 0xF0u) == 0xE0u) {
    if (index + 2u >= text_len)
      goto invalid;
    b1 = (uint8_t)text[index + 1u];
    b2 = (uint8_t)text[index + 2u];
    if (!unicode_is_cont(b1) || !unicode_is_cont(b2))
      goto invalid;
    cp = ((uint32_t)(b0 & 0x0Fu) << 12) | ((uint32_t)(b1 & 0x3Fu) << 6) |
         (uint32_t)(b2 & 0x3Fu);
    if (cp < 0x800u)
      goto invalid;
    if (cp >= 0xD800u && cp <= 0xDFFFu)
      goto invalid;
    *out_cp = cp;
    *out_next = index + 3u;
    return true;
  }

  if ((b0 & 0xF8u) == 0xF0u) {
    if (index + 3u >= text_len)
      goto invalid;
    b1 = (uint8_t)text[index + 1u];
    b2 = (uint8_t)text[index + 2u];
    b3 = (uint8_t)text[index + 3u];
    if (!unicode_is_cont(b1) || !unicode_is_cont(b2) || !unicode_is_cont(b3))
      goto invalid;
    cp = ((uint32_t)(b0 & 0x07u) << 18) | ((uint32_t)(b1 & 0x3Fu) << 12) |
         ((uint32_t)(b2 & 0x3Fu) << 6) | (uint32_t)(b3 & 0x3Fu);
    if (cp < 0x10000u || cp > 0x10FFFFu)
      goto invalid;
    *out_cp = cp;
    *out_next = index + 4u;
    return true;
  }

invalid:
  *out_cp = STYGIAN_REPLACEMENT_CHAR;
  *out_next = index + 1u;
  return true;
}

bool stygian_utf8_next(const char *text, size_t text_len, size_t *io_index,
                       uint32_t *out_codepoint) {
  size_t next;
  uint32_t cp;
  if (!text || !io_index || !out_codepoint || *io_index >= text_len)
    return false;
  if (!unicode_decode_at(text, text_len, *io_index, &next, &cp))
    return false;
  *io_index = next;
  *out_codepoint = cp;
  return true;
}

static bool unicode_take_modifier_or_mark(const char *text, size_t text_len,
                                          size_t *io_index, uint32_t *io_flags) {
  size_t look = *io_index;
  size_t next = look;
  uint32_t cp = 0;
  if (!unicode_decode_at(text, text_len, look, &next, &cp))
    return false;

  if (unicode_is_variation_selector(cp)) {
    *io_index = next;
    *io_flags |= STYGIAN_GRAPHEME_HAS_VARIATION;
    return true;
  }
  if (unicode_is_skin_tone(cp)) {
    *io_index = next;
    *io_flags |= STYGIAN_GRAPHEME_HAS_SKIN_TONE;
    return true;
  }
  if (unicode_is_combining_mark(cp)) {
    *io_index = next;
    return true;
  }
  return false;
}

bool stygian_grapheme_next(const char *text, size_t text_len, size_t *io_index,
                           StygianGraphemeSpan *out_span) {
  size_t start, cur;
  uint32_t first = 0;
  uint32_t flags = 0;
  size_t next = 0;
  uint32_t cp = 0;
  bool paired_regional = false;

  if (!text || !io_index || !out_span || *io_index >= text_len)
    return false;

  start = *io_index;
  cur = start;
  if (!unicode_decode_at(text, text_len, cur, &cur, &first))
    return false;

  if (unicode_is_regional_indicator(first)) {
    size_t look = cur;
    if (unicode_decode_at(text, text_len, look, &next, &cp) &&
        unicode_is_regional_indicator(cp)) {
      cur = next;
      paired_regional = true;
      flags |= STYGIAN_GRAPHEME_IS_REGIONAL_PAIR;
    }
  }

  while (unicode_take_modifier_or_mark(text, text_len, &cur, &flags)) {
  }

  if (!paired_regional) {
    for (;;) {
      size_t look = cur;
      if (!unicode_decode_at(text, text_len, look, &next, &cp))
        break;
      if (!unicode_is_joiner(cp))
        break;

      flags |= STYGIAN_GRAPHEME_HAS_ZWJ;
      cur = next;

      if (!unicode_decode_at(text, text_len, cur, &next, &cp))
        break;
      cur = next;

      while (unicode_take_modifier_or_mark(text, text_len, &cur, &flags)) {
      }
    }
  }

  out_span->byte_start = start;
  out_span->byte_len = cur - start;
  out_span->first_codepoint = first;
  out_span->flags = flags;
  *io_index = cur;
  return true;
}

static const char *trim_outer_colons(const char *in, size_t *out_len) {
  size_t n;
  const char *start;
  const char *end;

  if (!in || !out_len)
    return NULL;

  start = in;
  while (*start && isspace((unsigned char)*start))
    start++;

  end = start + strlen(start);
  while (end > start && isspace((unsigned char)end[-1]))
    end--;

  if ((size_t)(end - start) >= 2u && start[0] == ':' && end[-1] == ':') {
    start++;
    end--;
  }

  n = (size_t)(end - start);
  *out_len = n;
  return start;
}

static bool is_hexish_char(char c) {
  c = (char)tolower((unsigned char)c);
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool stygian_shortcode_normalize(const char *input, char *out, size_t out_size) {
  const char *s;
  size_t n, i, o;
  bool has_payload = false;

  if (!input || !out || out_size < 10u)
    return false;

  s = trim_outer_colons(input, &n);
  if (!s || n == 0u)
    return false;

  if (n >= 7u && (tolower((unsigned char)s[0]) == 'e') &&
      (tolower((unsigned char)s[1]) == 'm') &&
      (tolower((unsigned char)s[2]) == 'o') &&
      (tolower((unsigned char)s[3]) == 'j') &&
      (tolower((unsigned char)s[4]) == 'i') && s[5] == '_' &&
      (tolower((unsigned char)s[6]) == 'u')) {
    o = 0u;
    for (i = 0u; i < n && o + 1u < out_size; i++) {
      char c = (char)tolower((unsigned char)s[i]);
      if (c == '-')
        c = '_';
      if (is_hexish_char(c) || c == '_' || (o < 7u && c >= 'a' && c <= 'z')) {
        out[o++] = c;
      } else {
        return false;
      }
    }
    if (o == 0u || o + 1u >= out_size)
      return false;
    out[o] = '\0';
    return true;
  }

  i = 0u;
  if (n >= 2u && (s[0] == 'u' || s[0] == 'U') && s[1] == '+')
    i = 2u;

  if (out_size < 8u)
    return false;
  memcpy(out, "emoji_u", 7u);
  o = 7u;

  for (; i < n && o + 1u < out_size; i++) {
    char c = s[i];
    if (c == '-' || c == '+' || c == ' ')
      c = '_';
    c = (char)tolower((unsigned char)c);
    if (is_hexish_char(c)) {
      out[o++] = c;
      has_payload = true;
    } else if (c == '_') {
      if (o > 7u && out[o - 1u] != '_')
        out[o++] = c;
    } else {
      return false;
    }
  }

  if (!has_payload || o + 1u >= out_size)
    return false;
  if (out[o - 1u] == '_')
    o--;
  out[o] = '\0';
  return true;
}
