#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>

using namespace Layout;

static const char* PHASE_LABELS[4] = {"0", "90", "180", "270"};

void Gui::createCH3_() {
    const lv_color_t color = Colors::ch3();
    createHeader_("CH3 IQ", color, 3);

    lv_obj_t* card = addCard_(root_, MARGIN, CONTENT_Y, CONTENT_W, 66, color);
    w_.freq = addLabel_(card, "", &lv_font_montserrat_28, Colors::text(), 2, 0);
    w_.info = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), 2, 32);

    addLabel_(root_, "Master", &lv_font_montserrat_14, Colors::muted(), MARGIN + 2, 114);
    addLabel_(root_, "Phase (deg)", &lv_font_montserrat_14, Colors::muted(), 152, 114);

    w_.masterChips[0] = addChip_(root_, "CH1", MARGIN, 134, 64, 40, Colors::ch1(), Action::Ch3Master, 1);
    w_.masterChips[1] = addChip_(root_, "CH2", MARGIN + 70, 134, 64, 40, Colors::ch2(), Action::Ch3Master, 2);
    for (uint8_t i = 0; i < 4; ++i) {
        w_.chips[i] = addChip_(root_, PHASE_LABELS[i], 150 + i * 41, 134, 39, 40, color, Action::Ch3Phase, i);
    }

    lv_obj_t* note = addLabel_(root_, "IQ range 4.730 - 100 MHz. 180/270 deg use output inversion. "
                                      "Drive strength is set globally in CONFIG.",
                               &lv_font_montserrat_12, Colors::muted(), MARGIN + 2, 186);
    lv_obj_set_width(note, CONTENT_W - 4);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
}

void Gui::refreshCH3_() {
    const AppController& app = *app_;
    const Channel3& c3 = app.ch3();
    char buf[64];

    const Ch3Status status = app.ch3Status();
    setHeaderState_(c3.enabled, status == Ch3Status::Error);
    FrequencyUtils::format(app.ch3Frequency(), buf, sizeof(buf));
    lv_label_set_text(w_.freq, buf);

    lv_color_t infoColor = Colors::muted();
    switch (status) {
        case Ch3Status::Off:
            snprintf(buf, sizeof(buf), "Off  |  follows CH%u", c3.master);
            break;
        case Ch3Status::Active:
            snprintf(buf, sizeof(buf), "IQ active, follows CH%u", c3.master);
            infoColor = Colors::ok();
            break;
        case Ch3Status::MasterOff:
            snprintf(buf, sizeof(buf), "Waiting: master CH%u is off", c3.master);
            infoColor = Colors::warn();
            break;
        case Ch3Status::FreqTooLow:
            snprintf(buf, sizeof(buf), "Waiting: CH%u below 4.730 MHz", c3.master);
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
    lv_label_set_text(w_.info, buf);
    lv_obj_set_style_text_color(w_.info, infoColor, 0);

    for (uint8_t i = 0; i < 2; ++i) {
        if (c3.master == i + 1) lv_obj_add_state(w_.masterChips[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.masterChips[i], LV_STATE_CHECKED);
    }
    for (uint8_t i = 0; i < 4; ++i) {
        if (c3.phase == i * 90) lv_obj_add_state(w_.chips[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.chips[i], LV_STATE_CHECKED);
    }
}
