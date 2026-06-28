#include "gui.h"
#include <pins.h>
#include <utils/frequency_utils.h>
#include <utils/logger.h>
#include <cstring>

Gui GUI;
static Gui* gGui = nullptr;
lv_color_t Gui::buf1_[SCREEN_WIDTH * 24];

bool Gui::begin(AppController& app, TouchDriver& touch) {
    gGui = this;
    app_ = &app;
    touch_ = &touch;

    pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
    setLedForPage_(PageId::Main);

    lv_init();
    tft_.begin();
    tft_.setRotation(1);
    tft_.fillScreen(TFT_BLACK);

    lv_disp_draw_buf_init(&drawBuf_, buf1_, nullptr, SCREEN_WIDTH * 24);
    lv_disp_drv_init(&dispDrv_);
    dispDrv_.hor_res = SCREEN_WIDTH;
    dispDrv_.ver_res = SCREEN_HEIGHT;
    dispDrv_.flush_cb = flushCb_;
    dispDrv_.draw_buf = &drawBuf_;
    lv_disp_drv_register(&dispDrv_);

    lv_indev_drv_init(&indevDrv_);
    indevDrv_.type = LV_INDEV_TYPE_POINTER;
    indevDrv_.read_cb = touchCb_;
    lv_indev_drv_register(&indevDrv_);

    lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    navigate(PageId::Main);
    Logger::info("GUI", "LVGL initialized");
    return true;
}

void Gui::flushCb_(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    if (!gGui) return;
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gGui->tft_.startWrite();
    gGui->tft_.setAddrWindow(area->x1, area->y1, w, h);
    gGui->tft_.pushColors((uint16_t*)&color_p->full, w * h, true);
    gGui->tft_.endWrite();
    lv_disp_flush_ready(disp);
}

void Gui::touchCb_(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    if (gGui && gGui->touch_) gGui->touch_->readLvgl(drv, data);
}

void Gui::setLedForPage_(PageId page) {
    digitalWrite(PIN_LED_R, HIGH); digitalWrite(PIN_LED_G, HIGH); digitalWrite(PIN_LED_B, HIGH);
    if (page == PageId::CH1) digitalWrite(PIN_LED_R, LOW);
    else if (page == PageId::CH2) digitalWrite(PIN_LED_G, LOW);
    else if (page == PageId::CH3) digitalWrite(PIN_LED_B, LOW);
}

static lv_obj_t* addBtn(lv_obj_t* parent, const char* txt, int x, int y, int w, int h, const char* action,
                        lv_color_t color = lv_color_hex(0x2F4057)) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_obj_set_user_data(b, (void*)action);
    lv_obj_add_event_cb(b, Gui::btnEvent_, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_bg_color(b, color, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 10, 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_30, 0);
    lv_obj_t* l = lv_label_create(b); lv_label_set_text(l, txt); lv_obj_center(l);
    return b;
}

lv_obj_t* Gui::addButton_(lv_obj_t* parent, const char* txt, int x, int y, int w, int h, const char* action, lv_color_t color) {
    return addBtn(parent, txt, x, y, w, h, action, color);
}

lv_obj_t* Gui::addCard_(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t borderColor) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x151A22), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, borderColor, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    return card;
}

void Gui::createHeader_(const char* title, lv_color_t color, bool showBack, bool showSwitch, const char* switchAction, bool switchState) {
    lv_obj_t* bar = lv_obj_create(root_);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, SCREEN_WIDTH, 40);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x111820), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* stripe = lv_obj_create(bar);
    lv_obj_set_pos(stripe, 0, 0);
    lv_obj_set_size(stripe, 5, 40);
    lv_obj_set_style_bg_color(stripe, color, 0);
    lv_obj_set_style_border_width(stripe, 0, 0);
    lv_obj_set_style_radius(stripe, 0, 0);

    lv_obj_t* t = lv_label_create(bar);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 12, 8);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, color, 0);

    if (showSwitch && switchAction) {
        lv_obj_t* sw = lv_switch_create(bar);
        lv_obj_set_pos(sw, 132, 6);
        lv_obj_set_user_data(sw, (void*)switchAction);
        lv_obj_add_event_cb(sw, switchEvent_, LV_EVENT_VALUE_CHANGED, nullptr);
        if (switchState) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_t* sl = lv_label_create(bar);
        lv_label_set_text(sl, switchState ? "ON" : "OFF");
        lv_obj_set_pos(sl, 190, 11);
        lv_obj_set_style_text_font(sl, &lv_font_montserrat_16, 0);
    }

    if (showBack) addBtn(bar, "MAIN", 260, 4, 54, 32, "nav_main", lv_color_hex(0x455A64));
}

void Gui::navigate(PageId page, uint8_t selectedChannel) {
    currentPage_ = page;
    if (selectedChannel) selectedChannel_ = selectedChannel;
    setLedForPage_(page);
    lv_obj_clean(lv_scr_act());
    root_ = lv_scr_act();
    switch (page) {
        case PageId::Main: createMain_(); break;
        case PageId::CH1: createChannelPage_(1); break;
        case PageId::CH2: createChannelPage_(2); break;
        case PageId::CH3: createCH3_(); break;
        case PageId::Config: createConfig_(); break;
        case PageId::Numpad: createNumpad_(selectedChannel_); break;
    }
}

