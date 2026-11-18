// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ipv4.h"

#include <stdio.h>
#include <string.h>

namespace Ipv4 {

bool parse(const char* s, size_t n, uint32_t* out) {
    if (!s || !out) return false;
    uint32_t v = 0;
    size_t i = 0;
    for (int part = 0; part < 4; ++part) {
        if (i >= n || s[i] < '0' || s[i] > '9') return false;
        const size_t start = i;
        uint32_t x = 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            if (i - start == 3) return false;
            x = x * 10 + (uint32_t)(s[i] - '0');
            ++i;
        }
        if (x > 255 || (i - start > 1 && s[start] == '0')) return false; // no leading zeros (octal)
        v = (v << 8) | x;
        if (part < 3) {
            if (i >= n || s[i] != '.') return false;
            ++i;
        }
    }
    if (i != n) return false;
    *out = v;
    return true;
}

bool parse(const char* text, uint32_t* out) { return text && parse(text, strlen(text), out); }

bool format(uint32_t ip, char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    const int w = snprintf(out, outSize, "%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)((ip >> 16) & 255),
                           (unsigned)((ip >> 8) & 255), (unsigned)(ip & 255));
    return w > 0 && (size_t)w < outSize;
}

} // namespace Ipv4
