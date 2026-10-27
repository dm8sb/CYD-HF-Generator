#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <cstring>

static const char* btnmMap[] = {
    "7", "8", "9", "MHz", "\n",
    "4", "5", "6", "kHz", "\n",
    "1", "2", "3", "Hz", "\n",
    ".", "0", LV_SYMBOL_BACKSPACE, "C", ""};
static const uint16_t UNIT_BTN_IDS[] = {3, 7, 11};

static void showInputError(lv_obj_t* ta) {
    lv_textarea_set_text(ta, "");
    lv_textarea_set_placeholder_text(ta, "1 - 100 MHz");
    lv_obj_set_style_border_color(ta, Colors::error(), 0);
}

void Gui::createNumpad_(uint8_t ch) {
    const lv_color_t color = Colors::forChannel(ch);
    char buf[32];

    // Back returns to the channel page without changing the frequency.
    addButton_(root_, LV_SYMBOL_LEFT, 4, 3, 52, 36, Colors::gray(), Action::Navigate,
               (uint8_t)(ch == 1 ? PageId::CH1 : PageId::CH2));
    addLabel_(root_, ch == 1 ? "CH1" : "CH2", &lv_font_montserrat_20, color, 64, 10);

    w_.ta = lv_textarea_create(root_);
    lv_obj_set_pos(w_.ta, 112, 2);
    lv_obj_set_size(w_.ta, SCREEN_WIDTH - 116, 40);
    lv_textarea_set_one_line(w_.ta, true);
    lv_textarea_set_max_length(w_.ta, 9);
    lv_textarea_set_accepted_chars(w_.ta, "0123456789.");
    lv_obj_set_style_text_font(w_.ta, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(w_.ta, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_pad_ver(w_.ta, 6, 0);
    lv_obj_set_style_border_color(w_.ta, color, 0);
    lv_obj_clear_flag(w_.ta, LV_OBJ_FLAG_CLICKABLE);
    FrequencyUtils::format(app_->channel(ch).frequency, buf, sizeof(buf));
    lv_textarea_set_placeholder_text(w_.ta, buf);

    lv_obj_t* m = lv_btnmatrix_create(root_);
    lv_btnmatrix_set_map(m, btnmMap);
    lv_obj_set_pos(m, 4, 46);
    lv_obj_set_size(m, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 50);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m, 0, 0);
    lv_obj_set_style_pad_all(m, 2, 0);
    lv_obj_set_style_pad_gap(m, 4, 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_20, LV_PART_ITEMS);
    // Trigger on release: a unit key navigates away, and a press-trigger would let the
    // still-touching finger hit a widget of the next page.
    lv_btnmatrix_set_btn_ctrl_all(m, LV_BTNMATRIX_CTRL_CLICK_TRIG);
    // Unit keys confirm the input; highlight them in the channel color.
    lv_obj_set_style_bg_color(m, color, LV_PART_ITEMS | LV_STATE_CHECKED);
    for (uint16_t id : UNIT_BTN_IDS) lv_btnmatrix_set_btn_ctrl(m, id, LV_BTNMATRIX_CTRL_CHECKED);
    on_(m, LV_EVENT_VALUE_CHANGED, Action::NumpadKey, 0);
}

void Gui::handleNumpadKey_(lv_obj_t* matrix) {
    lv_obj_t* ta = w_.ta;
    const char* key = lv_btnmatrix_get_btn_text(matrix, lv_btnmatrix_get_selected_btn(matrix));
    if (!key || !ta) return;
    lv_obj_set_style_border_color(ta, Colors::forChannel(channel_), 0);

    if (!strcmp(key, LV_SYMBOL_BACKSPACE)) {
        lv_textarea_del_char(ta);
    } else if (!strcmp(key, "C")) {
        lv_textarea_set_text(ta, "");
    } else if (!strcmp(key, "Hz") || !strcmp(key, "kHz") || !strcmp(key, "MHz")) {
        uint64_t hz;
        if (!FrequencyUtils::parseFrequency(lv_textarea_get_text(ta), key, &hz)) {
            showInputError(ta);
            return;
        }
        app_->setChannelFrequency(channel_, hz);
        requestNavigate_(channel_ == 1 ? PageId::CH1 : PageId::CH2);
    } else if (!strcmp(key, ".") && strchr(lv_textarea_get_text(ta), '.')) {
        // Only one decimal point.
    } else {
        lv_textarea_add_text(ta, key);
    }
}
