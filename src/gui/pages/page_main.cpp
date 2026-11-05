#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <math.h>

using namespace Layout;

// Three cards above the status bar (W-E26); the bar replaces the navigation buttons CH1-CH3,
// the cards open the channel pages.
static constexpr int CARD_H = 62;
static constexpr int CARD_Y = 4;
static constexpr int BAR_H = 38;
static constexpr int BAR_Y = SCREEN_HEIGHT - BAR_H;
static_assert(CARD_Y + 3 * CARD_H + 3 * 4 <= BAR_Y, "cards above the status bar");
// Second row: state (14 px, filled, audit U-8) and detail line (14 px). The detail starts after
// the widest usual state ("WAIT", 43 px with padding) and moves right only for "! NO HW".
static constexpr int ROW2_Y = 27;
static constexpr int DETAIL_X = 50;
// Switch column at the right edge of the card content (288 px): a 44 px switch with 6 px extra
// touch area starts at 238, 13 px after the longest frequency ("100.000 000 MHz" ends at 225),
// so a tap on the unit cannot switch the output (audit U-3). A line marks the column.
static constexpr int SWITCH_W = 44;
static constexpr int SWITCH_H = 24;
static constexpr int DIVIDER_X = 231;
// Frequencies are right-aligned before the divider, so point and unit line up on all cards and
// a "~" (Integer raster) has room before two-digit MHz values. The label starts after the name.
static constexpr int FREQ_RIGHT = DIVIDER_X - 6;
static constexpr int NAME_W = 42;

