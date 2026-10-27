#include "gui.h"
#include "gui_layout.h"
#include <drivers/status_led.h>
#include <utils/frequency_utils.h>
#include <utils/logger.h>
#include <cstring>

Gui GUI;
static Gui* gGui = nullptr;
lv_color_t Gui::buf1_[SCREEN_WIDTH * 24];

static void* packAction(uint8_t action, uint8_t arg) { return (void*)(uintptr_t)((action << 8) | arg); }

bool Gui::begin(AppController& app, TouchDriver& touch) {
    gGui = this;
    app_ = &app;
    touch_ = &touch;

    StatusLed::begin();

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
    lastHwReady_ = app.hardwareReady();
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

// ---------------------------------------------------------------------------
// Widget helpers

void Gui::on_(lv_obj_t* obj, lv_event_code_t code, Action action, uint8_t arg) {
    lv_obj_add_event_cb(obj, eventCb_, code, packAction((uint8_t)action, arg));
}

lv_obj_t* Gui::addLabel_(lv_obj_t* parent, const char* txt, const lv_font_t* font, lv_color_t color, int x, int y) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

lv_obj_t* Gui::addButton_(lv_obj_t* parent, const char* txt, int x, int y, int w, int h, lv_color_t color,
                          Action action, uint8_t arg, bool repeat) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, color, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_brightness(color) > 170 ? Colors::textDark() : Colors::text(), 0);
    lv_obj_center(l);
    if (repeat) {
        // Single tap = one step on release; holding repeats every LV_INDEV_DEF_LONG_PRESS_REP_TIME.
        on_(b, LV_EVENT_SHORT_CLICKED, action, arg);
        on_(b, LV_EVENT_LONG_PRESSED, action, arg);
        on_(b, LV_EVENT_LONG_PRESSED_REPEAT, action, arg);
    } else {
        on_(b, LV_EVENT_CLICKED, action, arg);
    }
    return b;
}

lv_obj_t* Gui::addChip_(lv_obj_t* parent, const char* txt, int x, int y, int w, int h, lv_color_t color,
                        Action action, uint8_t arg) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_style_bg_color(b, Colors::chipOff(), 0);
    lv_obj_set_style_bg_color(b, color, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(b, Colors::text(), 0);
    lv_obj_set_style_text_color(b, lv_color_brightness(color) > 170 ? Colors::textDark() : Colors::text(), LV_STATE_CHECKED);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    on_(b, LV_EVENT_CLICKED, action, arg);
    return b;
}

lv_obj_t* Gui::addCard_(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t borderColor) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, Colors::cardBg(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, Layout::CARD_BORDER, 0);
    lv_obj_set_style_border_color(card, borderColor, 0);
    lv_obj_set_style_pad_all(card, Layout::CARD_PAD, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    return card;
}

lv_obj_t* Gui::addSwitch_(lv_obj_t* parent, uint8_t ch, lv_color_t color) {
    lv_obj_t* sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 50, 26);
    lv_obj_set_style_bg_color(sw, color, LV_PART_INDICATOR | LV_STATE_CHECKED);
    // A bigger touch area than the 50x26 switch itself.
    lv_obj_set_ext_click_area(sw, 6);
    on_(sw, LV_EVENT_VALUE_CHANGED, Action::ChannelSwitch, ch);
    return sw;
}

