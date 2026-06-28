#include "../gui.h"
#include <utils/frequency_utils.h>

void Gui::createChannelPage_(uint8_t ch) {
    const Channel& c = (ch == 1) ? app_->ch1() : app_->ch2();
    const lv_color_t color = (ch == 1) ? lv_color_hex(0xE53935) : lv_color_hex(0x43A047);
    char title[8]; snprintf(title, sizeof(title), "CH%u", ch);
    createHeader_(title, color, true, true, ch == 1 ? "ch1_en" : "ch2_en", c.enabled);

    char buf[48];
    lv_obj_t* card = addCard_(root_, 8, 58, 304, 62, color);
    FrequencyUtils::format(c.frequency, buf, sizeof(buf));
    lv_obj_t* freq = lv_label_create(card);
    lv_label_set_text(freq, buf);
    lv_obj_set_pos(freq, 8, 8);
    lv_obj_set_style_text_font(freq, &lv_font_montserrat_28, 0);

    snprintf(buf, sizeof(buf), "Step: %lu Hz", (unsigned long)app_->system().delta_step);
    lv_obj_t* step = lv_label_create(root_); lv_label_set_text(step, buf); lv_obj_set_pos(step, 16, 130);

    addButton_(root_, "+", 20, 162, 70, 48, ch == 1 ? "ch1_plus" : "ch2_plus", color);
    addButton_(root_, "-", 100, 162, 70, 48, ch == 1 ? "ch1_minus" : "ch2_minus", color);
    addButton_(root_, "NUMPAD", 188, 162, 104, 48, ch == 1 ? "num1" : "num2", color);
}

