#include "../gui.h"
#include <utils/frequency_utils.h>

void Gui::createConfig_() {
    const lv_color_t color = lv_color_hex(0xFDD835);
    createHeader_("CONFIG", color, true, false);
    char buf[64];

    lv_obj_t* card1 = addCard_(root_, 8, 48, 304, 48, color);
    snprintf(buf, sizeof(buf), "Backlight: %u%%", app_->system().backlight);
    lv_obj_t* lb = lv_label_create(card1); lv_label_set_text(lb, buf); lv_obj_set_pos(lb, 4, 0);
    lv_obj_t* s = lv_slider_create(card1); lv_slider_set_range(s, 0, 100); lv_slider_set_value(s, app_->system().backlight, LV_ANIM_OFF); lv_obj_set_pos(s, 128, 5); lv_obj_set_size(s, 154, 14); lv_obj_set_user_data(s, (void*)"bl"); lv_obj_add_event_cb(s, sliderEvent_, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* card2 = addCard_(root_, 8, 104, 304, 44, color);
    snprintf(buf, sizeof(buf), "Delta: %lu Hz", (unsigned long)app_->system().delta_step); lv_obj_t* ld = lv_label_create(card2); lv_label_set_text(ld, buf); lv_obj_set_pos(ld, 4, 2);
    addButton_(card2, "x10", 160, 0, 54, 28, "delta_x10", lv_color_hex(0xF9A825));
    addButton_(card2, "/10", 222, 0, 54, 28, "delta_d10", lv_color_hex(0xF9A825));

    lv_obj_t* card3 = addCard_(root_, 8, 156, 304, 44, color);
    snprintf(buf, sizeof(buf), "Correction: %ld", (long)app_->system().freq_correction); lv_obj_t* lc = lv_label_create(card3); lv_label_set_text(lc, buf); lv_obj_set_pos(lc, 4, 2);
    addButton_(card3, "+10", 160, 0, 54, 28, "corr_p", lv_color_hex(0xF9A825));
    addButton_(card3, "-10", 222, 0, 54, 28, "corr_m", lv_color_hex(0xF9A825));

    lv_obj_t* driveLabel = lv_label_create(root_); lv_label_set_text(driveLabel, "Drive all:"); lv_obj_set_pos(driveLabel, 12, 214);
    lv_obj_t* dd = lv_dropdown_create(root_); lv_dropdown_set_options(dd, "2 mA\n4 mA\n6 mA\n8 mA"); lv_dropdown_set_selected(dd, app_->system().default_drive_strength / 2 - 1); lv_obj_set_pos(dd, 82, 206); lv_obj_set_size(dd, 90, 30); lv_obj_set_user_data(dd, (void*)"def_drive"); lv_obj_add_event_cb(dd, dropdownEvent_, LV_EVENT_VALUE_CHANGED, nullptr);
    status_ = lv_label_create(root_); lv_label_set_text(status_, "Autosave 5s"); lv_obj_set_pos(status_, 212, 214);
}

