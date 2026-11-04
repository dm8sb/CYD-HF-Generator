#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <math.h>

using namespace Layout;

static constexpr int CARD_H = 58;
static constexpr int FREQ_X = 58;
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
        w_.cardFreq[i] = addLabel_(card, "", &lv_font_montserrat_20, Colors::text(), FREQ_X, 0);
        w_.cardDetail[i] = addLabel_(card, "", &lv_font_montserrat_12, Colors::muted(), FREQ_X, 26);
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

// The requested frequency of a card. In Integer mode the output lies on a raster; "~" marks
// a deviation from 1 Hz on. It sits left of the digits, which stay aligned with the other cards.
static void setCardFrequency(lv_obj_t* label, const AppController& app, uint8_t ch, uint64_t requested) {
    const double actual = app.actualHz(ch);
    const bool approx = app.system().tuning_mode == TuningMode::Integer && app.hardwareReady() && actual > 0 &&
                        fabs(FrequencyUtils::deviation(actual, requested)) >= FrequencyUtils::DEVIATION_WARN_HZ;
    char buf[24];
    buf[0] = '~';
    FrequencyUtils::format(requested, buf + 1, sizeof(buf) - 1);
    lv_label_set_text(label, approx ? buf : buf + 1);
    const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_obj_set_x(label, FREQ_X - (approx ? lv_txt_get_width("~", 1, font, 0, LV_TEXT_FLAG_NONE) : 0));
}

void Gui::refreshMain_() {
    const AppController& app = *app_;
    char buf[48];

    for (uint8_t ch = 1; ch <= 2; ++ch) {
        const uint8_t i = ch - 1;
        const Channel& c = app.channel(ch);
        setCardFrequency(w_.cardFreq[i], app, ch, c.frequency);
        switch (app.channelStatus(ch)) {
            case ChannelStatus::Off: setState(w_.cardState[i], "OFF", Colors::muted()); break;
            case ChannelStatus::On: setState(w_.cardState[i], "ON", Colors::ok()); break;
            case ChannelStatus::Error: setState(w_.cardState[i], "ERR", Colors::error()); break;
            case ChannelStatus::NoDevice: setState(w_.cardState[i], "NO HW", Colors::error()); break;
        }
        const bool iqMaster = app.ch3().master == ch && app.ch3Status() != Ch3Status::Off;
        lv_label_set_text(w_.cardDetail[i], iqMaster ? "IQ master of CH3" : "");
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
    const char* reason = "";
    switch (app.ch3Status()) {
        case Ch3Status::Off: setState(w_.cardState[2], "OFF", Colors::muted()); break;
        case Ch3Status::Active: setState(w_.cardState[2], "ON", Colors::ok()); break;
        case Ch3Status::MasterOff: setState(w_.cardState[2], "WAIT", Colors::warn()); reason = " / master off"; break;
        case Ch3Status::FreqTooLow: setState(w_.cardState[2], "WAIT", Colors::warn()); reason = tooLow; break;
        case Ch3Status::FreqTooHigh: setState(w_.cardState[2], "WAIT", Colors::warn()); reason = tooHigh; break;
        case Ch3Status::Error: setState(w_.cardState[2], "ERR", Colors::error()); reason = " / setup failed"; break;
        case Ch3Status::NoDevice: setState(w_.cardState[2], "NO HW", Colors::error()); break;
    }
    snprintf(buf, sizeof(buf), "CH%u / %u deg%s", c3.master, c3.phase, reason);
    lv_label_set_text(w_.cardDetail[2], buf);
    if (c3.enabled) lv_obj_add_state(w_.cardSwitch[2], LV_STATE_CHECKED);
    else lv_obj_clear_state(w_.cardSwitch[2], LV_STATE_CHECKED);
}
