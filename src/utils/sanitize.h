// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>

// Foreign strings (SSIDs from a scan, names from the network) before they reach LVGL or JSON
// (W-S9, S-7, S-14). Pure, host-tested and later fuzzed.
namespace Sanitize {

// Copies at most maxBytes bytes of valid UTF-8 into out (NUL-terminated, outSize > maxBytes):
// invalid or truncated sequences, overlong forms, surrogates and code points above U+10FFFF
// become '?', control characters (C0, DEL, C1) become '?', a character that does not fit
// completely is left out. Returns the length written.
size_t text(const char* in, size_t inLen, char* out, size_t outSize, size_t maxBytes = 32);

}
