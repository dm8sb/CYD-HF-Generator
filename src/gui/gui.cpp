#include "gui.h"
#include "gui_layout.h"
#include <drivers/status_led.h>
#include <utils/frequency_utils.h>
#include <utils/logger.h>
#include <cstring>
#include <math.h>

lv_color_t Gui::buf1_[SCREEN_WIDTH * 24];

static void* packAction(uint8_t action, uint8_t arg) { return (void*)(uintptr_t)((action << 8) | arg); }

void Gui::begin(AppController& app, TouchDriver& touch, DisplayPanel& display) {
    app_ = &app;
    touch_ = &touch;
    display_ = &display;
    lastConnections_ = app.connectionCount();

    StatusLed::begin();

    lv_init();

    lv_disp_draw_buf_init(&drawBuf_, buf1_, nullptr, SCREEN_WIDTH * 24);
    lv_disp_drv_init(&dispDrv_);
    dispDrv_.hor_res = SCREEN_WIDTH;
    dispDrv_.ver_res = SCREEN_HEIGHT;
    dispDrv_.flush_cb = flushCb_;
    dispDrv_.draw_buf = &drawBuf_;
    // The static LVGL callbacks find this instance through the driver user data.
    dispDrv_.user_data = this;
    lv_disp_drv_register(&dispDrv_);

    lv_indev_drv_init(&indevDrv_);
    indevDrv_.type = LV_INDEV_TYPE_POINTER;
    indevDrv_.read_cb = touchCb_;
    indevDrv_.user_data = this;
    lv_indev_drv_register(&indevDrv_);

    lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lastHwState_ = hardwareState_();
    navigate(PageId::Main);
    Logger::info("GUI", "LVGL initialized");
}

Gui* Gui::from_(lv_obj_t* obj) {
    return static_cast<Gui*>(lv_obj_get_disp(obj)->driver->user_data);
}

