#include "../gui.h"
#include <utils/frequency_utils.h>

void Gui::createMain_() {
    // Main page intentionally has no project-title header to maximize status space.
    char f1[32], f2[32], f3[32], line[96];
    FrequencyUtils::format(app_->ch1().frequency, f1, sizeof(f1));
    FrequencyUtils::format(app_->ch2().frequency, f2, sizeof(f2));
    FrequencyUtils::format(app_->ch3().master == 1 ? app_->ch1().frequency : app_->ch2().frequency, f3, sizeof(f3));

    lv_obj_t* c1 = addCard_(root_, 8, 8, 304, 40, lv_color_hex(0xE53935));
    snprintf(line, sizeof(line), "CH1   %s   %s", f1, app_->ch1().enabled ? "ON" : "OFF");
    lv_obj_t* l1 = lv_label_create(c1); lv_label_set_text(l1, line); lv_obj_set_pos(l1, 4, 0);

    lv_obj_t* c2 = addCard_(root_, 8, 56, 304, 40, lv_color_hex(0x43A047));
    snprintf(line, sizeof(line), "CH2   %s   %s", f2, app_->ch2().enabled ? "ON" : "OFF");
    lv_obj_t* l2 = lv_label_create(c2); lv_label_set_text(l2, line); lv_obj_set_pos(l2, 4, 0);

    lv_obj_t* c3 = addCard_(root_, 8, 104, 304, 50, lv_color_hex(0x1E88E5));
    snprintf(line, sizeof(line), "CH3   %s   M=CH%u  %u deg  %s", f3, app_->ch3().master, app_->ch3().phase,
             (app_->ch3().enabled && (app_->ch3().master == 1 ? app_->ch1().enabled : app_->ch2().enabled)) ? "ON" : "OFF");
    lv_obj_t* l3 = lv_label_create(c3); lv_label_set_text(l3, line); lv_obj_set_pos(l3, 4, 0);

    addButton_(root_, "CH1", 8, 188, 72, 40, "nav_ch1", lv_color_hex(0xE53935));
    addButton_(root_, "CH2", 86, 188, 72, 40, "nav_ch2", lv_color_hex(0x43A047));
    addButton_(root_, "CH3", 164, 188, 72, 40, "nav_ch3", lv_color_hex(0x1E88E5));
    addButton_(root_, "CONFIG", 242, 188, 72, 40, "nav_cfg", lv_color_hex(0xFDD835));
}

