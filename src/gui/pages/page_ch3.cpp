#include "../gui.h"
#include <utils/frequency_utils.h>

void Gui::createCH3_() {
    const lv_color_t color = lv_color_hex(0x1E88E5);
    createHeader_("CH3 IQ", color, true, true, "ch3_en", app_->ch3().enabled);

    char buf[48];
    lv_obj_t* card = addCard_(root_, 8, 58, 304, 54, color);
    FrequencyUtils::format(app_->ch3().master == 1 ? app_->ch1().frequency : app_->ch2().frequency, buf, sizeof(buf));
    lv_obj_t* freq = lv_label_create(card); lv_label_set_text(freq, buf); lv_obj_set_pos(freq, 8, 4); lv_obj_set_style_text_font(freq, &lv_font_montserrat_24, 0);
    lv_obj_t* info = lv_label_create(card); lv_label_set_text(info, "Follower frequency = master"); lv_obj_set_pos(info, 8, 32);

    lv_obj_t* lm = lv_label_create(root_); lv_label_set_text(lm, "Master"); lv_obj_set_pos(lm, 36, 128);
    lv_obj_t* lp = lv_label_create(root_); lv_label_set_text(lp, "Phase"); lv_obj_set_pos(lp, 190, 128);

    lv_obj_t* ddm = lv_dropdown_create(root_); lv_dropdown_set_options(ddm, "CH1\nCH2"); lv_dropdown_set_selected(ddm, app_->ch3().master - 1); lv_obj_set_pos(ddm, 34, 152); lv_obj_set_size(ddm, 110, 40); lv_obj_set_user_data(ddm, (void*)"ch3_master"); lv_obj_add_event_cb(ddm, dropdownEvent_, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t* ddp = lv_dropdown_create(root_); lv_dropdown_set_options(ddp, "0\n90\n180\n270"); uint8_t pidx = app_->ch3().phase / 90; lv_dropdown_set_selected(ddp, pidx); lv_obj_set_pos(ddp, 184, 152); lv_obj_set_size(ddp, 110, 40); lv_obj_set_user_data(ddp, (void*)"ch3_phase"); lv_obj_add_event_cb(ddp, dropdownEvent_, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* follow = lv_label_create(root_); lv_label_set_text(follow, "Drive strength is global from CONFIG for all channels."); lv_obj_set_pos(follow, 12, 206);
}