void Gui::createMain_() {
    // Main page intentionally has no project-title header to maximize status space.
    static const PageId pages[3] = {PageId::CH1, PageId::CH2, PageId::CH3};
    static const char* names[3] = {"CH1", "CH2", "CH3"};

    for (uint8_t i = 0; i < 3; ++i) {
        const uint8_t ch = i + 1;
        const lv_color_t color = Colors::forChannel(ch);
        lv_obj_t* card = addCard_(root_, MARGIN, CARD_Y + i * (CARD_H + 4), CONTENT_W, CARD_H, color);
        // Tapping the card opens the channel page; the switch toggles the output directly.
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(card, Colors::cardPressed(), LV_STATE_PRESSED);
        on_(card, LV_EVENT_CLICKED, Action::Navigate, (uint8_t)pages[i]);

        addLabel_(card, names[i], &lv_font_montserrat_20, color, 0, 0);
        w_.cardState[i] = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), 0, ROW2_Y);
        styleState_(w_.cardState[i]);
        w_.cardFreq[i] = addLabel_(card, "", &lv_font_montserrat_20, Colors::text(), NAME_W, 0);
        lv_obj_set_width(w_.cardFreq[i], FREQ_RIGHT - NAME_W);
        lv_obj_set_style_text_align(w_.cardFreq[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(w_.cardFreq[i], LV_LABEL_LONG_CLIP);
        w_.cardDetail[i] = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), DETAIL_X, ROW2_Y);
        // Keep the detail line clear of the switch column.
        lv_obj_set_width(w_.cardDetail[i], DIVIDER_X - 4 - DETAIL_X);
        lv_label_set_long_mode(w_.cardDetail[i], LV_LABEL_LONG_DOT);
        lv_obj_t* divider = lv_obj_create(card);
        lv_obj_remove_style_all(divider);
        lv_obj_set_style_bg_color(divider, Colors::chipOff(), 0);
        lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
        lv_obj_set_size(divider, 1, CARD_H - 2 * (CARD_PAD + CARD_BORDER));
        lv_obj_set_pos(divider, DIVIDER_X, 0);
        lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
        w_.cardSwitch[i] = addSwitch_(card, ch, color);
        lv_obj_set_size(w_.cardSwitch[i], SWITCH_W, SWITCH_H);
        lv_obj_align(w_.cardSwitch[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Status bar like the header: save state on the left (later WLAN symbols, S53), CONFIG at the
    // right edge.
    lv_obj_t* bar = lv_obj_create(root_);
    lv_obj_set_pos(bar, 0, BAR_Y);
    lv_obj_set_size(bar, SCREEN_WIDTH, BAR_H);
    lv_obj_set_style_bg_color(bar, Colors::headerBg(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    w_.saveState = addLabel_(bar, "", &lv_font_montserrat_14, Colors::muted(), 0, 0);
    lv_obj_align(w_.saveState, LV_ALIGN_LEFT_MID, MARGIN + 2, 0);
    addButton_(bar, "CONFIG", SCREEN_WIDTH - 86, 1, 82, BAR_H - 2, Colors::config(), Action::Navigate,
               (uint8_t)PageId::Config);
}

// Detail line after the state label: at DETAIL_X, further right after a wider state.
static void placeDetail(lv_obj_t* detail, lv_obj_t* state) {
    lv_point_t size;
    lv_txt_get_size(&size, lv_label_get_text(state), &lv_font_montserrat_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int x = LV_MAX(DETAIL_X, size.x + 2 * 2 + 6); // state padding 2 px per side
    lv_obj_set_x(detail, x);
    lv_obj_set_width(detail, DIVIDER_X - 4 - x);
}

// The requested frequency of a card. In Integer mode the output lies on a raster; "~" marks
// a deviation from 1 Hz on. The label is right-aligned, so the digits stay aligned. From
// 100 MHz on "~100.000 000 MHz" (187 px) does not fit the 183 px column; the mode line shows
// "Integer" there anyway.
static void setCardFrequency(lv_obj_t* label, const AppController& app, uint8_t ch, uint64_t requested) {
    const double actual = app.actualHz(ch);
    const bool approx = app.system().tuning_mode == TuningMode::Integer && app.hardwareReady() && actual > 0 &&
                        requested < 100000000ULL &&
                        fabs(FrequencyUtils::deviation(actual, requested)) >= FrequencyUtils::DEVIATION_WARN_HZ;
    char buf[24];
    buf[0] = '~';
    FrequencyUtils::format(requested, buf + 1, sizeof(buf) - 1);
    lv_label_set_text(label, approx ? buf : buf + 1);
}

void Gui::refreshMain_() {
    const AppController& app = *app_;
    char buf[48];

    for (uint8_t ch = 1; ch <= 2; ++ch) {
        const uint8_t i = ch - 1;
        const Channel& c = app.channel(ch);
        setCardFrequency(w_.cardFreq[i], app, ch, c.frequency);
        showState_(w_.cardState[i], StatusText::channel(app.channelStatus(ch)));
        // Special modes first (they change the signal); a long line ends in "...".
        showModes_(w_.cardDetail[i], ch);
        const bool iqMaster = app.ch3().master == ch && app.ch3Status() != Ch3Status::Off;
        if (iqMaster) {
            const char* modes = lv_label_get_text(w_.cardDetail[i]);
            if (modes[0]) {
                snprintf(buf, sizeof(buf), "%s | IQ master", modes);
                lv_label_set_text(w_.cardDetail[i], buf);
            } else {
                lv_label_set_text(w_.cardDetail[i], "IQ master of CH3");
            }
        }
        placeDetail(w_.cardDetail[i], w_.cardState[i]);
        if (c.enabled) lv_obj_add_state(w_.cardSwitch[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.cardSwitch[i], LV_STATE_CHECKED);
    }

    const Channel3& c3 = app.ch3();
    // CLK2 has an actual frequency only while IQ runs.
    setCardFrequency(w_.cardFreq[2], app, 3, app.ch3Frequency());
    char iqLimit[12], tooLow[24], tooHigh[24];
    FrequencyUtils::formatIqLimit(app.iqMinHz(), true, 2, false, iqLimit, sizeof(iqLimit));
    snprintf(tooLow, sizeof(tooLow), " / < %s MHz", iqLimit);
    FrequencyUtils::formatIqLimit(app.iqMaxHz(), false, 2, false, iqLimit, sizeof(iqLimit));
    snprintf(tooHigh, sizeof(tooHigh), " / > %s MHz", iqLimit);
    const Ch3Status st3 = app.ch3Status();
    showState_(w_.cardState[2], StatusText::ch3(st3));
    const char* reason = "";
    switch (st3) {
        case Ch3Status::MasterOff: reason = " / master off"; break;
        case Ch3Status::FreqTooLow: reason = tooLow; break;
        case Ch3Status::FreqTooHigh: reason = tooHigh; break;
        case Ch3Status::Error: reason = " / setup failed"; break;
        case Ch3Status::Off:
        case Ch3Status::Active:
        case Ch3Status::NoDevice: break;
    }
    snprintf(buf, sizeof(buf), "CH%u / %u\xC2\xB0%s", c3.master, c3.phase, reason);
    lv_label_set_text(w_.cardDetail[2], buf);
    placeDetail(w_.cardDetail[2], w_.cardState[2]);
    if (c3.enabled) lv_obj_add_state(w_.cardSwitch[2], LV_STATE_CHECKED);
    else lv_obj_clear_state(w_.cardSwitch[2], LV_STATE_CHECKED);
    refreshSaveState_();
}
