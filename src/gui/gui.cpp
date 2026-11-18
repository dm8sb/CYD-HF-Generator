// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui.h"
#include "gui_layout.h"
#include <net/web_state.h>
#include <net/wifi_policy.h>
#include <utils/frequency_utils.h>
#include <utils/logger.h>
#include <utils/sanitize.h>
#include <cstring>
#include <math.h>

lv_color_t Gui::buf1_[SCREEN_WIDTH * 24];

static void* packAction(uint8_t action, uint8_t arg) { return (void*)(uintptr_t)((action << 8) | arg); }

void Gui::begin(AppController& app, TouchDriver& touch, DisplayPanel& display) {
    app_ = &app;
    touch_ = &touch;
    display_ = &display;
    lastConnections_ = app.connectionCount();


    // LVGL warnings through the switchable logger (C-16), already for lv_init().
    lv_log_register_print_cb([](const char* line) { Logger::raw(line); });
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
    indevDrv_.long_press_time = REPEAT_DELAY_MS; // first repeat of a held +/- (U-20)
    lv_indev_drv_register(&indevDrv_);

    lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lastRevision_ = app.revision();
#if TEST_HIT_MAP
    createHitMap_();
#else
    navigate(PageId::Main);
#endif
    Logger::info("GUI", "LVGL initialized");
}

Gui* Gui::from_(lv_obj_t* obj) {
    return static_cast<Gui*>(lv_obj_get_disp(obj)->driver->user_data);
}

