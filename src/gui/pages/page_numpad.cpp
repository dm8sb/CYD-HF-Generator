#include "../gui.h"
#include <utils/frequency_utils.h>

static const char* btnmMap[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", ".", "0", "<--", "\n", "Clear", "Hz", "kHz", "MHz", ""};

void Gui::createNumpad_(uint8_t ch) {
    selectedChannel_ = ch;
    const lv_color_t color = (ch == 1) ? lv_color_hex(0xE53935) : lv_color_hex(0x43A047);
    char t[32]; snprintf(t, sizeof(t), "SET CH%u", ch);
    createHeader_(t, color, true, false);

    lv_obj_t* ta = lv_textarea_create(root_);
    lv_obj_set_pos(ta, 10, 48);
    lv_obj_set_size(ta, 300, 36);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 9);
    lv_obj_set_user_data(ta, (void*)"freq_ta");

    lv_obj_t* m = lv_btnmatrix_create(root_);
    lv_btnmatrix_set_map(m, btnmMap);
    lv_obj_set_pos(m, 8, 92);
    lv_obj_set_size(m, 304, 140);
    lv_obj_set_style_bg_color(m, lv_color_hex(0x151A22), 0);
    lv_obj_set_style_border_color(m, color, 0);
    lv_obj_set_style_border_width(m, 2, 0);
    lv_obj_add_event_cb(m, btnEvent_, LV_EVENT_VALUE_CHANGED, ta);
}

