/**
 * @file text_utils.cpp
 * @brief UTF-8 → ASCII folding for Portuguese strings.
 *
 * The LVGL Montserrat fonts compiled into the firmware only carry glyphs for
 * basic Latin (0x20..0x7E). Anything outside that range — accented letters,
 * smart quotes, emoji — renders as a placeholder box. We strip/transliterate
 * at load time instead of shipping a custom font (which would push the binary
 * past the partition budget).
 */

#include "text_utils.h"

#include <stdint.h>
#include <string.h>

namespace {

// Returns the single-byte ASCII replacement for a code point, or 0 to drop.
char fold_codepoint(uint32_t cp) {
    // Direct ASCII
    if (cp < 0x80) return (char)cp;

    switch (cp) {
        // Latin-1 lowercase vowels (with accent/tilde/diaeresis)
        case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3:
        case 0x00E4: case 0x00E5:                              return 'a';
        case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:    return 'e';
        case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:    return 'i';
        case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5:
        case 0x00F6:                                            return 'o';
        case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:    return 'u';
        case 0x00FD: case 0x00FF:                              return 'y';
        case 0x00E7:                                            return 'c';
        case 0x00F1:                                            return 'n';
        // Latin-1 uppercase
        case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3:
        case 0x00C4: case 0x00C5:                              return 'A';
        case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:    return 'E';
        case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:    return 'I';
        case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5:
        case 0x00D6:                                            return 'O';
        case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:    return 'U';
        case 0x00DD:                                            return 'Y';
        case 0x00C7:                                            return 'C';
        case 0x00D1:                                            return 'N';
        // Punctuation
        case 0x2018: case 0x2019: case 0x201A:                 return '\'';
        case 0x201C: case 0x201D: case 0x201E:                 return '"';
        case 0x2013: case 0x2014: case 0x2212:                 return '-';
        case 0x2026:                                            return '.'; // ellipsis → single dot (caller can repeat)
        case 0x00A0:                                            return ' ';
        default:                                                return 0;
    }
}

// Decode one UTF-8 sequence starting at *p. Advances *p past the consumed
// bytes. On malformed input, advances by one byte and returns 0xFFFD.
uint32_t utf8_next(const char** p) {
    const uint8_t* s = (const uint8_t*)*p;
    uint8_t b0 = s[0];
    if (b0 == 0) { return 0; }
    if (b0 < 0x80) { *p += 1; return b0; }

    int extra;
    uint32_t cp;
    if      ((b0 & 0xE0) == 0xC0) { extra = 1; cp = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { extra = 2; cp = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { extra = 3; cp = b0 & 0x07; }
    else                          { *p += 1; return 0xFFFD; }

    for (int i = 1; i <= extra; ++i) {
        uint8_t b = s[i];
        if ((b & 0xC0) != 0x80) { *p += 1; return 0xFFFD; }
        cp = (cp << 6) | (b & 0x3F);
    }
    *p += (extra + 1);
    return cp;
}

} // namespace

size_t text_ascii_fold(const char* src, char* out, size_t out_size) {
    if (!out || out_size == 0) return 0;
    if (!src) { out[0] = '\0'; return 0; }

    size_t w = 0;
    const char* p = src;
    while (*p && w + 1 < out_size) {
        uint32_t cp = utf8_next(&p);
        if (cp == 0) break;
        char c = fold_codepoint(cp);
        if (c == 0) continue;       // drop emoji / unknown
        out[w++] = c;
    }
    out[w] = '\0';
    return w;
}
