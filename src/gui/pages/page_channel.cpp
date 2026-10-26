#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <math.h>

using namespace Layout;

static const char* STEP_LABELS[DELTA_STEP_COUNT] = {"1", "10", "100", "1k", "10k", "100k", "1M"};

// CH1 and CH2 share this page.
void Gui::createChannelPage_(uint8_t ch) {
    const lv_color_t color = Colors::forChannel(ch);
    createHeader_(ch == 1 ? "CH1" : "CH2", color, ch);

    lv_obj_t* card = addCard_(root_, MARGIN, CONTENT_Y, CONTENT_W, 66, color);
    w_.freq = addLabel_(card, "", &lv_font_montserrat_28, Colors::text(), 2, 0);
    w_.info = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), 2, 32);

    // Step presets in Hz.
    const int chipW = (CONTENT_W - (DELTA_STEP_COUNT - 1) * 4) / DELTA_STEP_COUNT;
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) {
        w_.chips[i] = addChip_(root_, STEP_LABELS[i], MARGIN + i * (chipW + 4), 116, chipW, 36, color, Action::SetStep, i);
    }

    const int btnY = 160;
    const int btnH = 72;
    addButton_(root_, LV_SYMBOL_MINUS, MARGIN, btnY, 96, btnH, color, Action::FreqDown, 0, true);
    addButton_(root_, LV_SYMBOL_PLUS, MARGIN + 102, btnY, 96, btnH, color, Action::FreqUp, 0, true);
    addButton_(root_, "NUMPAD", MARGIN + 204, btnY, CONTENT_W - 204, btnH, Colors::gray(), Action::Navigate, (uint8_t)PageId::Numpad);
}

void Gui::refreshChannel_() {
    const AppController& app = *app_;
    const Channel& c = app.channel(channel_);
    char buf[64];
    char step[16];

    setHeaderState_(c.enabled);
    FrequencyUtils::format(c.frequency, buf, sizeof(buf));
    lv_label_set_text(w_.freq, buf);

    FrequencyUtils::formatStep(app.system().delta_step, step, sizeof(step));
    if (!app.hardwareReady()) {
        snprintf(buf, sizeof(buf), "Step %s  |  SI5351 not found", step);
        lv_obj_set_style_text_color(w_.info, Colors::error(), 0);
    } else {
        // Deviation of the synthesized frequency from the requested one (SI5351 resolution).
        double dev = app.actualHz(channel_) - (double)c.frequency;
        if (fabs(dev) < 0.05) dev = 0.0;
        snprintf(buf, sizeof(buf), "Step %s  |  Deviation %+.1f Hz", step, dev);
        lv_obj_set_style_text_color(w_.info, Colors::muted(), 0);
    }
    lv_label_set_text(w_.info, buf);

    const uint8_t sel = FrequencyUtils::stepIndex(app.system().delta_step);
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) {
        if (i == sel) lv_obj_add_state(w_.chips[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.chips[i], LV_STATE_CHECKED);
    }
}
