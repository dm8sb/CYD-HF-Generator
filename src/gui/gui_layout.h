#pragma once

#include <lvgl.h>

// Layout grid for the 320x240 landscape screen. All page coordinates derive from these values.
namespace Layout {
static constexpr int MARGIN = 8;
static constexpr int GAP = 6;
static constexpr int HEADER_H = 38;
static constexpr int CONTENT_Y = HEADER_H + 4;
static constexpr int CONTENT_W = 320 - 2 * MARGIN;
static constexpr int CARD_PAD = 6;
static constexpr int CARD_BORDER = 2;
}

// Color scheme (PROJECT_SPECIFICATION.md 11.3).
namespace Colors {
inline lv_color_t ch1() { return lv_color_hex(0xE53935); }
inline lv_color_t ch2() { return lv_color_hex(0x43A047); }
inline lv_color_t ch3() { return lv_color_hex(0x1E88E5); }
inline lv_color_t config() { return lv_color_hex(0xFDD835); }
inline lv_color_t gray() { return lv_color_hex(0x455A64); }
inline lv_color_t screenBg() { return lv_color_hex(0x0B0F14); }
inline lv_color_t headerBg() { return lv_color_hex(0x111820); }
inline lv_color_t cardBg() { return lv_color_hex(0x151A22); }
inline lv_color_t cardPressed() { return lv_color_hex(0x222B38); }
inline lv_color_t chipOff() { return lv_color_hex(0x2A3340); }
inline lv_color_t text() { return lv_color_hex(0xECEFF1); }
inline lv_color_t textDark() { return lv_color_hex(0x111820); }
inline lv_color_t muted() { return lv_color_hex(0x90A4AE); }
inline lv_color_t ok() { return lv_color_hex(0x66BB6A); }
inline lv_color_t warn() { return lv_color_hex(0xFFA726); }
inline lv_color_t error() { return lv_color_hex(0xEF5350); }

inline lv_color_t forChannel(uint8_t ch) { return ch == 1 ? ch1() : (ch == 2 ? ch2() : ch3()); }
}
