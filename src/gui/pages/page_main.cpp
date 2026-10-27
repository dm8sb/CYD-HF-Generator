#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>

using namespace Layout;

static constexpr int CARD_H = 58;
static constexpr int NAV_Y = 194;
static constexpr int NAV_H = 42;
static constexpr int NAV_W = 72;

void Gui::createMain_() {
    // Main page intentionally has no project-title header to maximize status space.
    static const PageId pages[3] = {PageId::CH1, PageId::CH2, PageId::CH3};
    static const char* names[3] = {"CH1", "CH2", "CH3"};

    for (uint8_t i = 0; i < 3; ++i) {
        const uint8_t ch = i + 1;
        const lv_color_t color = Colors::forChannel(ch);
        lv_obj_t* card = addCard_(root_, MARGIN, 6 + i * (CARD_H + 4), CONTENT_W, CARD_H, color);
        // Tapping the card opens the channel page; the switch toggles the output directly.
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(card, Colors::cardPressed(), LV_STATE_PRESSED);
        on_(card, LV_EVENT_CLICKED, Action::Navigate, (uint8_t)pages[i]);

        addLabel_(card, names[i], &lv_font_montserrat_20, color, 0, 0);
        w_.cardState[i] = addLabel_(card, "", &lv_font_montserrat_12, Colors::muted(), 0, 26);
        w_.cardFreq[i] = addLabel_(card, "", &lv_font_montserrat_20, Colors::text(), 58, 0);
        w_.cardDetail[i] = addLabel_(card, "", &lv_font_montserrat_12, Colors::muted(), 58, 26);
        // Keep the detail line clear of the switch (50 px at the right edge).
        lv_obj_set_width(w_.cardDetail[i], 172);
        lv_label_set_long_mode(w_.cardDetail[i], LV_LABEL_LONG_DOT);
        w_.cardSwitch[i] = addSwitch_(card, ch, color);
        lv_obj_align(w_.cardSwitch[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    addButton_(root_, "CH1", MARGIN, NAV_Y, NAV_W, NAV_H, Colors::ch1(), Action::Navigate, (uint8_t)PageId::CH1);
    addButton_(root_, "CH2", MARGIN + (NAV_W + GAP), NAV_Y, NAV_W, NAV_H, Colors::ch2(), Action::Navigate, (uint8_t)PageId::CH2);
    addButton_(root_, "CH3", MARGIN + 2 * (NAV_W + GAP), NAV_Y, NAV_W, NAV_H, Colors::ch3(), Action::Navigate, (uint8_t)PageId::CH3);
    addButton_(root_, "CONFIG", MARGIN + 3 * (NAV_W + GAP), NAV_Y, NAV_W, NAV_H, Colors::config(), Action::Navigate, (uint8_t)PageId::Config);
}

static void setState(lv_obj_t* label, const char* text, lv_color_t color) {
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
}

void Gui::refreshMain_() {
    const AppController& app = *app_;
    char buf[48];

    for (uint8_t ch = 1; ch <= 2; ++ch) {
        const uint8_t i = ch - 1;
        const Channel& c = app.channel(ch);
        FrequencyUtils::format(c.frequency, buf, sizeof(buf));
        lv_label_set_text(w_.cardFreq[i], buf);
        if (!app.hardwareReady()) setState(w_.cardState[i], "NO HW", Colors::error());
        else if (c.enabled) setState(w_.cardState[i], "ON", Colors::ok());
        else setState(w_.cardState[i], "OFF", Colors::muted());
        const bool iqMaster = app.ch3().master == ch && app.ch3Status() != Ch3Status::Off;
        lv_label_set_text(w_.cardDetail[i], iqMaster ? "IQ master of CH3" : "");
        if (c.enabled) lv_obj_add_state(w_.cardSwitch[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.cardSwitch[i], LV_STATE_CHECKED);
    }

    const Channel3& c3 = app.ch3();
    FrequencyUtils::format(app.ch3Frequency(), buf, sizeof(buf));
    lv_label_set_text(w_.cardFreq[2], buf);
    const char* reason = "";
    switch (app.ch3Status()) {
        case Ch3Status::Off: setState(w_.cardState[2], "OFF", Colors::muted()); break;
        case Ch3Status::Active: setState(w_.cardState[2], "ON", Colors::ok()); break;
        case Ch3Status::MasterOff: setState(w_.cardState[2], "WAIT", Colors::warn()); reason = " / master off"; break;
        case Ch3Status::FreqTooLow: setState(w_.cardState[2], "WAIT", Colors::warn()); reason = " / < 4.73 MHz"; break;
        case Ch3Status::NoDevice: setState(w_.cardState[2], "NO HW", Colors::error()); break;
    }
    snprintf(buf, sizeof(buf), "CH%u / %u deg%s", c3.master, c3.phase, reason);
    lv_label_set_text(w_.cardDetail[2], buf);
    if (c3.enabled) lv_obj_add_state(w_.cardSwitch[2], LV_STATE_CHECKED);
    else lv_obj_clear_state(w_.cardSwitch[2], LV_STATE_CHECKED);
}
