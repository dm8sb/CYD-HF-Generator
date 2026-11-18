// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../gui.h"
#include "../gui_layout.h"
#include <utils/build_info.h>
#include <utils/frequency_utils.h>
#include <utils/ipv4.h>
#include <utils/sanitize.h>
#include <esp_heap_caps.h>

using namespace Layout;

// CONFIG is split into tabs instead of a scrolled list (resistive touch, user tests): the tab bar
// at the bottom switches between pages of at most 3 rows, the settings that change the signal in
// the first tab (audit U-14). A tab change rebuilds the page like any navigation.
static constexpr int ROW_H = 50;
static constexpr int ROW_PITCH = ROW_H + 4;
static constexpr int ROW_PAD = 4;
static constexpr int ROW_CONTENT_W = CONTENT_W - 2 * ROW_PAD - 2; // border 1
static constexpr int ROW_CONTENT_H = ROW_H - 2 * ROW_PAD - 2;
static constexpr uint8_t ROWS_PER_TAB = 3;
static constexpr uint8_t INFO_LINES = 8;
static constexpr int INFO_LINE_H = 15;                             // montserrat 12
static constexpr int INFO_H = INFO_LINES * INFO_LINE_H + 2 * ROW_PAD + 2;
static constexpr int VALUE_X = 92;                                 // values and dropdowns
static constexpr int BTN_W = 52;                                   // buttons 40 px high (U-18)
static constexpr int DROPDOWN_H = 34;
static constexpr int BTN_RIGHT_X = 236;                            // right button of a row
static constexpr int DROPDOWN_W = 140;
static constexpr int DROPDOWN_LINE_SPACE = 9;                      // opened list: 25 px per entry
static constexpr int DROPDOWN_LIST_PAD = 4;
static constexpr int SAVE_STATE_X = 164;                           // header, after the title
// Tab bar: 5 tabs of 60 px with 3 px gaps, 34 px high (touch targets on the resistive panel).
static constexpr int TAB_H = 36;
static constexpr int TAB_Y = SCREEN_HEIGHT - TAB_H;
static constexpr int TAB_W = 60;
static constexpr int TAB_GAP = 3;
static constexpr int TAB_X0 = (SCREEN_WIDTH - Gui::CONFIG_TAB_COUNT * TAB_W - (Gui::CONFIG_TAB_COUNT - 1) * TAB_GAP) / 2;
static_assert(CONTENT_Y + ROWS_PER_TAB * ROW_PITCH <= TAB_Y, "3 rows fit above the tab bar");
static_assert(CONTENT_Y + INFO_H + 4 <= TAB_Y, "the info card fits above the tab bar");
static_assert(TAB_X0 >= 0, "the tabs fit the screen width");

// Dropdown options. Gui::handleAction_ takes the selected index as the value: enum order for
// tuning, spread and off state (checked in gui.cpp), DRIVE_MA_MIN + index * DRIVE_MA_STEP for
// the drive strength.
constexpr const char* DRIVE_OPTIONS = "2 mA\n4 mA\n6 mA\n8 mA";
constexpr const char* OFFSTATE_OPTIONS = "Low\nHigh\nHigh-Z";
constexpr const char* TUNING_OPTIONS = "Fractional\nLow noise\nInteger";
constexpr const char* SPREAD_OPTIONS = "Off\n-0.5 %\n-1.5 %\n+/-0.5 %";
constexpr uint8_t optionCount(const char* s) { return *s == '\0' ? 1 : (uint8_t)((*s == '\n') + optionCount(s + 1)); }
static_assert(optionCount(DRIVE_OPTIONS) == DRIVE_OPTION_COUNT, "one option per drive strength");
static_assert(optionCount(OFFSTATE_OPTIONS) == (uint8_t)OffState::HighZ + 1, "one option per OffState");
static_assert(optionCount(TUNING_OPTIONS) == (uint8_t)TuningMode::Integer + 1, "one option per TuningMode");
static_assert(optionCount(SPREAD_OPTIONS) == (uint8_t)Spread::Center05 + 1, "one option per Spread");

static void setSelected(lv_obj_t* dd, uint16_t index) {
    if (lv_dropdown_get_selected(dd) != index) lv_dropdown_set_selected(dd, index);
}