void Gui::flushCb_(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    Gui* g = static_cast<Gui*>(disp->user_data);
    ++g->flushCount_;
    const int16_t w = area->x2 - area->x1 + 1;
    const int16_t h = area->y2 - area->y1 + 1;
    g->display_->flush(area->x1, area->y1, w, h, (uint16_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

void Gui::touchCb_(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    Gui* g = static_cast<Gui*>(drv->user_data);
    if (g->touch_) g->touch_->readLvgl(drv, data);
    // Locked (U-13): LVGL sees no touch; holding UNLOCK_HOLD_MS unlocks.
    if (g->lock_.blocking()) {
        const InputLogic::ScreenLock::Event e = g->lock_.update(data->state == LV_INDEV_STATE_PR, millis());
        data->state = LV_INDEV_STATE_REL;
        g->handleLockEvent_(e);
    }
}

void Gui::handleLockEvent_(InputLogic::ScreenLock::Event e) {
    using Ev = InputLogic::ScreenLock::Event;
    if (e == Ev::None) return;
    if (!lockHint_) {
        lockHint_ = lv_label_create(lv_layer_top());
        Ui::setText(lockHint_, "Locked: hold 2 s to unlock");
        lv_obj_set_style_text_font(lockHint_, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lockHint_, Colors::textDark(), 0);
        lv_obj_set_style_bg_color(lockHint_, Colors::warn(), 0);
        lv_obj_set_style_bg_opa(lockHint_, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(lockHint_, 6, 0);
        lv_obj_set_style_pad_all(lockHint_, 8, 0);
        lv_obj_center(lockHint_);
        lv_obj_clear_flag(lockHint_, LV_OBJ_FLAG_CLICKABLE);
    }
    if (e == Ev::Pressed) {
        lv_obj_clear_flag(lockHint_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lockHint_, LV_OBJ_FLAG_HIDDEN);
    }
    if (e == Ev::Unlocked) {
        Logger::info("GUI", "Screen unlocked");
        refresh_();
    }
}

// ---------------------------------------------------------------------------
// Widget helpers

void Gui::on_(lv_obj_t* obj, lv_event_code_t code, Action action, uint8_t arg) {
    lv_obj_add_event_cb(obj, eventCb_, code, packAction((uint8_t)action, arg));
}

lv_obj_t* Gui::addLabel_(lv_obj_t* parent, const char* txt, const lv_font_t* font, lv_color_t color, int x, int y) {
    lv_obj_t* l = lv_label_create(parent);
    Ui::setText(l, txt);
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
    Ui::setText(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, Colors::textOn(color), 0);
    lv_obj_center(l);
    reachEdges(b);
    // A press that starts scrolling a parent triggers nothing (no page scrolls any more): once
    // scrolling, LVGL 8.3 sends neither CLICKED / SHORT_CLICKED nor LONG_PRESSED(_REPEAT).
    if (repeat) {
        // Single tap = one step on release; holding repeats every LV_INDEV_DEF_LONG_PRESS_REP_TIME.
        // The press start lets a long hold speed up (U-20).
        on_(b, LV_EVENT_PRESSED, action, arg);
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
    Ui::setText(l, txt);
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
    lv_obj_set_style_bg_color(sw, color, partState(LV_PART_INDICATOR, LV_STATE_CHECKED));
    // A bigger touch area than the 50x26 switch itself.
    lv_obj_set_ext_click_area(sw, 6);
    on_(sw, LV_EVENT_VALUE_CHANGED, Action::ChannelSwitch, ch);
    return sw;
}

lv_obj_t* Gui::createHeader_(const char* title, lv_color_t color, uint8_t switchChannel, PageId back) {
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

    addButton_(bar, LV_SYMBOL_LEFT, Layout::BACK_X, Layout::BACK_Y, Layout::BACK_W, Layout::BACK_H, Colors::gray(),
               Action::Navigate, (uint8_t)back);
    lv_obj_t* t = addLabel_(bar, title, &lv_font_montserrat_20, color, 0, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, Layout::HEADER_TITLE_X, 0);
    if (!switchChannel) {
        // A long title ends in "..." before the right edge.
        lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
        lv_obj_set_width(t, SCREEN_WIDTH - Layout::HEADER_TITLE_X - 8);
    }

    if (switchChannel) {
        // Right after the title: "CH3 IQ" is wider than "CH1" and would end under the switch.
        lv_obj_update_layout(t);
        const int switchX = Layout::HEADER_TITLE_X + lv_obj_get_width(t) + Layout::HEADER_TITLE_GAP;
        w_.hdrSwitch = addSwitch_(bar, switchChannel, color);
        lv_obj_align(w_.hdrSwitch, LV_ALIGN_LEFT_MID, switchX, 0);
        w_.hdrState = addLabel_(bar, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
        styleState_(w_.hdrState);
        lv_obj_align(w_.hdrState, LV_ALIGN_LEFT_MID, switchX + Layout::HEADER_STATE_DX, 0);
    }

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
    Ui::setText(label, state.text);
    const bool filled = state.tone != StatusText::Tone::Muted;
    Ui::setBgOpa(label, filled ? LV_OPA_COVER : LV_OPA_TRANSP);
    Ui::setBgColor(label, Colors::forTone(state.tone));
    Ui::setTextColor(label, filled ? Colors::textDark() : Colors::muted());
}

// Includes SI5351 rounding, the Integer raster and spread spectrum; assumes a correct
// frequency correction.
void Gui::showOutput_(uint8_t ch, uint64_t requested, const char* prefix) {
    char out[64], buf[72];
    const double dev = FrequencyUtils::formatOutput(app_->actualHz(ch), requested, app_->spreadOf(ch), out, sizeof(out));
    snprintf(buf, sizeof(buf), "%s%s", prefix, out);
    Ui::setText(w_.info, buf);
    const bool warn = fabs(dev) >= FrequencyUtils::DEVIATION_WARN_HZ;
    Ui::setTextColor(w_.info, warn ? Colors::warn() : Colors::muted());
}

// ---------------------------------------------------------------------------
// Navigation and refresh

void Gui::navigate(PageId page) {
    // The numpad edits the correction when opened from CONFIG, else the channel frequency.
    if (page == PageId::Numpad) {
        numpadPpm_ = currentPage_ == PageId::Config;
        if (!numpadPpm_) numpadReturn_ = currentPage_; // CH1, CH2 or CH3 (master frequency)
    }
    currentPage_ = page;
    if (page == PageId::CH1) channel_ = 1;
    else if (page == PageId::CH2) channel_ = 2;

    root_ = lv_scr_act();
    lv_obj_clean(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, Colors::screenBg(), 0);
    w_ = Widgets{};
    wifiPages_.clearWidgets();

    switch (page) {
        case PageId::Main: createMain_(); break;
        case PageId::CH1: createChannelPage_(1); break;
        case PageId::CH2: createChannelPage_(2); break;
        case PageId::CH3: createCH3_(); break;
        case PageId::Config: createConfig_(); break;
        case PageId::Numpad: createNumpad_(channel_); break;
        case PageId::TouchCal: createTouchCal_(); break;
        case PageId::Wifi:
        case PageId::WifiScan:
        case PageId::TextEntry:
        case PageId::WifiAp:
        case PageId::WifiDevice: wifiPages_.create(page); break;
    }
    // Dialogs wait while the calibration covers the screen.
    if (page != PageId::TouchCal) {
        openMessage_();
        showConfirm_();
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
#if TEST_HIT_MAP
    return; // the hit map has none of the page widgets
#endif
    switch (currentPage_) {
        case PageId::Main: refreshMain_(); break;
        case PageId::CH1:
        case PageId::CH2: refreshChannel_(); break;
        case PageId::CH3: refreshCH3_(); break;
        case PageId::Config: refreshConfig_(); break;
        case PageId::Numpad:
        case PageId::TouchCal: break;
        case PageId::Wifi:
        case PageId::WifiScan:
        case PageId::TextEntry:
        case PageId::WifiAp:
        case PageId::WifiDevice: wifiPages_.refresh(currentPage_); break;
    }
    // A redraw right after an action is the redraw of this state: update() must not repeat it
    // (AF12, every touch action redrew twice).
    lastRevision_ = app_->revision();
    lastView_ = statusView_();
    ++refreshCount_;
}

void Gui::update() {
    wifiPages_.update();
    if (currentPage_ == PageId::TouchCal) {
        tickTouchCal_();
        return;
    }
    // Without user input only the autosave indicator, the SI5351 connection and
    // output errors (cleared by the periodic retry) change.
    if (millis() - lastUpdateMs_ < GUI_UPDATE_INTERVAL_MS) return;
    lastUpdateMs_ = millis();
    if (LOG_DEBUG && lastUpdateMs_ - countLogMs_ >= 60000) {
        Logger::debugValue("GUI", "Redraws per min", refreshCount_);
        Logger::debugValue("GUI", "Flushes per min", flushCount_);
        refreshCount_ = flushCount_ = 0;
        countLogMs_ = lastUpdateMs_;
    }
    // SI5351 reconnected (possibly between two samples): connection dialogs are outdated.
    if (app_->connectionCount() != lastConnections_) {
        lastConnections_ = app_->connectionCount();
        if (app_->hardwareReady()) dismissLinkMessages_();
    }
    // Any change in the controller (also from the supervision and remote commands) or in what
    // the page shows of WiFi, heap and the REMOTE indicator: redraw (V-2, AF12). The setters
    // touch only widgets whose content changes.
    if (app_->revision() != lastRevision_ || statusView_() != lastView_) {
        refresh_();
    } else {
        refreshSaveState_(); // MAIN status bar and CONFIG header; no-op on the other pages
    }
}

// ---------------------------------------------------------------------------
// Events

void Gui::eventCb_(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_current_target(e);
    // The event user data carries the packed action, so the instance comes from the display.
    Gui* g = from_(obj);
    uintptr_t packed = (uintptr_t)lv_event_get_user_data(e);
    g->lastCode_ = lv_event_get_code(e);
    const uint32_t now = millis();
    if (g->lastCode_ == LV_EVENT_PRESSED) {
        g->pressStartMs_ = now;
        return;
    }
    if (g->lastCode_ == LV_EVENT_LONG_PRESSED_REPEAT &&
        !InputLogic::repeatDue(now - g->pressStartMs_, now - g->lastStepMs_)) {
        return; // slow phase of a hold: skip this repeat (U-20)
    }
    if (g->lastCode_ == LV_EVENT_LONG_PRESSED || g->lastCode_ == LV_EVENT_LONG_PRESSED_REPEAT) {
        g->lastStepMs_ = now;
    }
    g->handleAction_((Action)((packed >> 8) & 0xFF), (uint8_t)(packed & 0xFF), obj);
}

// The confirmation texts name these times.
static_assert(WifiPolicy::OPEN_AP_IDLE_OFF_MS == 15 * 60000, "ApOpen text says 15 min");
static_assert(NetChange::CONFIRM_MS == 120000, "WebNetwork/WebPending texts say 120 s");

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
    // Holding +/- speeds up after REPEAT_FASTER_MS (U-20).
    const int64_t steps = lastCode_ == LV_EVENT_LONG_PRESSED_REPEAT ? InputLogic::repeatSteps(millis() - pressStartMs_) : 1;
    switch (action) {
        case Action::Navigate:
            requestNavigate_((PageId)arg);
            return;
        case Action::ChannelSwitch:
            app.setChannelEnabled(arg, lv_obj_has_state(obj, LV_STATE_CHECKED));
            break;
        case Action::FreqUp:
            app.adjustChannelFrequency(channel_, steps * sys.delta_step);
            break;
        case Action::FreqDown:
            app.adjustChannelFrequency(channel_, -steps * (int64_t)sys.delta_step);
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
            app.adjustCorrection((int32_t)steps * CORRECTION_STEPS[corrStepIdx_]);
            break;
        case Action::CorrDown:
            app.adjustCorrection(-(int32_t)steps * CORRECTION_STEPS[corrStepIdx_]);
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
            app.setGlobalDriveStrength(DRIVE_MA_MIN + lv_dropdown_get_selected(obj) * DRIVE_MA_STEP);
            break;
        // The dropdown options are in enum order (checked above). refresh_() shows the resulting state, e.g.
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
                confirm_(Confirm::OffStateHigh); // the dropdown shows the old value until confirmed
                break;
            }
            app.setOffState(s);
            break;
        }
        case Action::Ch3MasterFreq:
            channel_ = app.ch3().master;
            requestNavigate_(PageId::Numpad);
            return;
        case Action::Ch3InfoTap:
            if (app.ch3Status() == Ch3Status::MasterOff) app.setChannelEnabled(app.ch3().master, true);
            break;
        case Action::LockScreen:
            lock_.lock();
            Logger::info("GUI", "Screen locked");
            requestNavigate_(PageId::Main);
            return;
        case Action::ResetDefaults:
            confirm_(Confirm::ResetDefaults);
            return;
        case Action::FactoryReset:
            confirm_(Confirm::FactoryReset);
            return;
        case Action::TouchCal:
            calFirstStart_ = false;
            requestNavigate_(PageId::TouchCal);
            return;
        case Action::NumpadKey:
            handleNumpadKey_(obj);
            return;
        case Action::WifiMode: {
            if (!services_.wifi) break;
            const WifiMode mode = (WifiMode)lv_dropdown_get_selected(obj);
            if (mode == services_.wifi->settings().mode) break;
            NetSettings s = services_.wifi->settings();
            s.mode = mode;
            wifiPages_.commitNet(s, WifiPages::NetApply::Restart);
            break;
        }
        case Action::RemoteOutputs:
            app.setRemoteOutputsAllowed(lv_dropdown_get_selected(obj) == 0);
            break;
        case Action::RemoteDisconnect:
            // Served by the next loop(); the info card shows the new counts then.
            if (services_.disconnectRemote) services_.disconnectRemote();
            Logger::info("GUI", "Reconnect all remote clients");
            break;
        case Action::ConfigTab:
            // Rebuilds CONFIG with the other tab (deferred like any navigation).
            if (arg < CONFIG_TAB_COUNT && arg != configTab_) {
                configTab_ = arg;
                requestNavigate_(PageId::Config);
            } else if (obj) {
                lv_obj_add_state(obj, LV_STATE_CHECKED); // a tap on the shown tab keeps it selected
            }
            return;
        case Action::WifiEdit:
        case Action::WifiDelete:
        case Action::WifiRescan:
        case Action::WifiScanPage:
        case Action::WifiPick:
        case Action::WifiOther:
        case Action::EntryShow:
        case Action::EntryChanged:
        case Action::EntryDone:
        case Action::ApRegenerate:
        case Action::ApSet:
        case Action::ApOpenToggle:
        case Action::DevName:
        case Action::DevIpMode:
        case Action::DevIpField:
        case Action::DevWebPassword:
            wifiPages_.handleAction(action, arg, obj);
            return;
    }
    refresh_();
}

// ---------------------------------------------------------------------------
// Message box: one at a time, further messages wait in a short queue.

void Gui::onAppEvent(const Event& e, void* gui) {
    // A remote client gets its error itself; the device shows no dialog for it.
    if (isRemote(e.source)) return;
    static_cast<Gui*>(gui)->showMessage(e.title, e.text, (uint8_t)e.category);
}

void Gui::showMessage(const char* title, const char* text, uint8_t tag) {
    switch (messages_.post(title, text, tag)) {
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
    lv_obj_set_style_bg_color(btns, Colors::warn(), partState(LV_PART_ITEMS, LV_STATE_CHECKED));
    lv_obj_set_style_text_color(btns, Colors::textDark(), partState(LV_PART_ITEMS, LV_STATE_CHECKED));
}

// Creates the box of the open message unless it exists (several async calls may be pending).
void Gui::openMessage_() {
    static const char* buttons[] = {"OK", ""};
    if (shownBox_ || !messages_.isOpen() || currentPage_ == PageId::TouchCal) return;
    msgbox_ = shownBox_ = lv_msgbox_create(nullptr, messages_.openTitle(), messages_.openText(), buttons, false);
    styleDialog(msgbox_, 1);
    lv_obj_center(msgbox_);
    lv_obj_add_event_cb(msgbox_, msgboxEventCb_, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(msgbox_, msgboxEventCb_, LV_EVENT_DELETE, nullptr);
}

void Gui::dismissLinkMessages_() {
    // Output errors from the restore stay.
    if (!messages_.dropTagged((uint8_t)EventCategory::Link)) return;
    if (msgbox_) {
        lv_msgbox_close(msgbox_); // the delete event moves on to the next message
    } else if (!shownBox_ && messages_.close()) {
        // The outdated message was still waiting for its box; the next one gets it.
        lv_async_call(nextMessageCb_, this);
    }
    // shownBox_ without msgbox_: already closing after OK, its delete event moves on.
}

void Gui::nextMessageCb_(void* gui) { static_cast<Gui*>(gui)->openMessage_(); }

void Gui::showModes_(lv_obj_t* label, uint8_t ch, const char* extra, const char* alone, const char* note) {
    const SystemSettings& sys = app_->system();
    char modes[64];
    const StatusText::Tone tone =
        StatusText::modeSummary(sys.tuning_mode, app_->spreadOf(ch), sys.drive_ma, sys.off_state, modes, sizeof(modes));
    if (!modes[0] && alone && extra[0]) extra = alone;
    char buf[160];
    snprintf(buf, sizeof(buf), "%s%s%s%s%s", modes, (modes[0] && extra[0]) ? " | " : "", extra,
             ((modes[0] || extra[0]) && note[0]) ? " | " : "", note);
    Ui::setText(label, buf);
    Ui::setTextColor(label, Colors::forTone(tone));
}

// Confirmation dialogs (AF49a): one row per Confirm value, in enum order. Button 0 cancels
// (Revert for a web change), button 1 is the action in the warning color.
static const char* const OFF_BUTTONS[] = {"Cancel", "High", ""};
static const char* const RESET_BUTTONS[] = {"Cancel", "Reset", ""};
static const char* const ERASE_BUTTONS[] = {"Cancel", "Erase", ""};
static const char* const DELETE_BUTTONS[] = {"Cancel", "Delete", ""};
static const char* const USE_BUTTONS[] = {"Cancel", "Use", ""};
static const char* const OPEN_BUTTONS[] = {"Cancel", "Open", ""};
static const char* const START_BUTTONS[] = {"Cancel", "Start", ""};
static const char* const KEEP_BUTTONS[] = {"Revert", "Keep", ""};
static const char* const REMOVE_BUTTONS[] = {"Cancel", "Remove", ""};

const Gui::ConfirmSpec Gui::CONFIRMS_[CONFIRM_COUNT] = {
    {"Off state High",
     "Switched-off outputs then carry DC (supply voltage, mostly 3.3 V). "
     "Do not use with DC-coupled mixers or inputs.",
     nullptr, OFF_BUTTONS, &Gui::answerOffStateHigh_, nullptr},
    {"Reset to defaults",
     "All settings and frequencies return to the factory values, all outputs switch off. "
     "Frequency correction and touch calibration are kept.",
     nullptr, RESET_BUTTONS, &Gui::answerResetDefaults_, nullptr},
    {"Factory reset",
     "Erases all stored settings including frequency correction and touch calibration, "
     "then restarts. The touch calibration runs again.",
     nullptr, ERASE_BUTTONS, &Gui::answerFactoryReset_, nullptr},
    // Payload: the network slot.
    {"Delete network", "Network 1 and its password are deleted.", "Network 2 and its password are deleted.",
     DELETE_BUTTONS, nullptr, &WifiPages::answerDeleteNetwork},
    {"Open network",
     "This network has no password: everything sent over it, including the web and SCPI "
     "traffic, can be read and changed by others nearby.",
     nullptr, USE_BUTTONS, nullptr, &WifiPages::answerOpenNetwork},
    {"Open access point",
     "Without a password everyone nearby can connect, read everything and operate the "
     "generator. It switches off after 15 min without a connected device.",
     nullptr, OPEN_BUTTONS, nullptr, &WifiPages::answerApOpen},
    {"Open fallback AP",
     "No stored network found. Start the access point without a password? Everyone "
     "nearby can then connect and operate the generator.",
     nullptr, START_BUTTONS, nullptr, &WifiPages::answerOpenFallback},
    {"Network change",
     "The network settings were changed from the web. Keep them? Without Keep here and "
     "Confirm in the browser, the previous settings return after 120 s.",
     nullptr, KEEP_BUTTONS, nullptr, &WifiPages::answerWebChange},
    {"Remove web password",
     "Everyone who can reach the device over the network can then operate it in the "
     "browser without logging in. Open web sessions end.",
     nullptr, REMOVE_BUTTONS, nullptr, &WifiPages::answerRemoveWebPassword},
    {"Web change pending",
     "Network settings were changed from the web and wait for Confirm in the browser "
     "(at most 120 s). Settings here can be changed after that. Revert restores the "
     "previous settings now.",
     nullptr, KEEP_BUTTONS, nullptr, &WifiPages::answerWebChange},
};

void Gui::answerOffStateHigh_(bool yes, uint8_t) {
    if (yes) app_->setOffState(OffState::High);
}

void Gui::answerResetDefaults_(bool yes, uint8_t) {
    if (!yes) return;
    app_->resetToDefaults();
    corrStepIdx_ = 1;
}

void Gui::answerFactoryReset_(bool yes, uint8_t) {
    if (!yes || !services_.factoryReset) return;
    app_->resetToDefaults(); // outputs off before the restart
    services_.factoryReset();         // does not return
}

void Gui::confirm_(Confirm what, uint8_t payload) {
    switch (confirms_.post((uint8_t)what, payload)) {
        case ConfirmQueue::Post::Open: showConfirm_(); break;
        case ConfirmQueue::Post::Full: Logger::error("GUI", "Confirmation queue full"); break;
        case ConfirmQueue::Post::Queued:
        case ConfirmQueue::Post::Duplicate: break;
    }
}

void Gui::dropConfirm_(Confirm what) {
    if (!confirms_.drop((uint8_t)what)) return;
    // It was the open one: its box goes without an action, the next one follows.
    if (confirmBox_) {
        lv_obj_t* box = confirmBox_;
        confirmBox_ = nullptr;
        lv_obj_clear_flag(lv_msgbox_get_btns(box), LV_OBJ_FLAG_CLICKABLE);
        lv_msgbox_close_async(box);
    }
    lv_async_call(nextConfirmCb_, this);
}

void Gui::nextConfirmCb_(void* gui) {
    Gui* g = static_cast<Gui*>(gui);
    g->confirms_.next();
    g->showConfirm_();
}

void Gui::showConfirm_() {
    if (confirmBox_ || !confirms_.isOpen() || currentPage_ == PageId::TouchCal) return;
    const ConfirmQueue::Entry& open = confirms_.open();
    const ConfirmSpec& spec = CONFIRMS_[open.kind < CONFIRM_COUNT ? open.kind : 0];
    const char* const text = open.payload != 0 && spec.text1 ? spec.text1 : spec.text;
    lv_obj_t* box = lv_msgbox_create(nullptr, spec.title, text, const_cast<const char**>(spec.buttons), false);
    styleDialog(box, 2);
    lv_btnmatrix_set_btn_ctrl(lv_msgbox_get_btns(box), 1, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_center(box);
    // The box answers only for the generation it was opened with (CQ-1).
    lv_obj_set_user_data(box, (void*)(uintptr_t)open.generation);
    lv_obj_add_event_cb(box, confirmEventCb_, LV_EVENT_VALUE_CHANGED, nullptr);
    confirmBox_ = box;
    refresh_();
}

void Gui::confirmEventCb_(lv_event_t* e) {
    lv_obj_t* box = lv_event_get_current_target(e);
    Gui* g = from_(box);
    // Button index, not label (CQ-15): 0 = Cancel / Revert, 1 = the action in the warning color.
    const uint16_t idx = lv_msgbox_get_active_btn(box);
    if (idx != 0 && idx != 1) return; // a press between the buttons
    lv_obj_clear_flag(lv_msgbox_get_btns(box), LV_OBJ_FLAG_CLICKABLE);
    lv_msgbox_close_async(box);
    if (g->confirmBox_ == box) g->confirmBox_ = nullptr;
    ConfirmQueue::Entry a {};
    if (!g->confirms_.answer((uint16_t)(uintptr_t)lv_obj_get_user_data(box), &a)) {
        g->refresh_(); // stale box: no action
        return;
    }
    if (a.kind < CONFIRM_COUNT) {
        const ConfirmSpec& spec = CONFIRMS_[a.kind];
        if (spec.answer) (g->*spec.answer)(idx == 1, a.payload);
        else (g->wifiPages_.*spec.wifiAnswer)(idx == 1, a.payload);
    }
    // A waiting question (e.g. the open fallback AP) follows outside this event.
    lv_async_call(nextConfirmCb_, g);
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
