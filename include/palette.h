#pragma once

#include <stdint.h>

// Color palette as 0xRRGGBB (PROJECT_SPECIFICATION.md A-27). The GUI turns them into lv_color_t
// (gui_layout.h); test_contrast checks the pairs the pages use (audit U-7).
namespace Palette {
static constexpr uint32_t CH1 = 0xE53935;
static constexpr uint32_t CH2 = 0x43A047;
static constexpr uint32_t CH3 = 0x1E88E5;
static constexpr uint32_t CONFIG = 0xFDD835;
static constexpr uint32_t GRAY = 0x455A64;
static constexpr uint32_t SCREEN_BG = 0x0B0F14;
static constexpr uint32_t HEADER_BG = 0x111820;
static constexpr uint32_t CARD_BG = 0x151A22;
static constexpr uint32_t CARD_PRESSED = 0x222B38;
static constexpr uint32_t CHIP_OFF = 0x2A3340;
static constexpr uint32_t TEXT = 0xECEFF1;
static constexpr uint32_t TEXT_DARK = 0x111820;
static constexpr uint32_t MUTED = 0x90A4AE;
static constexpr uint32_t TONE_OK = 0x66BB6A;
static constexpr uint32_t TONE_WARN = 0xFFA726;
static constexpr uint32_t TONE_ERROR = 0xEF5350;
// Switch track (off) and chip border: at least 3:1 against every background and the knob.
static constexpr uint32_t OUTLINE = 0x78909C;
static constexpr uint32_t KNOB = 0xFFFFFF;
// Dialog buttons (audit U-4).
static constexpr uint32_t DIALOG_BUTTON = 0x90A4AE;
}
