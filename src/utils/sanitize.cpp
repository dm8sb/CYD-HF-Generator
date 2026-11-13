// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sanitize.h"

namespace Sanitize {

namespace {

// Length of the valid UTF-8 sequence at p (1..4), 0 if invalid; *cp gets the code point.
size_t decode(const unsigned char* p, size_t n, unsigned long* cp) {
    const unsigned char c = p[0];
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    size_t len;
    unsigned long v;
    unsigned long min;
    if ((c & 0xE0) == 0xC0) {
        len = 2;
        v = c & 0x1F;
        min = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        len = 3;
        v = c & 0x0F;
        min = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        len = 4;
        v = c & 0x07;
        min = 0x10000;
    } else {
        return 0; // continuation byte or 0xF8..0xFF
    }
    if (len > n) return 0; // truncated
    for (size_t i = 1; i < len; ++i) {
        if ((p[i] & 0xC0) != 0x80) return 0;
        v = (v << 6) | (p[i] & 0x3F);
    }
    if (v < min || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return 0;
    *cp = v;
    return len;
}

bool control(unsigned long cp) { return cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0); }

}

size_t text(const char* in, size_t inLen, char* out, size_t outSize, size_t maxBytes) {
    if (!out || outSize == 0) return 0;
    if (maxBytes > outSize - 1) maxBytes = outSize - 1;
    size_t o = 0;
    const unsigned char* p = (const unsigned char*)in;
    size_t i = 0;
    while (in && i < inLen) {
        unsigned long cp = 0;
        size_t len = decode(p + i, inLen - i, &cp);
        const bool bad = len == 0 || control(cp);
        const size_t outLen = bad ? 1 : len;
        if (o + outLen > maxBytes) break;
        if (bad) {
            out[o++] = '?';
            i += len ? len : 1;
        } else {
            for (size_t k = 0; k < len; ++k) out[o++] = (char)p[i + k];
            i += len;
        }
    }
    out[o] = '\0';
    return o;
}

}