void Gui::flushCb_(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    Gui* g = static_cast<Gui*>(disp->user_data);
    const int16_t w = area->x2 - area->x1 + 1;
    const int16_t h = area->y2 - area->y1 + 1;
    g->display_->flush(area->x1, area->y1, w, h, (uint16_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

void Gui::touchCb_(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    Gui* g = static_cast<Gui*>(drv->user_data);
    if (g->touch_) g->touch_->readLvgl(drv, data);
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

// Buttons near the screen edge take taps up to the edge (audit U-5): the click area grows by
// EDGE_REACH on all sides, the hit test keeps only the sides that face an edge.
static void edgeHitTestCb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_current_target(e);
    lv_hit_test_info_t* info = (lv_hit_test_info_t*)lv_event_get_param(e);
    lv_area_t c;
    lv_obj_get_coords(obj, &c);
    const InputLogic::Area a = InputLogic::edgeArea({(int16_t)c.x1, (int16_t)c.y1, (int16_t)c.x2, (int16_t)c.y2},
                                                    InputLogic::EDGE_REACH, SCREEN_WIDTH, SCREEN_HEIGHT);
    info->res = InputLogic::contains(a, (int16_t)info->point->x, (int16_t)info->point->y);
}

static void reachEdges(lv_obj_t* obj) {
    lv_obj_set_ext_click_area(obj, InputLogic::EDGE_REACH);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(obj, edgeHitTestCb, LV_EVENT_HIT_TEST, nullptr);
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
    lv_obj_set_style_text_color(l, Colors::textOn(color), 0);
    lv_obj_center(l);
    reachEdges(b);
    // A press that starts scrolling a parent (CONFIG) triggers nothing: once scrolling, LVGL 8.3
    // sends neither CLICKED / SHORT_CLICKED nor LONG_PRESSED(_REPEAT), only RELEASED.
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
    // The border shows an unselected chip on the dark background (1.5:1 without, audit U-7).
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, Colors::outline(), 0);
    lv_obj_set_style_border_color(b, color, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(b, Colors::text(), 0);
    lv_obj_set_style_text_color(b, Colors::textOn(color), LV_STATE_CHECKED);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    reachEdges(b);
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
    // Track off 5.2:1 against the card, 3.4:1 against the knob (theme: 1.4:1, audit U-7).
    lv_obj_set_style_bg_color(sw, Colors::outline(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, Colors::knob(), LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, color, LV_PART_INDICATOR | LV_STATE_CHECKED);
    // A bigger touch area than the 50x26 switch itself.
    lv_obj_set_ext_click_area(sw, 6);
    on_(sw, LV_EVENT_VALUE_CHANGED, Action::ChannelSwitch, ch);
    return sw;
}

lv_obj_t* Gui::createHeader_(const char* title, lv_color_t color, uint8_t switchChannel) {
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
        lv_obj_align(w_.hdrSwitch, LV_ALIGN_LEFT_MID, Layout::HEADER_SWITCH_X, 0);
        w_.hdrState = addLabel_(bar, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
        styleState_(w_.hdrState);
        lv_obj_align(w_.hdrState, LV_ALIGN_LEFT_MID, Layout::HEADER_STATE_X, 0);
    }

    // 36 px high and up to the top and right edge (audit U-5).
    addButton_(bar, "MAIN", SCREEN_WIDTH - 66, 1, 62, Layout::HEADER_H - 2, Colors::gray(), Action::Navigate, (uint8_t)PageId::Main);
    return bar;
}

void Gui::setHeaderState_(bool on, const StatusText::Text& state) {
    if (!w_.hdrSwitch) return;
    if (on) lv_obj_add_state(w_.hdrSwitch, LV_STATE_CHECKED);
    else lv_obj_clear_state(w_.hdrSwitch, LV_STATE_CHECKED);
    showState_(w_.hdrState, state);
}

void Gui::styleState_(lv_obj_t* label) {
    lv_obj_set_style_radius(label, 3, 0);
    lv_obj_set_style_pad_hor(label, 2, 0);
}

// OFF as gray text, every other state filled with its tone and dark text: a filled label marks
// an active or faulty output also for color-blind users and from a distance (audit U-19).
void Gui::showState_(lv_obj_t* label, const StatusText::Text& state) {
    lv_label_set_text(label, state.text);
    const bool filled = state.tone != StatusText::Tone::Muted;
    lv_obj_set_style_bg_opa(label, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(label, Colors::forTone(state.tone), 0);
    lv_obj_set_style_text_color(label, filled ? Colors::textDark() : Colors::muted(), 0);
}

// Includes SI5351 rounding, the Integer raster and spread spectrum; assumes a correct
// frequency correction.
void Gui::showOutput_(uint8_t ch, uint64_t requested, const char* prefix) {
    char out[64], buf[72];
    const double dev = FrequencyUtils::formatOutput(app_->actualHz(ch), requested, app_->spreadOf(ch), out, sizeof(out));
    snprintf(buf, sizeof(buf), "%s%s", prefix, out);
    lv_label_set_text(w_.info, buf);
    const bool warn = fabs(dev) >= FrequencyUtils::DEVIATION_WARN_HZ;
    lv_obj_set_style_text_color(w_.info, warn ? Colors::warn() : Colors::muted(), 0);
}

// ---------------------------------------------------------------------------
// Navigation and refresh

void Gui::navigate(PageId page) {
    if (currentPage_ == PageId::Config && w_.cfgList) {
        // Where a running snap animation ends, so the restored position is a row top.
        lv_point_t end;
        lv_obj_get_scroll_end(w_.cfgList, &end);
        configScrollY_ = end.y;
    }
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
    // Without user input only the autosave indicator, the SI5351 connection and
    // output errors (cleared by the periodic retry) change.
    if (millis() - lastUpdateMs_ < GUI_UPDATE_INTERVAL_MS) return;
    lastUpdateMs_ = millis();
    // SI5351 reconnected (possibly between two samples): connection dialogs are outdated.
    if (app_->connectionCount() != lastConnections_) {
        lastConnections_ = app_->connectionCount();
        if (app_->hardwareReady()) dismissLinkMessages_();
    }
    const uint8_t hwState = hardwareState_();
    if (hwState != lastHwState_) {
        lastHwState_ = hwState;
        refresh_();
    } else {
        refreshSaveState_(); // MAIN status bar and CONFIG header; no-op on the other pages
    }
}

// Changes whenever the connection or an output error state changes.
uint8_t Gui::hardwareState_() const {
    uint8_t s = app_->hardwareReady() ? 1 : 0;
    if (app_->channelStatus(1) == ChannelStatus::Error) s |= 2;
    if (app_->channelStatus(2) == ChannelStatus::Error) s |= 4;
    if (app_->ch3Status() == Ch3Status::Error) s |= 8;
    return s;
}

// ---------------------------------------------------------------------------
// Events

void Gui::eventCb_(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_current_target(e);
    // The event user data carries the packed action, so the instance comes from the display.
    Gui* g = from_(obj);
    uintptr_t packed = (uintptr_t)lv_event_get_user_data(e);
    g->handleAction_((Action)((packed >> 8) & 0xFF), (uint8_t)(packed & 0xFF), obj);
}

// CONFIG dropdowns (page_config.cpp) list their options in enum order; the index is the value.
static_assert((uint8_t)TuningMode::Fractional == 0 && (uint8_t)TuningMode::LowNoise == 1 &&
                  (uint8_t)TuningMode::Integer == 2, "Tuning dropdown order");
static_assert((uint8_t)Spread::Off == 0 && (uint8_t)Spread::Down05 == 1 && (uint8_t)Spread::Down15 == 2 &&
                  (uint8_t)Spread::Center05 == 3, "Spread CH1 dropdown order");
static_assert((uint8_t)OffState::Low == 0 && (uint8_t)OffState::High == 1 && (uint8_t)OffState::HighZ == 2,
              "Off state dropdown order");

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
        case Action::BacklightDown:
            app.stepBacklight(-1);
            break;
        case Action::BacklightUp:
            app.stepBacklight(1);
            break;
        case Action::Drive:
            app.setGlobalDriveStrength((lv_dropdown_get_selected(obj) + 1) * 2);
            break;
        // The dropdown options are in enum order (checked below). refresh_() shows the resulting state, e.g.
        // spread spectrum turned off by the Integer mode.
        case Action::Tuning:
            app.setTuningMode((TuningMode)lv_dropdown_get_selected(obj));
            break;
        case Action::Spread:
            app.setSpread((Spread)lv_dropdown_get_selected(obj));
            break;
        case Action::OffState: {
            const OffState s = (OffState)lv_dropdown_get_selected(obj);
            if (s == OffState::High && sys.off_state != OffState::High) {
                confirmOffStateHigh_(); // the dropdown shows the old value until confirmed
                break;
            }
            app.setOffState(s);
            break;
        }
        case Action::NumpadKey:
            handleNumpadKey_(obj);
            return;
    }
    refresh_();
}

// ---------------------------------------------------------------------------
// Message box: one at a time, further messages wait in a short queue.

void Gui::showMessage(const char* title, const char* text) {
    switch (messages_.post(title, text)) {
        case MessageQueue::Post::Open: openMessage_(); break;
        case MessageQueue::Post::Full: Logger::error("GUI", "Message queue full"); break;
        case MessageQueue::Post::Queued:
        case MessageQueue::Post::Duplicate: break;
    }
}

// Dialog with large buttons (audit U-4): 300 px wide, buttons 120 x 40 px with 6.7:1 against the
// dialog, centered. A button with LV_BTNMATRIX_CTRL_CHECKED is drawn in the warning color.
static void styleDialog(lv_obj_t* box, uint8_t buttons) {
    static constexpr int DIALOG_W = 300;
    static constexpr int BTN_W = 120;
    static constexpr int BTN_H = 40;
    static constexpr int BTN_GAP = 12;
    lv_obj_set_width(box, DIALOG_W);
    lv_obj_set_style_bg_color(box, Colors::cardBg(), 0);
    lv_obj_set_style_border_color(box, Colors::muted(), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* btns = lv_msgbox_get_btns(box);
    lv_obj_set_size(btns, buttons * BTN_W + (buttons - 1) * BTN_GAP, BTN_H);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_style_pad_column(btns, BTN_GAP, 0);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_bg_color(btns, Colors::dialogButton(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btns, Colors::textDark(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(btns, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(btns, Colors::warn(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btns, Colors::textDark(), LV_PART_ITEMS | LV_STATE_CHECKED);
}

// Creates the box of the open message unless it exists (several async calls may be pending).
void Gui::openMessage_() {
    static const char* buttons[] = {"OK", ""};
    if (shownBox_ || !messages_.isOpen()) return;
    msgbox_ = shownBox_ = lv_msgbox_create(nullptr, messages_.openTitle(), messages_.openText(), buttons, false);
    styleDialog(msgbox_, 1);
    lv_obj_center(msgbox_);
    lv_obj_add_event_cb(msgbox_, msgboxEventCb_, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(msgbox_, msgboxEventCb_, LV_EVENT_DELETE, nullptr);
}

void Gui::dismissLinkMessages_() {
    // Output errors from the restore stay.
    if (!messages_.dropMatching(AppController::isLinkText)) return;
    if (msgbox_) {
        lv_msgbox_close(msgbox_); // the delete event moves on to the next message
    } else if (!shownBox_ && messages_.close()) {
        // The outdated message was still waiting for its box; the next one gets it.
        lv_async_call(nextMessageCb_, this);
    }
    // shownBox_ without msgbox_: already closing after OK, its delete event moves on.
}

void Gui::nextMessageCb_(void* gui) { static_cast<Gui*>(gui)->openMessage_(); }

void Gui::showModes_(lv_obj_t* label, uint8_t ch) {
    const SystemSettings& sys = app_->system();
    char buf[64];
    const StatusText::Tone tone =
        StatusText::modeSummary(sys.tuning_mode, app_->spreadOf(ch), sys.drive_ma, sys.off_state, buf, sizeof(buf));
    lv_label_set_text(label, buf);
    lv_obj_set_style_text_color(label, Colors::forTone(tone), 0);
}

void Gui::confirmOffStateHigh_() {
    static const char* buttons[] = {"Cancel", "High", ""};
    lv_obj_t* box = lv_msgbox_create(nullptr, "Off state High",
                                     "Switched-off outputs then carry DC (supply voltage, mostly 3.3 V). "
                                     "Do not use with DC-coupled mixers or inputs.",
                                     buttons, false);
    styleDialog(box, 2);
    lv_btnmatrix_set_btn_ctrl(lv_msgbox_get_btns(box), 1, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_center(box);
    lv_obj_add_event_cb(box, confirmEventCb_, LV_EVENT_VALUE_CHANGED, nullptr);
    refresh_();
}

void Gui::confirmEventCb_(lv_event_t* e) {
    lv_obj_t* box = lv_event_get_current_target(e);
    Gui* g = from_(box);
    const char* btn = lv_msgbox_get_active_btn_text(box);
    if (btn && strcmp(btn, "High") == 0) g->app_->setOffState(OffState::High);
    lv_obj_clear_flag(lv_msgbox_get_btns(box), LV_OBJ_FLAG_CLICKABLE);
    lv_msgbox_close_async(box);
    g->refresh_();
}

void Gui::msgboxEventCb_(lv_event_t* e) {
    lv_obj_t* box = lv_event_get_current_target(e);
    Gui* g = from_(box);
    // lv_msgbox footer buttons have no default action in LVGL 8.3.
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        // Closing: no second close from dismissLinkMessages_(), no second tap before the
        // async close runs (it would close a deleted box).
        if (g->msgbox_ == box) g->msgbox_ = nullptr;
        lv_obj_clear_flag(lv_msgbox_get_btns(box), LV_OBJ_FLAG_CLICKABLE);
        lv_msgbox_close_async(box);
    } else if (box == g->shownBox_) {
        g->shownBox_ = g->msgbox_ = nullptr;
        // The next message opens outside the delete event of the old box.
        if (g->messages_.close()) lv_async_call(nextMessageCb_, g);
    }
}
