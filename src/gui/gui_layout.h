// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lvgl.h>
#include <string.h>
#include <palette.h>
#include <utils/contrast.h>
#include <utils/status_text.h>

// Layout grid for the 320x240 landscape screen. All page coordinates derive from these values.
namespace Layout {
static constexpr int MARGIN = 8;
static constexpr int GAP = 6;
static constexpr int HEADER_H = 38;
static constexpr int CONTENT_Y = HEADER_H + 4;
static constexpr int CONTENT_W = 320 - 2 * MARGIN;
static constexpr int CARD_PAD = 6;
static constexpr int CARD_BORDER = 2;
// Header of every page except MAIN (one rule for all levels): the back button at the top left
// goes one level up (CH1-CH3 and CONFIG: MAIN), the title follows, page actions sit at the right.
// The back button is 36 px high and reaches the top and left edge (U-5).
static constexpr int BACK_X = 4;
static constexpr int BACK_Y = 1;
static constexpr int BACK_W = 52;
static constexpr int BACK_H = HEADER_H - 2;
static constexpr int HEADER_TITLE_X = BACK_X + BACK_W + 8;
// Switch and state text right after the title (U-1); a wider title ("CH3 IQ") moves them right.
static constexpr int HEADER_TITLE_GAP = 6;
static constexpr int HEADER_STATE_DX = 60; // state text after the switch
// Frequency card of the channel and CH3 pages: frequency (28 px), Out line, mode line.
static constexpr int CARD_FREQ_H = 80;
static constexpr int CARD_TEXT_X = 2;
static constexpr int CARD_INFO_Y = 32;
static constexpr int CARD_MODES_Y = 50;
}

// Color scheme (PROJECT_SPECIFICATION.md A-27), values in palette.h.
namespace Colors {
inline lv_color_t ch1() { return lv_color_hex(Palette::CH1); }
inline lv_color_t ch2() { return lv_color_hex(Palette::CH2); }
inline lv_color_t ch3() { return lv_color_hex(Palette::CH3); }
inline lv_color_t config() { return lv_color_hex(Palette::CONFIG); }
inline lv_color_t gray() { return lv_color_hex(Palette::GRAY); }
inline lv_color_t screenBg() { return lv_color_hex(Palette::SCREEN_BG); }
inline lv_color_t headerBg() { return lv_color_hex(Palette::HEADER_BG); }
inline lv_color_t cardBg() { return lv_color_hex(Palette::CARD_BG); }
inline lv_color_t cardPressed() { return lv_color_hex(Palette::CARD_PRESSED); }
inline lv_color_t chipOff() { return lv_color_hex(Palette::CHIP_OFF); }
inline lv_color_t text() { return lv_color_hex(Palette::TEXT); }
inline lv_color_t textDark() { return lv_color_hex(Palette::TEXT_DARK); }
inline lv_color_t muted() { return lv_color_hex(Palette::MUTED); }
inline lv_color_t ok() { return lv_color_hex(Palette::TONE_OK); }
inline lv_color_t warn() { return lv_color_hex(Palette::TONE_WARN); }
inline lv_color_t error() { return lv_color_hex(Palette::TONE_ERROR); }
// Switch track (off) and chip border, at least 3:1 (audit U-7).
inline lv_color_t outline() { return lv_color_hex(Palette::OUTLINE); }
inline lv_color_t knob() { return lv_color_hex(Palette::KNOB); }
// Dialog buttons (audit U-4): 6.7:1 against cardBg(), dark text 6.9:1.
inline lv_color_t dialogButton() { return lv_color_hex(Palette::DIALOG_BUTTON); }
// Text color with the higher contrast on bg: dark on the channel colors (audit U-7).
inline lv_color_t textOn(lv_color_t bg) { return lv_color_hex(Contrast::textOn(lv_color_to32(bg) & 0xFFFFFF)); }

inline lv_color_t forChannel(uint8_t ch) { return ch == 1 ? ch1() : (ch == 2 ? ch2() : ch3()); }
inline lv_color_t forTone(StatusText::Tone t) {
    switch (t) {
        case StatusText::Tone::Ok: return ok();
        case StatusText::Tone::Warn: return warn();
        case StatusText::Tone::Error: return error();
        case StatusText::Tone::Muted: break;
    }
    return muted();
}
}

// Setters that touch a widget only when the value changes (AF12, PERF-2/16): in LVGL 8.4
// lv_label_set_text() and the local style setters invalidate the widget even for the same
// value, and every invalidated area is a SPI burst to the display next to the SI5351.
namespace Ui {
inline void setText(lv_obj_t* label, const char* text) {
    const char* now = lv_label_get_text(label);
    if (!now || !text || strcmp(now, text) != 0) lv_label_set_text(label, text);
}
inline void setTextColor(lv_obj_t* obj, lv_color_t c) {
    if (lv_obj_get_style_text_color(obj, LV_PART_MAIN).full != c.full) lv_obj_set_style_text_color(obj, c, 0);
}
inline void setBgColor(lv_obj_t* obj, lv_color_t c) {
    if (lv_obj_get_style_bg_color(obj, LV_PART_MAIN).full != c.full) lv_obj_set_style_bg_color(obj, c, 0);
}
inline void setBgOpa(lv_obj_t* obj, lv_opa_t opa) {
    if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) != opa) lv_obj_set_style_bg_opa(obj, opa, 0);
}
inline void setHidden(lv_obj_t* obj, bool hidden) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
}
