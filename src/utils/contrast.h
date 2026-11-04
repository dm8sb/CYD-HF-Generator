#pragma once

#include <stdint.h>

// WCAG 2.x contrast of 0xRRGGBB colors (audit U-7). Pure functions, host-tested in test_contrast.
namespace Contrast {

// Contrast ratio 1..21 of two colors, independent of the order.
float ratio(uint32_t a, uint32_t b);
// Palette::TEXT or Palette::TEXT_DARK, whichever has the higher contrast on bg.
uint32_t textOn(uint32_t bg);

}
