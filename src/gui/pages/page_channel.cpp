#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <utils/input_logic.h>

using namespace Layout;

// Step chips and the -/+/NUMPAD row below the card.
static constexpr int CHIP_Y = 128;
static constexpr int CHIP_H = 36;
static constexpr int BTN_W = 96;

static const char* STEP_LABELS[DELTA_STEP_COUNT] = {"1", "10", "100", "1k", "10k", "100k", "1M"};

// CH1 and CH2 share this page.
void Gui::createChannelPage_(uint8_t ch) {
    const lv_color_t color = Colors::forChannel(ch);
    createHeader_(ch == 1 ? "CH1" : "CH2", color, ch);

    // Frequency, actual output (Out) and special modes (U-2).
    lv_obj_t* card = addCard_(root_, MARGIN, CONTENT_Y, CONTENT_W, CARD_FREQ_H, color);
    w_.freq = addLabel_(card, "", &lv_font_montserrat_28, Colors::text(), CARD_TEXT_X, 0);
    // Tap a digit to select it as step, tap the unit to open the numpad.
    lv_label_set_recolor(w_.freq, true);
    lv_obj_add_flag(w_.freq, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(w_.freq, 4);
    on_(w_.freq, LV_EVENT_CLICKED, Action::FreqTap, 0);
    w_.info = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), CARD_TEXT_X, CARD_INFO_Y);
    w_.modes = addLabel_(card, "", &lv_font_montserrat_12, Colors::muted(), CARD_TEXT_X, CARD_MODES_Y);
    lv_obj_set_width(w_.modes, CONTENT_W - 2 * (CARD_PAD + CARD_BORDER) - 4);
    lv_label_set_long_mode(w_.modes, LV_LABEL_LONG_DOT);

    // Step presets in Hz.
    const int chipW = (CONTENT_W - (DELTA_STEP_COUNT - 1) * 4) / DELTA_STEP_COUNT;
    // Largest step on the left, as the digits of the frequency above (U-25).
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) {
        const int x = MARGIN + (DELTA_STEP_COUNT - 1 - i) * (chipW + 4);
        w_.chips[i] = addChip_(root_, STEP_LABELS[i], x, CHIP_Y, chipW, CHIP_H, color, Action::SetStep, i);
    }

    const int btnY = 170;
    const int btnH = 62;
    addButton_(root_, LV_SYMBOL_MINUS, MARGIN, btnY, BTN_W, btnH, color, Action::FreqDown, 0, true);
    addButton_(root_, LV_SYMBOL_PLUS, MARGIN + BTN_W + GAP, btnY, BTN_W, btnH, color, Action::FreqUp, 0, true);
    addButton_(root_, "NUMPAD", MARGIN + 2 * (BTN_W + GAP), btnY, CONTENT_W - 2 * (BTN_W + GAP), btnH, Colors::gray(), Action::Navigate, (uint8_t)PageId::Numpad);
}

void Gui::refreshChannel_() {
    const AppController& app = *app_;
    const Channel& c = app.channel(channel_);
    char buf[64];

    const ChannelStatus status = app.channelStatus(channel_);
    setHeaderState_(c.enabled, StatusText::channel(status));
    // The digit that +/- changes is shown in the channel color.
    FrequencyUtils::format(c.frequency, freqText_, sizeof(freqText_));
    InputLogic::stepMarkup(freqText_, app.system().delta_step, lv_color_to32(Colors::forChannel(channel_)), buf,
                           sizeof(buf));
    lv_label_set_text(w_.freq, buf);

    // The step is visible through the colored digit and the chips; the info line shows the
    // actual output frequency.
    if (status == ChannelStatus::NoDevice || status == ChannelStatus::Error) {
        lv_label_set_text(w_.info, status == ChannelStatus::NoDevice ? "SI5351 not found" : "Output error, retrying");
        lv_obj_set_style_text_color(w_.info, Colors::error(), 0);
    } else {
        // Switched off, the line shows what the output will deliver (U-17).
        showOutput_(channel_, c.frequency, status == ChannelStatus::Off ? "Off | " : "");
    }

    showModes_(w_.modes, channel_);
    // CH3 follows this channel: frequency changes move both (U-17).
    if (app.ch3().master == channel_ && app.ch3Status() != Ch3Status::Off) {
        const char* modes = lv_label_get_text(w_.modes);
        char line[96];
        snprintf(line, sizeof(line), "%s%sIQ master of CH3 (%u\xC2\xB0)", modes, modes[0] ? " | " : "",
                 (unsigned)app.ch3().phase);
        lv_label_set_text(w_.modes, line);
    }
    // Outside 1-100 MHz: level and harmonics hint (E-16).
    const char* note = StatusText::rangeNote(c.frequency);
    if (note[0]) {
        const char* modes = lv_label_get_text(w_.modes);
        char line[128];
        snprintf(line, sizeof(line), "%s%s%s", modes, modes[0] ? " | " : "", note);
        lv_label_set_text(w_.modes, line);
    }

    const uint8_t sel = FrequencyUtils::stepIndex(app.system().delta_step);
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) {
        if (i == sel) lv_obj_add_state(w_.chips[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.chips[i], LV_STATE_CHECKED);
    }
}

static int glyphWidth(char c, char next, void* font) {
    return lv_font_get_glyph_width(static_cast<const lv_font_t*>(font), (uint8_t)c, (uint8_t)next);
}

void Gui::handleFreqTap_(lv_obj_t* label) {
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev || !freqText_[0]) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t area;
    lv_obj_get_content_coords(label, &area);

    // Find the tapped character in the plain text; the recolor commands take no space.
    lv_font_t* font = const_cast<lv_font_t*>(lv_obj_get_style_text_font(label, LV_PART_MAIN));
    const size_t index = InputLogic::tapIndexAt(freqText_, p.x - area.x1, glyphWidth, font);
    uint32_t step = 0;
    switch (InputLogic::tapTarget(freqText_, index, &step)) {
        case InputLogic::TapTarget::Numpad:
            requestNavigate_(PageId::Numpad);
            return;
        case InputLogic::TapTarget::Step:
            app_->setDeltaStep(step);
            refresh_();
            return;
        case InputLogic::TapTarget::None:
            return;
    }
}
