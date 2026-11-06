#include "../gui.h"
#include "../gui_layout.h"
#include <utils/build_info.h>
#include <utils/frequency_utils.h>
#include <utils/sanitize.h>
#include <esp_heap_caps.h>

using namespace Layout;

// CONFIG is the only page that scrolls: below the fixed header the rows sit in a list that
// snaps to row tops, the settings that change the signal first (audit U-14). 3 rows and 36 px
// of the next one and the 6 px scrollbar show that the list continues (U-24).
static constexpr int ROW_H = 50;
static constexpr int ROW_PITCH = ROW_H + 4;
static constexpr int ROW_PAD = 4;
static constexpr int ROW_CONTENT_W = CONTENT_W - 2 * ROW_PAD - 2; // border 1
static constexpr int ROW_CONTENT_H = ROW_H - 2 * ROW_PAD - 2;
static constexpr uint8_t ROW_COUNT = 11;                           // setting rows above the info card
static constexpr uint8_t INFO_LINES = 7;
static constexpr int INFO_LINE_H = 15;                             // montserrat 12
static constexpr int INFO_H = INFO_LINES * INFO_LINE_H + 2 * ROW_PAD + 2;
static constexpr int VALUE_X = 92;                                 // values and dropdowns
static constexpr int BTN_W = 52;                                   // buttons 40 px high (U-18)
static constexpr int DROPDOWN_H = 34;
static constexpr int BTN_RIGHT_X = 236;                            // right button of a row
static constexpr int DROPDOWN_W = 140;
static constexpr int DROPDOWN_LINE_SPACE = 9;                      // opened list: 25 px per entry
static constexpr int DROPDOWN_LIST_PAD = 4;
static constexpr int LIST_PAD = 4;                                 // above the first and below the last card
static constexpr int LIST_HEIGHT = SCREEN_HEIGHT - HEADER_H;
// Largest scroll position: the info card at the bottom.
static constexpr int SCROLL_MAX = LIST_PAD + ROW_COUNT * ROW_PITCH + INFO_H + LIST_PAD - LIST_HEIGHT;
static constexpr int SAVE_STATE_X = 116;                           // header, between title and MAIN
static_assert(LIST_PAD == CONTENT_Y - HEADER_H, "first row on the content grid of the other pages");
static_assert(SCROLL_MAX > 0, "the rows fit on one screen");

// LVGL keeps the touched button pressed while the finger scrolls the list (no click follows);
// show it released as soon as the list moves.
static void scrollBeginCb(lv_event_t* e) {
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev || lv_indev_get_scroll_obj(indev) != lv_event_get_current_target(e)) return;
    lv_obj_t* pressed = lv_indev_get_obj_act();
    if (pressed) lv_obj_clear_state(pressed, LV_STATE_PRESSED);
}

// LVGL scrolls a tapped button or dropdown into view, and with snapping it moves the row to
// the top on every tap. The rows only move by swiping.
static void noScrollOnFocus(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    const uint32_t n = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < n; ++i) noScrollOnFocus(lv_obj_get_child(obj, i));
}

static void setSelected(lv_obj_t* dd, uint16_t index) {
    if (lv_dropdown_get_selected(dd) != index) lv_dropdown_set_selected(dd, index);
}