void Gui::createHeader_(const char* title, lv_color_t color, uint8_t switchChannel) {
    lv_obj_t* bar = lv_obj_create(root_);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, SCREEN_WIDTH, Layout::HEADER_H);
    lv_obj_set_style_bg_color(bar, Colors::headerBg(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* stripe = lv_obj_create(bar);
    lv_obj_set_pos(stripe, 0, 0);
    lv_obj_set_size(stripe, 5, Layout::HEADER_H);
    lv_obj_set_style_bg_color(stripe, color, 0);
    lv_obj_set_style_border_width(stripe, 0, 0);
    lv_obj_set_style_radius(stripe, 0, 0);
    lv_obj_clear_flag(stripe, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* t = addLabel_(bar, title, &lv_font_montserrat_20, color, 0, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 14, 0);

    if (switchChannel) {
        w_.hdrSwitch = addSwitch_(bar, switchChannel, color);
        lv_obj_align(w_.hdrSwitch, LV_ALIGN_LEFT_MID, 158, 0);
        w_.hdrState = addLabel_(bar, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
        lv_obj_align(w_.hdrState, LV_ALIGN_LEFT_MID, 214, 0);
    }

    addButton_(bar, "MAIN", SCREEN_WIDTH - 66, 4, 62, Layout::HEADER_H - 8, Colors::gray(), Action::Navigate, (uint8_t)PageId::Main);
}

void Gui::setHeaderState_(bool on) {
    if (!w_.hdrSwitch) return;
    if (on) lv_obj_add_state(w_.hdrSwitch, LV_STATE_CHECKED);
    else lv_obj_clear_state(w_.hdrSwitch, LV_STATE_CHECKED);
    lv_label_set_text(w_.hdrState, on ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Navigation and refresh

void Gui::navigate(PageId page) {
    currentPage_ = page;
    if (page == PageId::CH1) channel_ = 1;
    else if (page == PageId::CH2) channel_ = 2;
    StatusLed::showPage(page);

    root_ = lv_scr_act();
    lv_obj_clean(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, Colors::screenBg(), 0);
    w_ = Widgets{};

    switch (page) {
        case PageId::Main: createMain_(); break;
        case PageId::CH1: createChannelPage_(1); break;
        case PageId::CH2: createChannelPage_(2); break;
        case PageId::CH3: createCH3_(); break;
        case PageId::Config: createConfig_(); break;
        case PageId::Numpad: createNumpad_(channel_); break;
    }
    refresh_();
}

// Deferred so the widget that triggered the navigation is not deleted inside its own event.
void Gui::requestNavigate_(PageId page) {
    pendingPage_ = page;
    // The new page must not receive the press that triggered the navigation.
    lv_indev_t* indev = lv_indev_get_act();
    if (indev) lv_indev_wait_release(indev);
    if (navPending_) return;
    navPending_ = true;
    lv_async_call(asyncNavigateCb_, this);
}

void Gui::asyncNavigateCb_(void* gui) {
    Gui* g = static_cast<Gui*>(gui);
    g->navPending_ = false;
    g->navigate(g->pendingPage_);
}

void Gui::refresh_() {
    switch (currentPage_) {
        case PageId::Main: refreshMain_(); break;
        case PageId::CH1:
        case PageId::CH2: refreshChannel_(); break;
        case PageId::CH3: refreshCH3_(); break;
        case PageId::Config: refreshConfig_(); break;
        case PageId::Numpad: break;
    }
}

void Gui::update() {
    // Without user input only the autosave indicator and the SI5351 connection change.
    if (millis() - lastUpdateMs_ < 300) return;
    lastUpdateMs_ = millis();
    if (app_->hardwareReady() != lastHwReady_) {
        lastHwReady_ = app_->hardwareReady();
        refresh_();
    } else if (currentPage_ == PageId::Config) {
        refreshSaveState_();
    }
}

// ---------------------------------------------------------------------------
// Events

void Gui::eventCb_(lv_event_t* e) {
    if (!gGui || !gGui->app_) return;
    uintptr_t packed = (uintptr_t)lv_event_get_user_data(e);
    gGui->handleAction_((Action)((packed >> 8) & 0xFF), (uint8_t)(packed & 0xFF), lv_event_get_current_target(e));
}

void Gui::handleAction_(Action action, uint8_t arg, lv_obj_t* obj) {
    AppController& app = *app_;
    const SystemSettings& sys = app.system();
    switch (action) {
        case Action::Navigate:
            requestNavigate_((PageId)arg);
            return;
        case Action::ChannelSwitch:
            app.setChannelEnabled(arg, lv_obj_has_state(obj, LV_STATE_CHECKED));
            break;
        case Action::FreqUp:
            app.adjustChannelFrequency(channel_, sys.delta_step);
            break;
        case Action::FreqDown:
            app.adjustChannelFrequency(channel_, -(int64_t)sys.delta_step);
            break;
        case Action::FreqTap:
            handleFreqTap_(obj);
            return;
        case Action::SetStep:
            app.setDeltaStep(DELTA_STEPS[arg]);
            break;
        case Action::StepUp: {
            uint8_t i = FrequencyUtils::stepIndex(sys.delta_step);
            if (i + 1 < DELTA_STEP_COUNT) app.setDeltaStep(DELTA_STEPS[i + 1]);
            break;
        }
        case Action::StepDown: {
            uint8_t i = FrequencyUtils::stepIndex(sys.delta_step);
            if (i > 0) app.setDeltaStep(DELTA_STEPS[i - 1]);
            break;
        }
        case Action::Ch3Master:
            app.setCH3Master(arg);
            break;
        case Action::Ch3Phase:
            app.setCH3Phase(arg * 90);
            break;
        case Action::CorrUp:
            app.setCorrection(sys.freq_correction + CORRECTION_STEPS[corrStepIdx_]);
            break;
        case Action::CorrDown:
            app.setCorrection(sys.freq_correction - CORRECTION_STEPS[corrStepIdx_]);
            break;
        case Action::CorrStepCycle:
            corrStepIdx_ = (corrStepIdx_ + 1) % CORRECTION_STEP_COUNT;
            break;
        case Action::Backlight:
            app.setBacklight((uint8_t)lv_slider_get_value(obj));
            break;
        case Action::Drive:
            app.setGlobalDriveStrength((lv_dropdown_get_selected(obj) + 1) * 2);
            break;
        case Action::NumpadKey:
            handleNumpadKey_(obj);
            return;
    }
    refresh_();
}

// ---------------------------------------------------------------------------
// Message box: one at a time, further messages wait in a short queue.

void Gui::showMessage(const char* title, const char* text) {
    if (!msgbox_) {
        openMessage_(title, text);
        return;
    }
    if (strcmp(msgText_, text) == 0) return;
    for (uint8_t i = 0; i < msgCount_; ++i) {
        if (strcmp(msgQueue_[i].text, text) == 0) return;
    }
    if (msgCount_ == MSG_QUEUE_LEN) {
        Logger::error("GUI", "Message queue full");
        return;
    }
    Message& m = msgQueue_[msgCount_++];
    snprintf(m.title, sizeof(m.title), "%s", title);
    snprintf(m.text, sizeof(m.text), "%s", text);
}

void Gui::openMessage_(const char* title, const char* text) {
    static const char* buttons[] = {"OK", ""};
    snprintf(msgText_, sizeof(msgText_), "%s", text);
    msgbox_ = lv_msgbox_create(nullptr, title, msgText_, buttons, false);
    lv_obj_center(msgbox_);
    lv_obj_add_event_cb(msgbox_, msgboxEventCb_, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(msgbox_, msgboxEventCb_, LV_EVENT_DELETE, nullptr);
}

void Gui::nextMessageCb_(void* gui) {
    Gui* g = static_cast<Gui*>(gui);
    if (g->msgbox_ || g->msgCount_ == 0) return;
    const Message m = g->msgQueue_[0];
    g->msgCount_--;
    memmove(&g->msgQueue_[0], &g->msgQueue_[1], g->msgCount_ * sizeof(Message));
    g->openMessage_(m.title, m.text);
}

void Gui::msgboxEventCb_(lv_event_t* e) {
    lv_obj_t* box = lv_event_get_current_target(e);
    if (!gGui) return;
    if (gGui->msgbox_ == box) {
        gGui->msgbox_ = nullptr;
        gGui->msgText_[0] = '\0';
    }
    // lv_msgbox footer buttons have no default action in LVGL 8.3.
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        // Block a second tap before the async close runs, it would close a deleted box.
        lv_obj_clear_flag(lv_msgbox_get_btns(box), LV_OBJ_FLAG_CLICKABLE);
        lv_msgbox_close_async(box);
    } else if (gGui->msgCount_ > 0) {
        // Not inside the delete event of the old box.
        lv_async_call(nextMessageCb_, gGui);
    }
}