void Gui::btnEvent_(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    const char* a = (const char*)lv_obj_get_user_data(obj);
    if (!gGui || !gGui->app_) return;
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
        const char* txt = lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj));
        if (!txt) return;
        if (!strcmp(txt, "<--")) lv_textarea_del_char(ta);
        else if (!strcmp(txt, "Clear")) lv_textarea_set_text(ta, "");
        else if (!strcmp(txt, "Hz") || !strcmp(txt, "kHz") || !strcmp(txt, "MHz")) { uint64_t hz; if (FrequencyUtils::parseFrequency(lv_textarea_get_text(ta), txt, &hz)) gGui->app_->setChannelFrequency(gGui->selectedChannel_, hz); gGui->navigate(gGui->selectedChannel_ == 1 ? PageId::CH1 : PageId::CH2); }
        else lv_textarea_add_text(ta, txt);
        return;
    }
    if (!a) return;
    if (!strcmp(a, "nav_main")) gGui->navigate(PageId::Main);
    else if (!strcmp(a, "nav_ch1")) gGui->navigate(PageId::CH1, 1);
    else if (!strcmp(a, "nav_ch2")) gGui->navigate(PageId::CH2, 2);
    else if (!strcmp(a, "nav_ch3")) gGui->navigate(PageId::CH3);
    else if (!strcmp(a, "nav_cfg")) gGui->navigate(PageId::Config);
    else if (!strcmp(a, "ch1_plus")) { gGui->app_->adjustChannelFrequency(1, gGui->app_->system().delta_step); gGui->navigate(PageId::CH1, 1); }
    else if (!strcmp(a, "ch1_minus")) { gGui->app_->adjustChannelFrequency(1, -(int64_t)gGui->app_->system().delta_step); gGui->navigate(PageId::CH1, 1); }
    else if (!strcmp(a, "ch2_plus")) { gGui->app_->adjustChannelFrequency(2, gGui->app_->system().delta_step); gGui->navigate(PageId::CH2, 2); }
    else if (!strcmp(a, "ch2_minus")) { gGui->app_->adjustChannelFrequency(2, -(int64_t)gGui->app_->system().delta_step); gGui->navigate(PageId::CH2, 2); }
    else if (!strcmp(a, "num1")) gGui->navigate(PageId::Numpad, 1);
    else if (!strcmp(a, "num2")) gGui->navigate(PageId::Numpad, 2);
    else if (!strcmp(a, "delta_x10")) { uint32_t d = gGui->app_->system().delta_step; if (d <= 10000000UL) d *= 10; gGui->app_->setDeltaStep(d); gGui->navigate(PageId::Config); }
    else if (!strcmp(a, "delta_d10")) { uint32_t d = gGui->app_->system().delta_step / 10; if (d == 0) d = 1; gGui->app_->setDeltaStep(d); gGui->navigate(PageId::Config); }
    else if (!strcmp(a, "corr_p")) { gGui->app_->setCorrection(gGui->app_->system().freq_correction + 10); gGui->navigate(PageId::Config); }
    else if (!strcmp(a, "corr_m")) { gGui->app_->setCorrection(gGui->app_->system().freq_correction - 10); gGui->navigate(PageId::Config); }
    // Touch calibration button/routine disabled; default calibration is always used.
}

void Gui::sliderEvent_(lv_event_t* e) {
    const char* a = (const char*)lv_obj_get_user_data(lv_event_get_target(e));
    if (a && !strcmp(a, "bl")) { gGui->app_->setBacklight((uint8_t)lv_slider_get_value(lv_event_get_target(e))); }
}

void Gui::switchEvent_(lv_event_t* e) {
    lv_obj_t* o = lv_event_get_target(e); const char* a = (const char*)lv_obj_get_user_data(o); bool en = lv_obj_has_state(o, LV_STATE_CHECKED);
    if (!strcmp(a, "ch1_en")) { gGui->app_->setChannelEnabled(1, en); gGui->navigate(PageId::CH1, 1); }
    else if (!strcmp(a, "ch2_en")) { gGui->app_->setChannelEnabled(2, en); gGui->navigate(PageId::CH2, 2); }
    else if (!strcmp(a, "ch3_en")) { gGui->app_->setChannelEnabled(3, en); gGui->navigate(PageId::CH3); }
}

void Gui::dropdownEvent_(lv_event_t* e) {
    lv_obj_t* o = lv_event_get_target(e); const char* a = (const char*)lv_obj_get_user_data(o); uint16_t idx = lv_dropdown_get_selected(o);
    if (!strcmp(a, "ch3_master")) gGui->app_->setCH3Master(idx + 1);
    else if (!strcmp(a, "ch3_phase")) gGui->app_->setCH3Phase(idx * 90);
    else if (!strcmp(a, "def_drive")) { uint8_t d = (idx + 1) * 2; gGui->app_->setGlobalDriveStrength(d); }
}

void Gui::taEvent_(lv_event_t*) {}

// Touch calibration code removed; firmware always uses default calibration values.

void Gui::showMessage(const char* title, const char* text) {
    static const char* buttons[] = {"OK", ""};
    lv_obj_t* m = lv_msgbox_create(nullptr, title, text, buttons, true);
    lv_obj_center(m);
}

void Gui::update() {
    // Touch calibration is disabled; default calibration values are always used.
}

void Gui::refreshCurrent_() { navigate(currentPage_, selectedChannel_); }