lv_obj_t* Gui::addConfigRow_(lv_obj_t* list, uint8_t index, int h, const char* title) {
    const int y = index * ROW_PITCH;
    // Settings rows are slimmer cards than the default ones.
    lv_obj_t* row = addCard_(list, MARGIN, y, CONTENT_W, h, Colors::config());
    lv_obj_set_style_pad_all(row, ROW_PAD, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    // A row that cannot reach the top would make the snap scroll past the end (end stop below).
    if (y > SCROLL_MAX) lv_obj_clear_flag(row, LV_OBJ_FLAG_SNAPPABLE);
    if (title) {
        lv_obj_t* l = addLabel_(row, title, &lv_font_montserrat_14, Colors::text(), 0, 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 2, 0);
    }
    return row;
}

lv_obj_t* Gui::addOptionRow_(lv_obj_t* list, uint8_t index, const char* title, const char* options, Action action) {
    lv_obj_t* row = addConfigRow_(list, index, ROW_H, title);
    lv_obj_t* dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_size(dd, DROPDOWN_W, DROPDOWN_H);
    lv_obj_align(dd, LV_ALIGN_LEFT_MID, VALUE_X, 0);
    // Grayed out while the option does not apply (the arrow inherits the color).
    lv_obj_set_style_text_color(dd, Colors::muted(), LV_STATE_DISABLED);
    // Denser than the theme (line space 16, pad 13): a 4-entry list needs 103 px and so opens
    // completely in every snap position (the second visible row has 104 px above its dropdown).
    lv_obj_t* opts = lv_dropdown_get_list(dd);
    lv_obj_set_style_text_line_space(opts, DROPDOWN_LINE_SPACE, 0);
    lv_obj_set_style_pad_ver(opts, DROPDOWN_LIST_PAD, 0);
    on_(dd, LV_EVENT_VALUE_CHANGED, action, 0);
    return dd;
}

void Gui::createConfig_() {
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    lv_obj_t* bar = createHeader_("CONFIG", Colors::config(), 0);
    // Autosave state in the header, visible in every scroll position.
    w_.saveState = addLabel_(bar, "", &lv_font_montserrat_14, Colors::muted(), 0, 0);
    lv_obj_align(w_.saveState, LV_ALIGN_LEFT_MID, SAVE_STATE_X, 0);

    lv_obj_t* list = lv_obj_create(root_);
    w_.cfgList = list;
    lv_obj_set_pos(list, 0, HEADER_H);
    lv_obj_set_size(list, SCREEN_WIDTH, LIST_HEIGHT);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_ver(list, LIST_PAD, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(list, LV_SCROLL_SNAP_START);
    // Momentum stays; no elastic overscroll at the ends.
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    // Always visible (the list continues), 6 px in the right margin next to the cards, 5.7:1
    // against the background (U-24).
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_width(list, 6, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(list, 1, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_ver(list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list, Colors::outline(), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(list, scrollBeginCb, LV_EVENT_SCROLL_BEGIN, nullptr);
    lv_obj_t* row;

    // Options that change the signal first; the dropdown entries are in the order of the enum values.
    w_.driveDd = addOptionRow_(list, 0, "Drive all", "2 mA\n4 mA\n6 mA\n8 mA", Action::Drive);
    w_.offStateDd = addOptionRow_(list, 1, "Off state", "Low\nHigh\nHigh-Z", Action::OffState);
    w_.tuningDd = addOptionRow_(list, 2, "Tuning", "Fractional\nLow noise\nInteger", Action::Tuning);
    w_.spreadDd = addOptionRow_(list, 3, "Spread CH1", "Off\n-0.5 %\n-1.5 %\n+/-0.5 %", Action::Spread);

    // Frequency correction in ppm; the step chip cycles 0.01 / 0.1 / 1 ppm.
    row = addConfigRow_(list, 4, ROW_H, "Corr.");
    // 14 px: with the frame the widest value ("+48.44 ppm", 96 px) ends 8 px before the chip.
    w_.corrValue = addLabel_(row, "", &lv_font_montserrat_14, Colors::text(), 0, 0);
    lv_obj_align(w_.corrValue, LV_ALIGN_LEFT_MID, 40, 0);
    // Tap the value to type it on the numpad (U-11); the frame shows that it can be tapped.
    lv_obj_add_flag(w_.corrValue, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(w_.corrValue, 1, 0);
    lv_obj_set_style_border_color(w_.corrValue, Colors::outline(), 0);
    lv_obj_set_style_radius(w_.corrValue, 4, 0);
    lv_obj_set_style_pad_hor(w_.corrValue, 3, 0);
    lv_obj_set_style_pad_ver(w_.corrValue, 4, 0);
    lv_obj_set_ext_click_area(w_.corrValue, 4);
    on_(w_.corrValue, LV_EVENT_CLICKED, Action::Navigate, (uint8_t)PageId::Numpad);
    w_.corrStep = addChip_(row, "", 144, 0, 42, ROW_CONTENT_H, btnColor, Action::CorrStepCycle, 0);
    addButton_(row, LV_SYMBOL_MINUS, 190, 0, 47, ROW_CONTENT_H, btnColor, Action::CorrDown, 0, true);
    addButton_(row, LV_SYMBOL_PLUS, 241, 0, 47, ROW_CONTENT_H, btnColor, Action::CorrUp, 0, true);

    // Backlight in BACKLIGHT_STEP_PERCENT steps, value centered between the buttons
    row = addConfigRow_(list, 5, ROW_H, "Backlight");
    addButton_(row, LV_SYMBOL_MINUS, VALUE_X, 0, BTN_W, ROW_CONTENT_H, btnColor, Action::BacklightDown);
    w_.blValue = addLabel_(row, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
    lv_obj_set_width(w_.blValue, BTN_RIGHT_X - (VALUE_X + BTN_W));
    lv_obj_set_style_text_align(w_.blValue, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(w_.blValue, LV_ALIGN_LEFT_MID, VALUE_X + BTN_W, 0);
    addButton_(row, LV_SYMBOL_PLUS, BTN_RIGHT_X, 0, BTN_W, ROW_CONTENT_H, btnColor, Action::BacklightUp);


    // WiFi mode (W-E1, default Off) and remote switch-on (W-E25, only here).
    w_.wifiDd = addOptionRow_(list, 6, "WiFi", "Off\nClient\nAccess point", Action::WifiMode);
    w_.remoteDd = addOptionRow_(list, 7, "Remote out.", "allowed\nlocal only", Action::RemoteOutputs);

    // Touch calibration (U-26): the wizard with crosses near the corners.
    row = addConfigRow_(list, 8, ROW_H, "Touch");
    addButton_(row, "Calibrate", VALUE_X, 0, DROPDOWN_W, ROW_CONTENT_H, btnColor, Action::TouchCal);

    // Screen lock and reset for the next group (U-13); both resets ask first.
    row = addConfigRow_(list, 9, ROW_H, "Screen");
    addButton_(row, "Lock", VALUE_X, 0, DROPDOWN_W, ROW_CONTENT_H, btnColor, Action::LockScreen);
    row = addConfigRow_(list, 10, ROW_H, "Reset");
    addButton_(row, "Defaults", VALUE_X, 0, 96, ROW_CONTENT_H, btnColor, Action::ResetDefaults);
    addButton_(row, "Factory", VALUE_X + 100, 0, 96, ROW_CONTENT_H, Colors::error(), Action::FactoryReset);

    // Info card: firmware, build, display controller, touch, SI5351 link, WiFi, heap (refreshConfig_).
    row = addConfigRow_(list, ROW_COUNT, INFO_H, nullptr);
    lv_obj_t* info[INFO_LINES];
    for (uint8_t i = 0; i < INFO_LINES; ++i) {
        info[i] = addLabel_(row, "", &lv_font_montserrat_12, Colors::muted(), 2, i * INFO_LINE_H);
        // One line each; a long branch name ends in "..." instead of leaving the card.
        lv_obj_set_size(info[i], ROW_CONTENT_W - 2, INFO_LINE_H);
        lv_label_set_long_mode(info[i], LV_LABEL_LONG_DOT);
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "Firmware %s | %s %s", BuildInfo::version(), BuildInfo::branch(), BuildInfo::commit());
    lv_label_set_text(info[0], buf);
    snprintf(buf, sizeof(buf), "Build %s", BuildInfo::time());
    lv_label_set_text(info[1], buf);
    snprintf(buf, sizeof(buf), "Display %s (%s)", DisplayDetect::name(display_->panel()),
             DisplayDetect::sourceName(display_->source()));
    lv_label_set_text(info[2], buf);
    w_.touchInfo = info[3];
    w_.si5351Info = info[4];
    w_.wifiInfo = info[5];
    w_.heapInfo = info[6];

    // Snap point for the end of the list: the rows below SCROLL_MAX are not snappable, this
    // invisible stop shows the info card completely.
    lv_obj_t* endStop = lv_obj_create(list);
    lv_obj_remove_style_all(endStop);
    lv_obj_clear_flag(endStop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(endStop, 0, SCROLL_MAX);
    lv_obj_set_size(endStop, 1, 1);

    noScrollOnFocus(list);
    lv_obj_update_layout(list);
    lv_obj_scroll_to_y(list, configScrollY_, LV_ANIM_OFF);
}

void Gui::refreshConfig_() {
    const AppController& app = *app_;
    const SystemSettings& sys = app.system();
    char buf[96];

    snprintf(buf, sizeof(buf), "%u %%", sys.backlight);
    lv_label_set_text(w_.blValue, buf);

    FrequencyUtils::formatPpm(sys.freq_correction, buf, sizeof(buf));
    lv_label_set_text(w_.corrValue, buf);
    static const char* corrStepLabels[CORRECTION_STEP_COUNT] = {"0.01", "0.1", "1"};
    lv_label_set_text(lv_obj_get_child(w_.corrStep, 0), corrStepLabels[corrStepIdx_]);

    setSelected(w_.driveDd, sys.drive_ma / 2 - 1);
    setSelected(w_.tuningDd, (uint16_t)sys.tuning_mode);
    // Spread spectrum needs a fractional PLL A; the controller keeps it Off in Integer mode.
    setSelected(w_.spreadDd, (uint16_t)sys.spread);
    // Locked with a reason instead of a silently grayed "Off" (U-18).
    if (sys.tuning_mode == TuningMode::Integer) {
        lv_obj_add_state(w_.spreadDd, LV_STATE_DISABLED);
        lv_dropdown_set_text(w_.spreadDd, "Off (Integer)");
    } else {
        lv_obj_clear_state(w_.spreadDd, LV_STATE_DISABLED);
        lv_dropdown_set_text(w_.spreadDd, nullptr);
    }
    setSelected(w_.offStateDd, (uint16_t)sys.off_state);
    setSelected(w_.wifiDd, wifi_ ? (uint16_t)wifi_->settings().mode : 0);
    setSelected(w_.remoteDd, app.remoteOutputsAllowed() ? 0 : 1);

    const TouchCalibration& tc = touch_->calibration();
    if (touch_->calibrated()) {
        snprintf(buf, sizeof(buf), "Touch calibrated x %ld-%ld y %ld-%ld", (long)tc.x_min, (long)tc.x_max,
                 (long)tc.y_min, (long)tc.y_max);
    } else {
        snprintf(buf, sizeof(buf), "Touch not calibrated (default)");
    }
    lv_label_set_text(w_.touchInfo, buf);

    if (app.hardwareReady()) {
        snprintf(buf, sizeof(buf), "SI5351 SDA %d SCL %d%s%s", app.i2cSdaPin(), app.i2cSclPin(),
                 app.i2cSwapped() ? " (swapped)" : "", app.si5351IdUnverified() ? " (clone?)" : "");
        lv_label_set_text(w_.si5351Info, buf);
        lv_obj_set_style_text_color(w_.si5351Info, Colors::muted(), 0);
    } else {
        lv_label_set_text(w_.si5351Info, "SI5351 not found");
        lv_obj_set_style_text_color(w_.si5351Info, Colors::error(), 0);
    }

    // WiFi line; a foreign SSID only through sanitize (W-S9).
    lv_color_t wifiColor = Colors::muted();
    if (!wifi_ || wifi_->status().mode == WifiMode::Off) {
        snprintf(buf, sizeof(buf), "WiFi off");
    } else {
        const WifiManager::Status& w = wifi_->status();
        char ip[16];
        if (w.apActive) {
            NetConfig::formatIpv4(w.apIp, ip, sizeof(ip));
            snprintf(buf, sizeof(buf), "%s %s %s, %u connected", w.fallback ? "Fallback AP" : "AP", w.apSsid, ip,
                     (unsigned)w.apStations);
            if (w.apOpen) wifiColor = Colors::warn();
        } else if (w.staConnected) {
            char ssid[33];
            Sanitize::text(w.ssid, strlen(w.ssid), ssid, sizeof(ssid));
            NetConfig::formatIpv4(w.staIp, ip, sizeof(ip));
            snprintf(buf, sizeof(buf), "WiFi %s %s %d dBm", ssid, ip, (int)w.rssi);
        } else if (w.needsOpenConfirmation) {
            snprintf(buf, sizeof(buf), "WiFi searching; open fallback AP needs confirmation");
            wifiColor = Colors::warn();
        } else {
            snprintf(buf, sizeof(buf), "WiFi searching (%s)", wifi_->settings().deviceName);
        }
    }
    lv_label_set_text(w_.wifiInfo, buf);
    lv_obj_set_style_text_color(w_.wifiInfo, wifiColor, 0);
    // Heap (V-8): free, minimum since boot, largest block; LVGL pool free.
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    snprintf(buf, sizeof(buf), "Heap %u/%u KB, block %u KB, LVGL %u KB", (unsigned)(ESP.getFreeHeap() / 1024),
             (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024),
             (unsigned)(mon.free_size / 1024));
    lv_label_set_text(w_.heapInfo, buf);

    refreshSaveState_();
}

void Gui::refreshSaveState_() {
    if (!w_.saveState) return;
    const char* text = "Saved";
    lv_color_t color = Colors::muted();
    switch (app_->saveState()) {
        case SaveState::Saved: break;
        case SaveState::Pending: text = "Saving..."; break;
        case SaveState::Failed: text = "Save failed"; color = Colors::error(); break;
        case SaveState::Unavailable: text = "Not saved"; color = Colors::error(); break;
    }
    lv_label_set_text(w_.saveState, text);
    lv_obj_set_style_text_color(w_.saveState, color, 0);
}
