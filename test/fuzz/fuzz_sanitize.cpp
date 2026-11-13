// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target sanitize: Sanitize::text (W-S9).
// Input: byte 0 = maxBytes (0..63; bit 7: in = nullptr), byte 1 = outSize (0..71), rest = text.
// Invariants: never writes past outSize; result <= min(maxBytes, outSize - 1), NUL-terminated,
// strictly valid UTF-8 without control characters; idempotent; valid text that fits comes back
// unchanged; a shorter limit only cuts the full result at a character boundary.

#include "fuzz_common.h"

#include <string>
#include <vector>
#include <utils/sanitize.h>

namespace fuzz_sanitize {

int run(const uint8_t* data, size_t size) {
    if (size < 2) return 0;
    const bool nullInput = (data[0] & 0x80) != 0;
    const size_t maxBytes = data[0] & 0x3F;
    const size_t outSize = data[1] % 72;
    const char* in = nullInput ? nullptr : reinterpret_cast<const char*>(data + 2);
    const size_t inLen = size - 2;

    // Exactly outSize bytes on the heap (ASan); outSize 0 gets a guard byte that must stay.
    std::vector<char> out(outSize ? outSize : 1, '#');
    const size_t n = Sanitize::text(in, inLen, out.data(), outSize, maxBytes);
    if (outSize == 0) {
        FUZZ_CHECK(n == 0 && out[0] == '#');
        FUZZ_CHECK(Sanitize::text(in, inLen, nullptr, 8, 4) == 0);
        return 0;
    }
    const size_t limit = maxBytes < outSize - 1 ? maxBytes : outSize - 1;
    FUZZ_CHECK(n <= limit);
    FUZZ_CHECK(out[n] == '\0');
    FUZZ_CHECK(strlen(out.data()) == n);
    FUZZ_CHECK(fuzzValidUtf8(out.data(), n, true));
    if (!in) {
        FUZZ_CHECK(n == 0);
        return 0;
    }

    // Idempotent.
    std::vector<char> again(outSize, '#');
    FUZZ_CHECK(Sanitize::text(out.data(), n, again.data(), outSize, maxBytes) == n);
    FUZZ_CHECK(memcmp(again.data(), out.data(), n + 1) == 0);

    // Valid text without control characters that fits is copied unchanged.
    if (inLen <= limit && fuzzValidUtf8(in, inLen, true)) {
        FUZZ_CHECK(n == inLen && memcmp(out.data(), in, inLen) == 0);
    }

    // The limited result is a prefix of the unlimited one, cut before a character that does
    // not fit completely.
    std::vector<char> full(inLen + 1, '#');
    const size_t fullLen = Sanitize::text(in, inLen, full.data(), full.size(), inLen);
    FUZZ_CHECK(fullLen <= inLen);
    FUZZ_CHECK(n <= fullLen && memcmp(full.data(), out.data(), n) == 0);
    if (n < fullLen) FUZZ_CHECK(n + fuzzUtf8Len((unsigned char)full[n]) > limit);
    return 0;
}

} // namespace fuzz_sanitize

FUZZ_TARGET(sanitize, fuzz_sanitize::run)
