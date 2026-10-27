#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <math.h>
#include <ctype.h>
#include <cstring>

using namespace Layout;

static const char* STEP_LABELS[DELTA_STEP_COUNT] = {"1", "10", "100", "1k", "10k", "100k", "1M"};

// CH1 and CH2 share this page.
void Gui::createChannelPage_(uint8_t ch) {
    const lv_color_t color = Colors::forChannel(ch);
    createHeader_(ch == 1 ? "CH1" : "CH2", color, ch);

    lv_obj_t* card = addCard_(root_, MARGIN, CONTENT_Y, CONTENT_W, 66, color);
    w_.freq = addLabel_(card, "", &lv_font_montserrat_28, Colors::text(), 2, 0);
    // Tap a digit to select it as step, tap the unit to open the numpad.
    lv_label_set_recolor(w_.freq, true);
    lv_obj_add_flag(w_.freq, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(w_.freq, 4);
    on_(w_.freq, LV_EVENT_CLICKED, Action::FreqTap, 0);
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
    // The digit that +/- changes is shown in the channel color.
    FrequencyUtils::format(c.frequency, freqText_, sizeof(freqText_));
    const int stepExp = FrequencyUtils::decimalExponent(app.system().delta_step);
    const uint32_t rgb = lv_color_to32(Colors::forChannel(channel_)) & 0xFFFFFF;
    size_t n = 0;
    for (size_t i = 0; freqText_[i] && n + 12 < sizeof(buf); ++i) {
        if (FrequencyUtils::digitExponent(freqText_, i) == stepExp) {
            n += snprintf(buf + n, sizeof(buf) - n, "#%06lX %c#", (unsigned long)rgb, freqText_[i]);
        } else {
            buf[n++] = freqText_[i];
        }
    }
    buf[n] = '\0';
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

void Gui::handleFreqTap_(lv_obj_t* label) {
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev || !freqText_[0]) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t area;
    lv_obj_get_content_coords(label, &area);

    // Find the tapped character in the plain text; the recolor commands take no space.
    const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const size_t len = strlen(freqText_);
    size_t index = len - 1; // right of the text = unit
    lv_coord_t x = area.x1;
    for (size_t i = 0; i < len; ++i) {
        x += lv_font_get_glyph_width(font, freqText_[i], freqText_[i + 1]);
        if (p.x < x) {
            index = i;
            break;
        }
    }

    if (isalpha((unsigned char)freqText_[index])) {
        requestNavigate_(PageId::Numpad);
        return;
    }
    const int exponent = FrequencyUtils::digitExponent(freqText_, index);
    if (exponent < 0) return; // decimal point or space
    // Digits above the largest step (10 / 100 MHz) select the largest step.
    app_->setDeltaStep(exponent < 10 ? FrequencyUtils::powerOfTen((uint8_t)exponent) : DELTA_STEPS[DELTA_STEP_COUNT - 1]);
    refresh_();
}
