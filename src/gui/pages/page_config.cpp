#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>

using namespace Layout;

static constexpr int ROW_H = 44;
static constexpr int ROW_PITCH = ROW_H + 4;
static constexpr int ROW_CONTENT_H = ROW_H - 2 * 4 - 2; // pad 4, border 1

// Settings rows are slimmer cards than the default ones.
static lv_obj_t* asRow(lv_obj_t* card) {
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    return card;
}

void Gui::createConfig_() {
    const lv_color_t color = Colors::config();
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    createHeader_("CONFIG", color, 0);
    lv_obj_t* row;
    lv_obj_t* l;

    // Backlight
    row = asRow(addCard_(root_, MARGIN, CONTENT_Y, CONTENT_W, ROW_H, color));
    l = addLabel_(row, "Backlight", &lv_font_montserrat_14, Colors::text(), 2, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_t* s = lv_slider_create(row);
    lv_slider_set_range(s, BACKLIGHT_MIN_PERCENT, 100);
    lv_slider_set_value(s, app_->system().backlight, LV_ANIM_OFF);
    lv_obj_set_size(s, 140, 10);
    lv_obj_align(s, LV_ALIGN_LEFT_MID, 92, 0);
    lv_obj_set_style_bg_color(s, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, color, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s, 12);
    on_(s, LV_EVENT_VALUE_CHANGED, Action::Backlight, 0);
    w_.blValue = addLabel_(row, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
    lv_obj_align(w_.blValue, LV_ALIGN_RIGHT_MID, -2, 0);

    // Frequency step (same presets as the chips on the channel pages)
    row = asRow(addCard_(root_, MARGIN, CONTENT_Y + ROW_PITCH, CONTENT_W, ROW_H, color));
    l = addLabel_(row, "Step", &lv_font_montserrat_14, Colors::text(), 0, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 2, 0);
    w_.stepValue = addLabel_(row, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
    lv_obj_align(w_.stepValue, LV_ALIGN_LEFT_MID, 92, 0);
    addButton_(row, "/10", 180, 0, 52, ROW_CONTENT_H, btnColor, Action::StepDown);
    addButton_(row, "x10", 236, 0, 52, ROW_CONTENT_H, btnColor, Action::StepUp);

    // Frequency correction in ppm; the step chip cycles 0.01 / 0.1 / 1 ppm.
    row = asRow(addCard_(root_, MARGIN, CONTENT_Y + 2 * ROW_PITCH, CONTENT_W, ROW_H, color));
    l = addLabel_(row, "Corr.", &lv_font_montserrat_14, Colors::text(), 0, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 2, 0);
    w_.corrValue = addLabel_(row, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
    lv_obj_align(w_.corrValue, LV_ALIGN_LEFT_MID, 46, 0);
    w_.corrStep = addChip_(row, "", 140, 0, 46, ROW_CONTENT_H, btnColor, Action::CorrStepCycle, 0);
    addButton_(row, LV_SYMBOL_MINUS, 190, 0, 47, ROW_CONTENT_H, btnColor, Action::CorrDown, 0, true);
    addButton_(row, LV_SYMBOL_PLUS, 241, 0, 47, ROW_CONTENT_H, btnColor, Action::CorrUp, 0, true);

    // Global drive strength and autosave state
    row = asRow(addCard_(root_, MARGIN, CONTENT_Y + 3 * ROW_PITCH, CONTENT_W, ROW_H, color));
    l = addLabel_(row, "Drive all", &lv_font_montserrat_14, Colors::text(), 0, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_t* dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, "2 mA\n4 mA\n6 mA\n8 mA");
    lv_dropdown_set_selected(dd, app_->system().default_drive_strength / 2 - 1);
    lv_obj_set_size(dd, 96, ROW_CONTENT_H);
    lv_obj_align(dd, LV_ALIGN_LEFT_MID, 92, 0);
    on_(dd, LV_EVENT_VALUE_CHANGED, Action::Drive, 0);
    w_.saveState = addLabel_(row, "", &lv_font_montserrat_14, Colors::muted(), 0, 0);
    lv_obj_align(w_.saveState, LV_ALIGN_RIGHT_MID, -2, 0);
}

void Gui::refreshConfig_() {
    const SystemSettings& sys = app_->system();
    char buf[24];

    snprintf(buf, sizeof(buf), "%u %%", sys.backlight);
    lv_label_set_text(w_.blValue, buf);

    FrequencyUtils::formatStep(sys.delta_step, buf, sizeof(buf));
    lv_label_set_text(w_.stepValue, buf);

    FrequencyUtils::formatPpm(sys.freq_correction, buf, sizeof(buf));
    lv_label_set_text(w_.corrValue, buf);
    static const char* corrStepLabels[CORRECTION_STEP_COUNT] = {"0.01", "0.1", "1"};
    lv_label_set_text(lv_obj_get_child(w_.corrStep, 0), corrStepLabels[corrStepIdx_]);

    refreshSaveState_();
}

void Gui::refreshSaveState_() {
    if (!w_.saveState) return;
    const char* text = "Saved";
    lv_color_t color = Colors::muted();
    switch (app_->saveState()) {
        case SaveState::Saved: break;
        case SaveState::Pending: text = "Saving..."; break;
        case SaveState::Failed: text = "Save failed"; color = Colors::error(); break;
        case SaveState::Unavailable: text = "Not saved"; color = Colors::error(); break;
    }
    lv_label_set_text(w_.saveState, text);
    lv_obj_set_style_text_color(w_.saveState, color, 0);
}
