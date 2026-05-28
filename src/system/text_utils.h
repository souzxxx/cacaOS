#pragma once

#include <stddef.h>

/**
 * Fold UTF-8 Portuguese text to plain 7-bit ASCII so it renders with the
 * default Montserrat fonts (which only ship glyphs for 0x20..0x7E).
 *
 * - Accented letters (á, à, â, ã, é, ê, í, ó, ô, õ, ú, ç, …) become their
 *   unaccented ASCII equivalent.
 * - Curly quotes, em/en dashes, ellipsis become their ASCII counterparts.
 * - Anything else outside ASCII (emoji, symbols) is dropped silently.
 *
 * Writes at most `out_size - 1` bytes plus a NUL terminator. Returns the
 * number of bytes written (excluding the terminator).
 */
size_t text_ascii_fold(const char* src, char* out, size_t out_size);