lv_obj_t* Gui::addConfigRow_(lv_obj_t* parent, uint8_t index, int h, const char* title) {
    // Settings rows are slimmer cards than the default ones.
    lv_obj_t* row = addCard_(parent, MARGIN, CONTENT_Y + index * ROW_PITCH, CONTENT_W, h, Colors::config());
    lv_obj_set_style_pad_all(row, ROW_PAD, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    if (title) {
        lv_obj_t* l = addLabel_(row, title, &lv_font_montserrat_14, Colors::text(), 0, 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 2, 0);
    }
    return row;
}

lv_obj_t* Gui::addOptionRow_(lv_obj_t* parent, uint8_t index, const char* title, const char* options, Action action) {
    lv_obj_t* row = addConfigRow_(parent, index, ROW_H, title);
    lv_obj_t* dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_size(dd, DROPDOWN_W, DROPDOWN_H);
    lv_obj_align(dd, LV_ALIGN_LEFT_MID, VALUE_X, 0);
    // Grayed out while the option does not apply (the arrow inherits the color).
    lv_obj_set_style_text_color(dd, Colors::muted(), LV_STATE_DISABLED);
    // Denser than the theme (line space 16, pad 13): a 4-entry list needs 103 px and so opens
    // completely below the first row; the lists of the third row open upwards.
    lv_obj_t* opts = lv_dropdown_get_list(dd);
    lv_obj_set_style_text_line_space(opts, DROPDOWN_LINE_SPACE, 0);
    lv_obj_set_style_pad_ver(opts, DROPDOWN_LIST_PAD, 0);
    on_(dd, LV_EVENT_VALUE_CHANGED, action, 0);
    return dd;
}

void Gui::createConfig_() {
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    lv_obj_t* bar = createHeader_("CONFIG", Colors::config(), 0);
    // Autosave state in the header, visible in every tab.
    w_.saveState = addLabel_(bar, "", &lv_font_montserrat_14, Colors::muted(), 0, 0);
    lv_obj_align(w_.saveState, LV_ALIGN_LEFT_MID, SAVE_STATE_X, 0);

    // Tab bar; the selected tab is filled in the CONFIG color.
    static const char* const TABS[CONFIG_TAB_COUNT] = {"Signal", "Freq", "Net", "Device", "Info"};
    if (configTab_ >= CONFIG_TAB_COUNT) configTab_ = 0;
    for (uint8_t i = 0; i < CONFIG_TAB_COUNT; ++i) {
        lv_obj_t* t = addChip_(root_, TABS[i], TAB_X0 + i * (TAB_W + TAB_GAP), TAB_Y, TAB_W, TAB_H - 2, Colors::config(),
                               Action::ConfigTab, i);
        if (i == configTab_) lv_obj_add_state(t, LV_STATE_CHECKED);
    }

    lv_obj_t* row;
    switch (configTab_) {
        case 0: // Signal; the dropdown entries are in the order of the enum values.
            w_.driveDd = addOptionRow_(root_, 0, "Drive all", DRIVE_OPTIONS, Action::Drive);
            w_.offStateDd = addOptionRow_(root_, 1, "Off state", OFFSTATE_OPTIONS, Action::OffState);
            w_.tuningDd = addOptionRow_(root_, 2, "Tuning", TUNING_OPTIONS, Action::Tuning);
            break;
        case 1: // Freq: spread spectrum and the crystal correction.
            w_.spreadDd = addOptionRow_(root_, 0, "Spread CH1", SPREAD_OPTIONS, Action::Spread);
            // Frequency correction in ppm; the step chip cycles 0.01 / 0.1 / 1 ppm.
            row = addConfigRow_(root_, 1, ROW_H, "Corr.");
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
            break;
        case 2: // Net: WiFi mode (W-E1, default Off), remote switch-on (W-E25, only here) and the
                // remote clients (S55b; they connect again by themselves).
            w_.wifiDd = addOptionRow_(root_, 0, "WiFi", "Off\nClient\nAccess point", Action::WifiMode);
            addButton_(lv_obj_get_parent(w_.wifiDd), "Setup", BTN_RIGHT_X, 0, BTN_W, ROW_CONTENT_H, btnColor,
                       Action::Navigate, (uint8_t)PageId::Wifi);
            w_.remoteDd = addOptionRow_(root_, 1, "Remote out.", "allowed\nlocal only", Action::RemoteOutputs);
            row = addConfigRow_(root_, 2, ROW_H, "Clients");
            addButton_(row, "Reconnect all", VALUE_X, 0, DROPDOWN_W + 56, ROW_CONTENT_H, btnColor,
                       Action::RemoteDisconnect);
            break;
        case 3: // Device: backlight, screen (lock, touch calibration U-26) and reset (U-13; both ask first).
            row = addConfigRow_(root_, 0, ROW_H, "Backlight");
            addButton_(row, LV_SYMBOL_MINUS, VALUE_X, 0, BTN_W, ROW_CONTENT_H, btnColor, Action::BacklightDown);
            w_.blValue = addLabel_(row, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
            lv_obj_set_width(w_.blValue, BTN_RIGHT_X - (VALUE_X + BTN_W));
            lv_obj_set_style_text_align(w_.blValue, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(w_.blValue, LV_ALIGN_LEFT_MID, VALUE_X + BTN_W, 0);
            addButton_(row, LV_SYMBOL_PLUS, BTN_RIGHT_X, 0, BTN_W, ROW_CONTENT_H, btnColor, Action::BacklightUp);
            row = addConfigRow_(root_, 1, ROW_H, "Screen");
            addButton_(row, "Lock", VALUE_X, 0, 96, ROW_CONTENT_H, btnColor, Action::LockScreen);
            addButton_(row, "Touch cal.", VALUE_X + 100, 0, 96, ROW_CONTENT_H, btnColor, Action::TouchCal);
            row = addConfigRow_(root_, 2, ROW_H, "Reset");
            addButton_(row, "Defaults", VALUE_X, 0, 96, ROW_CONTENT_H, btnColor, Action::ResetDefaults);
            addButton_(row, "Factory", VALUE_X + 100, 0, 96, ROW_CONTENT_H, Colors::error(), Action::FactoryReset);
            break;
        default: { // Info: firmware, build, display, touch, SI5351 link, WiFi, remote clients, heap.
            row = addConfigRow_(root_, 0, INFO_H, nullptr);
            lv_obj_t* info[INFO_LINES];
            for (uint8_t i = 0; i < INFO_LINES; ++i) {
                info[i] = addLabel_(row, "", &lv_font_montserrat_12, Colors::muted(), 2, i * INFO_LINE_H);
                // One line each; a long branch name ends in "..." instead of leaving the card.
                lv_obj_set_size(info[i], ROW_CONTENT_W - 2, INFO_LINE_H);
                lv_label_set_long_mode(info[i], LV_LABEL_LONG_DOT);
            }
            char buf[96];
            snprintf(buf, sizeof(buf), "Firmware %s | %s %s", BuildInfo::version(), BuildInfo::branch(),
                     BuildInfo::commit());
            Ui::setText(info[0], buf);
            snprintf(buf, sizeof(buf), "Build %s", BuildInfo::time());
            Ui::setText(info[1], buf);
            snprintf(buf, sizeof(buf), "Display %s (%s)", DisplayDetect::name(display_->panel()),
                     DisplayDetect::sourceName(display_->source()));
            Ui::setText(info[2], buf);
            w_.touchInfo = info[3];
            w_.si5351Info = info[4];
            w_.wifiInfo = info[5];
            w_.remoteInfo = info[6];
            w_.heapInfo = info[7];
            break;
        }
    }
}

void Gui::refreshConfig_() {
    const AppController& app = *app_;
    const SystemSettings& sys = app.system();
    char buf[96];

    refreshSaveState_();
    // Only the widgets of the shown tab exist (the others are null).
    if (w_.blValue) {
        snprintf(buf, sizeof(buf), "%u %%", sys.backlight);
        Ui::setText(w_.blValue, buf);
    }
    if (w_.corrValue) {
        FrequencyUtils::formatPpm(sys.freq_correction, buf, sizeof(buf));
        Ui::setText(w_.corrValue, buf);
        static const char* corrStepLabels[CORRECTION_STEP_COUNT] = {"0.01", "0.1", "1"};
        Ui::setText(lv_obj_get_child(w_.corrStep, 0), corrStepLabels[corrStepIdx_]);
    }
    if (w_.driveDd) setSelected(w_.driveDd, (sys.drive_ma - DRIVE_MA_MIN) / DRIVE_MA_STEP);
    if (w_.tuningDd) setSelected(w_.tuningDd, (uint16_t)sys.tuning_mode);
    if (w_.spreadDd) {
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
    }
    if (w_.offStateDd) setSelected(w_.offStateDd, (uint16_t)sys.off_state);
    if (w_.wifiDd) setSelected(w_.wifiDd, services_.wifi ? (uint16_t)services_.wifi->settings().mode : 0);
    if (w_.remoteDd) setSelected(w_.remoteDd, app.remoteOutputsAllowed() ? 0 : 1);
    if (!w_.touchInfo) return; // the info lines exist on the Info tab only

    const TouchCalibration& tc = touch_->calibration();
    if (touch_->calibrated()) {
        snprintf(buf, sizeof(buf), "Touch calibrated x %ld-%ld y %ld-%ld", (long)tc.x_min, (long)tc.x_max,
                 (long)tc.y_min, (long)tc.y_max);
    } else {
        snprintf(buf, sizeof(buf), "Touch not calibrated (default)");
    }
    Ui::setText(w_.touchInfo, buf);

    if (app.hardwareReady()) {
        snprintf(buf, sizeof(buf), "SI5351 SDA %d SCL %d%s%s", app.i2cSdaPin(), app.i2cSclPin(),
                 app.i2cSwapped() ? " (swapped)" : "", app.si5351IdUnverified() ? " (clone?)" : "");
        Ui::setText(w_.si5351Info, buf);
        Ui::setTextColor(w_.si5351Info, Colors::muted());
    } else {
        Ui::setText(w_.si5351Info, "SI5351 not found");
        Ui::setTextColor(w_.si5351Info, Colors::error());
    }

    // WiFi line; a foreign SSID only through sanitize (W-S9).
    lv_color_t wifiColor = Colors::muted();
    if (!services_.wifi || services_.wifi->status().mode == WifiMode::Off) {
        snprintf(buf, sizeof(buf), "WiFi off");
    } else {
        const WifiManager::Status& w = services_.wifi->status();
        char ip[16];
        if (w.apActive) {
            Ipv4::format(w.apIp, ip, sizeof(ip));
            snprintf(buf, sizeof(buf), "%s %s %s, %u connected", w.fallback ? "Fallback AP" : "AP", w.apSsid, ip,
                     (unsigned)w.apStations);
            if (w.apOpen) wifiColor = Colors::warn();
        } else if (w.staConnected) {
            char ssid[33];
            Sanitize::text(w.ssid, strlen(w.ssid), ssid, sizeof(ssid));
            Ipv4::format(w.staIp, ip, sizeof(ip));
            rssiShown_ = StatusViewRules::rssiShown(rssiShown_, w.rssi);
            snprintf(buf, sizeof(buf), "WiFi %s %s %d dBm", ssid, ip, (int)rssiShown_);
        } else if (w.needsOpenConfirmation) {
            snprintf(buf, sizeof(buf), "WiFi searching; open fallback AP needs confirmation");
            wifiColor = Colors::warn();
        } else {
            snprintf(buf, sizeof(buf), "WiFi searching (%s)", services_.wifi->settings().deviceName);
        }
    }
    Ui::setText(w_.wifiInfo, buf);
    Ui::setTextColor(w_.wifiInfo, wifiColor);
    // Remote clients: open web pages (WebSockets) and the SCPI TCP client (W-S20).
    const unsigned web = services_.webClients ? services_.webClients() : 0;
    const uint32_t scpi = services_.scpiClient ? services_.scpiClient() : 0;
    char scpiText[16] = "-";
    if (scpi) Ipv4::format(scpi, scpiText, sizeof(scpiText));
    snprintf(buf, sizeof(buf), "Remote: web %u, SCPI %s", web, scpiText);
    Ui::setText(w_.remoteInfo, buf);
    // Heap (V-8): free, minimum since boot, largest block; LVGL pool free. Sampled every 5 s
    // (AF12: the walks run under the heap lock).
    if (!heapSampled_) sampleHeap_();
    snprintf(buf, sizeof(buf), "Heap %u/%u KB, block %u KB, LVGL %u KB", (unsigned)(heapFree_ / 1024),
             (unsigned)(heapMin_ / 1024), (unsigned)(heapBlock_ / 1024), (unsigned)(lvglFree_ / 1024));
    Ui::setText(w_.heapInfo, buf);
}

void Gui::sampleHeap_() {
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    heapFree_ = ESP.getFreeHeap();
    heapMin_ = ESP.getMinFreeHeap();
    heapBlock_ = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    lvglFree_ = mon.free_size;
    heapSampleMs_ = millis();
    heapSampled_ = true;
}

void Gui::refreshSaveState_() {
    if (!w_.saveState) return;
    // Only errors are shown: "Saved" / "Saving..." irritated users (user tests); changes are
    // saved a few seconds after the last one (manual section 9).
    const char* text = "";
    lv_color_t color = Colors::muted();
    switch (app_->saveState()) {
        case SaveState::Saved: break;
        case SaveState::Pending: break;
        case SaveState::Failed: text = "Save failed"; color = Colors::error(); break;
        case SaveState::Unavailable: text = "Not saved"; color = Colors::error(); break;
    }
    Ui::setText(w_.saveState, text);
    Ui::setTextColor(w_.saveState, color);
}
