#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <utils/input_logic.h>

// Frequency entry of CH1/CH2: the unit keys confirm.
// Top row: back key, title, input field; below the current value and error line, then the keys.
static constexpr int BACK_W = 52;
static constexpr int TITLE_X = 64;
static constexpr int FIELD_X = 112;
static constexpr int INFO_Y = 45;
static constexpr int KEYS_Y = 62;

static const char* freqMap[] = {
    "7", "8", "9", "MHz", "\n",
    "4", "5", "6", "kHz", "\n",
    "1", "2", "3", "Hz", "\n",
    ".", "0", LV_SYMBOL_BACKSPACE, "C", ""};
static const uint16_t FREQ_CONFIRM_IDS[] = {3, 7, 11};

// Correction entry from CONFIG (U-11): "+/-" toggles the sign, "ppm" confirms.
static const char* ppmMap[] = {
    "7", "8", "9", "+/-", "\n",
    "4", "5", "6", "ppm", "\n",
    "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
    ".", "0", "C", ""};
static const uint16_t PPM_CONFIRM_IDS[] = {7};

lv_color_t Gui::numpadColor_() const { return numpadPpm_ ? Colors::config() : Colors::forChannel(channel_); }

void Gui::createNumpad_(uint8_t ch) {
    const lv_color_t color = numpadColor_();
    char buf[32];

    // Back returns without a change: to the channel page, or to CONFIG for the correction.
    const PageId back = numpadPpm_ ? PageId::Config : numpadReturn_;
    addButton_(root_, LV_SYMBOL_LEFT, 4, 3, BACK_W, 36, Colors::gray(), Action::Navigate, (uint8_t)back);
    addLabel_(root_, numpadPpm_ ? "Corr." : (ch == 1 ? "CH1" : "CH2"), &lv_font_montserrat_20, color, TITLE_X, 10);

    numpad_ = InputLogic::Numpad {};
    w_.ta = lv_textarea_create(root_);
    lv_obj_set_pos(w_.ta, FIELD_X, 2);
    lv_obj_set_size(w_.ta, SCREEN_WIDTH - FIELD_X - 4, 40);
    lv_textarea_set_one_line(w_.ta, true);
    lv_textarea_set_max_length(w_.ta, InputLogic::NUMPAD_MAX_LEN + 1); // + sign
    lv_textarea_set_accepted_chars(w_.ta, "-0123456789.");
    lv_obj_set_style_text_font(w_.ta, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(w_.ta, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_ver(w_.ta, 6, 0);
    lv_obj_set_style_border_color(w_.ta, color, 0);
    lv_obj_clear_flag(w_.ta, LV_OBJ_FLAG_CLICKABLE);
    // The current value stays visible while typing, the error text on the right (U-9).
    char now[24];
    if (numpadPpm_) FrequencyUtils::formatPpm(app_->system().freq_correction, now, sizeof(now));
    else FrequencyUtils::format(app_->channel(ch).frequency, now, sizeof(now));
    snprintf(buf, sizeof(buf), "Now %s", now);
    addLabel_(root_, buf, &lv_font_montserrat_14, Colors::muted(), 8, INFO_Y);
    w_.npError = addLabel_(root_, "", &lv_font_montserrat_14, Colors::error(), 0, 0);
    lv_obj_align(w_.npError, LV_ALIGN_TOP_RIGHT, -8, INFO_Y);

    lv_obj_t* m = lv_btnmatrix_create(root_);
    lv_btnmatrix_set_map(m, numpadPpm_ ? ppmMap : freqMap);
    // Up to the screen edge; the padding keeps the keys where they were (6 px from the edge).
    // Keys in the outer rows and columns take taps in the padding, so up to the edge (U-5).
    lv_obj_set_pos(m, 0, KEYS_Y);
    lv_obj_set_size(m, SCREEN_WIDTH, SCREEN_HEIGHT - KEYS_Y);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m, 0, 0);
    lv_obj_set_style_pad_hor(m, 6, 0);
    lv_obj_set_style_pad_top(m, 2, 0);
    lv_obj_set_style_pad_bottom(m, 6, 0);
    lv_obj_set_style_pad_gap(m, 4, 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_20, LV_PART_ITEMS);
    // Trigger on release: a confirm key navigates away, and a press-trigger would let the
    // still-touching finger hit a widget of the next page.
    lv_btnmatrix_set_btn_ctrl_all(m, LV_BTNMATRIX_CTRL_CLICK_TRIG);
    // Confirm keys in the page color.
    lv_obj_set_style_bg_color(m, color, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(m, Colors::textOn(color), LV_PART_ITEMS | LV_STATE_CHECKED);
    if (numpadPpm_) {
        for (uint16_t id : PPM_CONFIRM_IDS) lv_btnmatrix_set_btn_ctrl(m, id, LV_BTNMATRIX_CTRL_CHECKED);
    } else {
        for (uint16_t id : FREQ_CONFIRM_IDS) lv_btnmatrix_set_btn_ctrl(m, id, LV_BTNMATRIX_CTRL_CHECKED);
    }
    on_(m, LV_EVENT_VALUE_CHANGED, Action::NumpadKey, 0);
}

void Gui::handleNumpadKey_(lv_obj_t* matrix) {
    lv_obj_t* ta = w_.ta;
    const char* key = lv_btnmatrix_get_btn_text(matrix, lv_btnmatrix_get_selected_btn(matrix));
    if (!key || !ta) return;
    uint64_t hz = 0;
    int32_t centiPpm = 0;
    const InputLogic::NumpadEvent e = numpadPpm_ ? InputLogic::numpadPpmKey(numpad_, key, &centiPpm)
                                                 : InputLogic::numpadKey(numpad_, key, &hz);
    switch (e) {
        case InputLogic::NumpadEvent::Confirmed:
            if (numpadPpm_) {
                app_->setCorrection(centiPpm);
                requestNavigate_(PageId::Config);
            } else {
                app_->setChannelFrequency(channel_, hz);
                requestNavigate_(numpadReturn_);
            }
            return;
        case InputLogic::NumpadEvent::Rejected:
            lv_label_set_text(w_.npError, InputLogic::numpadErrorText(numpad_.error));
            lv_obj_set_style_border_color(ta, Colors::error(), 0);
            return;
        case InputLogic::NumpadEvent::Edited: {
            char shown[InputLogic::NUMPAD_MAX_LEN + 2];
            snprintf(shown, sizeof(shown), "%s%s", numpad_.negative ? "-" : "", numpad_.text);
            lv_textarea_set_text(ta, shown);
            break;
        }
        case InputLogic::NumpadEvent::Ignored:
            break;
    }
    lv_label_set_text(w_.npError, "");
    lv_obj_set_style_border_color(ta, numpadColor_(), 0);
}
