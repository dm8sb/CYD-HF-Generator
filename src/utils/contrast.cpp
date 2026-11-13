// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contrast.h"

#include <math.h>
#include <palette.h>

namespace Contrast {

// sRGB channel 0..255 to linear light (WCAG 2.x relative luminance).
static float linear(uint32_t c) {
    const float v = (float)c / 255.0f;
    return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}

static float luminance(uint32_t rgb) {
    return 0.2126f * linear((rgb >> 16) & 0xFF) + 0.7152f * linear((rgb >> 8) & 0xFF) + 0.0722f * linear(rgb & 0xFF);
}

float ratio(uint32_t a, uint32_t b) {
    const float la = luminance(a);
    const float lb = luminance(b);
    return la > lb ? (la + 0.05f) / (lb + 0.05f) : (lb + 0.05f) / (la + 0.05f);
}

uint32_t textOn(uint32_t bg) {
    return ratio(Palette::TEXT_DARK, bg) > ratio(Palette::TEXT, bg) ? Palette::TEXT_DARK : Palette::TEXT;
}

}
