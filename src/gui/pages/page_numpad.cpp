#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <utils/input_logic.h>

static const char* btnmMap[] = {
    "7", "8", "9", "MHz", "\n",
    "4", "5", "6", "kHz", "\n",
    "1", "2", "3", "Hz", "\n",
    ".", "0", LV_SYMBOL_BACKSPACE, "C", ""};
static const uint16_t UNIT_BTN_IDS[] = {3, 7, 11};

void Gui::createNumpad_(uint8_t ch) {
    const lv_color_t color = Colors::forChannel(ch);
    char buf[32];

    // Back returns to the channel page without changing the frequency.
    addButton_(root_, LV_SYMBOL_LEFT, 4, 3, 52, 36, Colors::gray(), Action::Navigate,
               (uint8_t)(ch == 1 ? PageId::CH1 : PageId::CH2));
    addLabel_(root_, ch == 1 ? "CH1" : "CH2", &lv_font_montserrat_20, color, 64, 10);

    numpad_ = InputLogic::Numpad {};
    w_.ta = lv_textarea_create(root_);
    lv_obj_set_pos(w_.ta, 112, 2);
    lv_obj_set_size(w_.ta, SCREEN_WIDTH - 116, 40);
    lv_textarea_set_one_line(w_.ta, true);
    lv_textarea_set_max_length(w_.ta, InputLogic::NUMPAD_MAX_LEN);
    lv_textarea_set_accepted_chars(w_.ta, "0123456789.");
    lv_obj_set_style_text_font(w_.ta, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(w_.ta, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_ver(w_.ta, 6, 0);
    lv_obj_set_style_border_color(w_.ta, color, 0);
    lv_obj_clear_flag(w_.ta, LV_OBJ_FLAG_CLICKABLE);
    // The current frequency stays visible while typing, the error text on the right (U-9).
    char freq[24];
    FrequencyUtils::format(app_->channel(ch).frequency, freq, sizeof(freq));
    snprintf(buf, sizeof(buf), "Now %s", freq);
    addLabel_(root_, buf, &lv_font_montserrat_14, Colors::muted(), 8, 45);
    w_.npError = addLabel_(root_, "", &lv_font_montserrat_14, Colors::error(), 0, 0);
    lv_obj_align(w_.npError, LV_ALIGN_TOP_RIGHT, -8, 45);

    lv_obj_t* m = lv_btnmatrix_create(root_);
    lv_btnmatrix_set_map(m, btnmMap);
    // Up to the screen edge; the padding keeps the keys where they were (6 px from the edge).
    // Keys in the outer rows and columns take taps in the padding, so up to the edge (U-5).
    lv_obj_set_pos(m, 0, 62);
    lv_obj_set_size(m, SCREEN_WIDTH, SCREEN_HEIGHT - 62);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m, 0, 0);
    lv_obj_set_style_pad_hor(m, 6, 0);
    lv_obj_set_style_pad_top(m, 2, 0);
    lv_obj_set_style_pad_bottom(m, 6, 0);
    lv_obj_set_style_pad_gap(m, 4, 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_20, LV_PART_ITEMS);
    // Trigger on release: a unit key navigates away, and a press-trigger would let the
    // still-touching finger hit a widget of the next page.
    lv_btnmatrix_set_btn_ctrl_all(m, LV_BTNMATRIX_CTRL_CLICK_TRIG);
    // Unit keys confirm the input; highlight them in the channel color.
    lv_obj_set_style_bg_color(m, color, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(m, Colors::textOn(color), LV_PART_ITEMS | LV_STATE_CHECKED);
    for (uint16_t id : UNIT_BTN_IDS) lv_btnmatrix_set_btn_ctrl(m, id, LV_BTNMATRIX_CTRL_CHECKED);
    on_(m, LV_EVENT_VALUE_CHANGED, Action::NumpadKey, 0);
}

void Gui::handleNumpadKey_(lv_obj_t* matrix) {
    lv_obj_t* ta = w_.ta;
    const char* key = lv_btnmatrix_get_btn_text(matrix, lv_btnmatrix_get_selected_btn(matrix));
    if (!key || !ta) return;
    uint64_t hz = 0;
    switch (InputLogic::numpadKey(numpad_, key, &hz)) {
        case InputLogic::NumpadEvent::Confirmed:
            app_->setChannelFrequency(channel_, hz);
            requestNavigate_(channel_ == 1 ? PageId::CH1 : PageId::CH2);
            return;
        case InputLogic::NumpadEvent::Rejected:
            lv_label_set_text(w_.npError, InputLogic::numpadErrorText(numpad_.error));
            lv_obj_set_style_border_color(ta, Colors::error(), 0);
            return;
        case InputLogic::NumpadEvent::Edited:
            lv_textarea_set_text(ta, numpad_.text);
            break;
        case InputLogic::NumpadEvent::Ignored:
            break;
    }
    lv_label_set_text(w_.npError, "");
    lv_obj_set_style_border_color(ta, Colors::forChannel(channel_), 0);
}
