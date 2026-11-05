#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>

using namespace Layout;

static const char* PHASE_LABELS[4] = {"0", "90", "180", "270"};

void Gui::createCH3_() {
    const lv_color_t color = Colors::ch3();
    createHeader_("CH3 IQ", color, 3);

    lv_obj_t* card = addCard_(root_, MARGIN, CONTENT_Y, CONTENT_W, 80, color);
    w_.freq = addLabel_(card, "", &lv_font_montserrat_28, Colors::text(), 2, 0);
    // The frequency is the master's: a tap opens its numpad and returns here (U-16).
    lv_obj_add_flag(w_.freq, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(w_.freq, 4);
    on_(w_.freq, LV_EVENT_CLICKED, Action::Ch3MasterFreq, 0);
    w_.info = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), 2, 32);
    // While CH3 waits for its master, a tap on this line switches the master on (U-16).
    lv_obj_add_flag(w_.info, LV_OBJ_FLAG_CLICKABLE);
    on_(w_.info, LV_EVENT_CLICKED, Action::Ch3InfoTap, 0);
    w_.modes = addLabel_(card, "", &lv_font_montserrat_12, Colors::muted(), 2, 50);
    lv_obj_set_width(w_.modes, CONTENT_W - 2 * (CARD_PAD + CARD_BORDER) - 4);
    lv_label_set_long_mode(w_.modes, LV_LABEL_LONG_DOT);

    addLabel_(root_, "Master", &lv_font_montserrat_14, Colors::muted(), MARGIN + 2, 128);
    addLabel_(root_, "Phase (\xC2\xB0)", &lv_font_montserrat_14, Colors::muted(), 148, 128);

    // Chips 6 px apart, a mistap would swap the sideband (audit U-6); 8 px between the groups.
    w_.masterChips[0] = addChip_(root_, "CH1", MARGIN, 146, 62, 40, Colors::ch1(), Action::Ch3Master, 1);
    w_.masterChips[1] = addChip_(root_, "CH2", MARGIN + 68, 146, 62, 40, Colors::ch2(), Action::Ch3Master, 2);
    for (uint8_t i = 0; i < 4; ++i) {
        w_.chips[i] = addChip_(root_, PHASE_LABELS[i], 146 + i * 43, 146, 37, 40, color, Action::Ch3Phase, i);
    }

    char lo[12], hi[12], text[128];
    // Rounded to kHz towards the inside of the range; 4.730 / 100 MHz without spread spectrum.
    FrequencyUtils::formatIqLimit(app_->iqMinHz(), true, 3, false, lo, sizeof(lo));
    FrequencyUtils::formatIqLimit(app_->iqMaxHz(), false, 3, true, hi, sizeof(hi));
    snprintf(text, sizeof(text), "IQ range %s - %s MHz. 180\xC2\xB0/270\xC2\xB0 use output inversion. "
                                 "Drive strength is set globally in CONFIG.", lo, hi);
    lv_obj_t* note = addLabel_(root_, text, &lv_font_montserrat_12, Colors::muted(), MARGIN + 2, 194);
    lv_obj_set_width(note, CONTENT_W - 4);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
}

void Gui::refreshCH3_() {
    const AppController& app = *app_;
    const Channel3& c3 = app.ch3();
    char buf[64];

    const Ch3Status status = app.ch3Status();
    setHeaderState_(c3.enabled, StatusText::ch3(status));
    FrequencyUtils::format(app.ch3Frequency(), buf, sizeof(buf));
    lv_label_set_text(w_.freq, buf);

    lv_color_t infoColor = Colors::muted();
    char iqMin[12], iqMax[12];
    FrequencyUtils::formatIqLimit(app.iqMinHz(), true, 3, false, iqMin, sizeof(iqMin));
    FrequencyUtils::formatIqLimit(app.iqMaxHz(), false, 3, false, iqMax, sizeof(iqMax));
    switch (status) {
        case Ch3Status::Off:
            snprintf(buf, sizeof(buf), "Off  |  follows CH%u", c3.master);
            break;
        case Ch3Status::Active:
            // Actual frequency of CLK2 instead of a status text; the header shows ON.
            showOutput_(3, app.ch3Frequency());
            break;
        case Ch3Status::MasterOff:
            snprintf(buf, sizeof(buf), "Master CH%u is off: tap to switch on", c3.master);
            infoColor = Colors::warn();
            break;
        case Ch3Status::FreqTooLow:
            snprintf(buf, sizeof(buf), "Waiting: CH%u below %s MHz", c3.master, iqMin);
            infoColor = Colors::warn();
            break;
        case Ch3Status::FreqTooHigh:
            snprintf(buf, sizeof(buf), "Waiting: CH%u above %s MHz", c3.master, iqMax);
            infoColor = Colors::warn();
            break;
        case Ch3Status::Error:
            snprintf(buf, sizeof(buf), "IQ setup failed, retrying");
            infoColor = Colors::error();
            break;
        case Ch3Status::NoDevice:
            snprintf(buf, sizeof(buf), "SI5351 not found");
            infoColor = Colors::error();
            break;
    }
    if (status != Ch3Status::Active) {
        lv_label_set_text(w_.info, buf);
        lv_obj_set_style_text_color(w_.info, infoColor, 0);
    }

    showModes_(w_.modes, 3);

    for (uint8_t i = 0; i < 2; ++i) {
        if (c3.master == i + 1) lv_obj_add_state(w_.masterChips[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.masterChips[i], LV_STATE_CHECKED);
    }
    for (uint8_t i = 0; i < 4; ++i) {
        if (c3.phase == i * 90) lv_obj_add_state(w_.chips[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.chips[i], LV_STATE_CHECKED);
    }
}
