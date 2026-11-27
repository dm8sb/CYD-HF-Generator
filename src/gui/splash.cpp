// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "splash.h"

#include "gui_layout.h"
#include <constants.h>
#include <stdio.h>
#include <utils/build_info.h>

namespace Splash {

namespace {

constexpr int WAVE_X = 56;                    // waves start right of the channel names
constexpr int WAVE_W = SCREEN_WIDTH - WAVE_X; // visible width
constexpr int PERIOD = 64;                    // CH1 and CH3 in px; CH2 has half of it
constexpr int TRACK_W = WAVE_W + 2 * PERIOD;  // the track scrolls left by one period
constexpr int WAVE_H = 22;
constexpr int WAVE_Y0 = 98;
constexpr int WAVE_PITCH = 34;
constexpr int MAX_POINTS = 2 * (TRACK_W / (PERIOD / 4) + 4);
constexpr uint32_t SCROLL_MS = 900; // one CH1 period
constexpr uint32_t FADE_MS = 300;

lv_point_t points[3][MAX_POINTS];
lv_obj_t* root = nullptr;
lv_timer_t* timer = nullptr;
bool fading = false;

// Square wave from x = -phase over the whole track: high = y 0, low = y WAVE_H.
uint16_t square(lv_point_t* p, int period, int phase) {
    const int half = period / 2;
    uint16_t n = 0;
    int x = -phase;
    bool high = true;
    p[n++] = {(lv_coord_t)x, 0};
    while (x < TRACK_W && n + 2 <= MAX_POINTS) {
        x += half;
        const lv_coord_t y = high ? 0 : WAVE_H;
        p[n++] = {(lv_coord_t)x, y};
        high = !high;
        p[n++] = {(lv_coord_t)x, (lv_coord_t)(high ? 0 : WAVE_H)};
    }
    return n;
}

void setOpa(void* obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); }
void setX(void* obj, int32_t v) { lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v); }

void removed(lv_anim_t*) {
    if (root) lv_obj_del(root);
    root = nullptr;
    fading = false;
}

void fadeOut() {
    if (!root || fading) return;
    fading = true;
    if (timer) {
        lv_timer_del(timer);
        timer = nullptr;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, root);
    lv_anim_set_exec_cb(&a, setOpa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, FADE_MS);
    lv_anim_set_ready_cb(&a, removed);
    lv_anim_start(&a);
}

void timeUp(lv_timer_t*) {
    timer = nullptr; // single shot: LVGL deletes it after this call
    fadeOut();
}

void tapped(lv_event_t*) { fadeOut(); }

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

} // namespace

bool shown() { return root != nullptr; }

void show(uint32_t durationMs) {
    if (root) return;
    root = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(root, Colors::screenBg(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, tapped, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = label(root, "HF Generator", &lv_font_montserrat_28, Colors::text());
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    char buf[64];
    snprintf(buf, sizeof(buf), "SI5351  |  3 outputs  |  %u kHz - %u MHz", (unsigned)(FREQ_MIN_HZ / 1000),
             (unsigned)(FREQ_MAX_HZ / 1000000));
    lv_obj_t* sub = label(root, buf, &lv_font_montserrat_14, Colors::muted());
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 62);

    // The waves run in a clipping window; the track inside moves left by one period and repeats.
    lv_obj_t* window = lv_obj_create(root);
    lv_obj_remove_style_all(window);
    lv_obj_set_pos(window, WAVE_X, WAVE_Y0 - 2);
    lv_obj_set_size(window, WAVE_W, 2 * WAVE_PITCH + WAVE_H + 4);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* track = lv_obj_create(window);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, TRACK_W, 2 * WAVE_PITCH + WAVE_H + 4);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    static const char* const NAMES[3] = {"CH1", "CH2", "CH3"};
    const int period[3] = {PERIOD, PERIOD / 2, PERIOD};
    const int phase[3] = {0, 0, PERIOD / 4}; // CH3 90 degrees behind CH1
    for (uint8_t i = 0; i < 3; ++i) {
        const lv_color_t color = Colors::forChannel(i + 1);
        lv_obj_t* name = label(root, NAMES[i], &lv_font_montserrat_16, color);
        lv_obj_set_pos(name, Layout::MARGIN, WAVE_Y0 + i * WAVE_PITCH + (WAVE_H - 16) / 2);
        lv_obj_t* line = lv_line_create(track);
        lv_line_set_points(line, points[i], square(points[i], period[i], phase[i]));
        lv_obj_set_pos(line, 0, 2 + i * WAVE_PITCH);
        lv_obj_set_style_line_width(line, 3, 0);
        lv_obj_set_style_line_color(line, color, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, track);
    lv_anim_set_exec_cb(&a, setX);
    lv_anim_set_values(&a, 0, -PERIOD);
    lv_anim_set_time(&a, SCROLL_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    snprintf(buf, sizeof(buf), "Firmware %s", BuildInfo::version());
    lv_obj_t* ver = label(root, buf, &lv_font_montserrat_12, Colors::muted());
    lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -10);

    timer = lv_timer_create(timeUp, durationMs, nullptr);
    lv_timer_set_repeat_count(timer, 1);
}

} // namespace Splash
