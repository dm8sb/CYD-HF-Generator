// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../gui.h"
#include "../gui_layout.h"
#include <utils/frequency_utils.h>
#include <math.h>

using namespace Layout;

// Three cards above the status bar (A-503); the bar replaces the navigation buttons CH1-CH3,
// the cards open the channel pages.
static constexpr int CARD_H = 62;
static constexpr int CARD_Y = 4;
static constexpr int BAR_H = 38;
static constexpr int BAR_Y = SCREEN_HEIGHT - BAR_H;
static_assert(CARD_Y + 3 * CARD_H + 3 * 4 <= BAR_Y, "cards above the status bar");
// Second row: state (14 px, filled, audit U-8) and detail line (14 px). The detail starts after
// the widest usual state ("WAIT", 43 px with padding) and moves right only for "! NO HW".
static constexpr int ROW2_Y = 27;
static constexpr int DETAIL_X = 50;
// Switch column at the right edge of the card content (288 px): a 44 px switch with 6 px extra
// touch area starts at 238, 13 px after the longest frequency ("100.000 000 MHz" ends at 225),
// so a tap on the unit cannot switch the output (audit U-3). A line marks the column.
static constexpr int SWITCH_W = 44;
static constexpr int SWITCH_H = 24;
static constexpr int DIVIDER_X = 231;
// Frequencies are right-aligned before the divider, so point and unit line up on all cards and
// a "~" (Integer raster) has room before two-digit MHz values. The label starts after the name.
static constexpr int FREQ_RIGHT = DIVIDER_X - 6;
static constexpr int NAME_W = 42;
// Status bar from the left, widths measured from the font data: save state 14 px ("Save
// failed" 76 px, ends at 86), WiFi 12 px ("AP open" 52 px, ends at 144), LOCKED 14 px or
// REMOTE <source> 12 px ("REMOTE WEB" 87 px + 4 px padding, ends at 241), CONFIG from 250
// (66 px, text 57 px): at least 6 px between neighbors, 9 px before CONFIG.
static constexpr int WIFI_X = 92;
static constexpr int STATE_X = 150;
static constexpr int CONFIG_W = 66;
static constexpr int CONFIG_X = SCREEN_WIDTH - 4 - CONFIG_W;
static_assert(STATE_X + 92 + 8 <= CONFIG_X, "REMOTE WEB keeps 8 px before CONFIG");

void Gui::createMain_() {
    // Main page intentionally has no project-title header to maximize status space.
    static const PageId pages[3] = {PageId::CH1, PageId::CH2, PageId::CH3};
    static const char* names[3] = {"CH1", "CH2", "CH3"};

    for (uint8_t i = 0; i < 3; ++i) {
        const uint8_t ch = i + 1;
        const lv_color_t color = Colors::forChannel(ch);
        lv_obj_t* card = addCard_(root_, MARGIN, CARD_Y + i * (CARD_H + 4), CONTENT_W, CARD_H, color);
        // Tapping the card opens the channel page; the switch toggles the output directly.
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(card, Colors::cardPressed(), LV_STATE_PRESSED);
        on_(card, LV_EVENT_CLICKED, Action::Navigate, (uint8_t)pages[i]);

        addLabel_(card, names[i], &lv_font_montserrat_20, color, 0, 0);
        w_.cardState[i] = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), 0, ROW2_Y);
        styleState_(w_.cardState[i]);
        w_.cardFreq[i] = addLabel_(card, "", &lv_font_montserrat_20, Colors::text(), NAME_W, 0);
        lv_obj_set_width(w_.cardFreq[i], FREQ_RIGHT - NAME_W);
        lv_obj_set_style_text_align(w_.cardFreq[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(w_.cardFreq[i], LV_LABEL_LONG_CLIP);
        w_.cardDetail[i] = addLabel_(card, "", &lv_font_montserrat_14, Colors::muted(), DETAIL_X, ROW2_Y);
        // Keep the detail line clear of the switch column.
        lv_obj_set_width(w_.cardDetail[i], DIVIDER_X - 4 - DETAIL_X);
        lv_label_set_long_mode(w_.cardDetail[i], LV_LABEL_LONG_DOT);
        lv_obj_t* divider = lv_obj_create(card);
        lv_obj_remove_style_all(divider);
        lv_obj_set_style_bg_color(divider, Colors::chipOff(), 0);
        lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
        lv_obj_set_size(divider, 1, CARD_H - 2 * (CARD_PAD + CARD_BORDER));
        lv_obj_set_pos(divider, DIVIDER_X, 0);
        lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
        w_.cardSwitch[i] = addSwitch_(card, ch, color);
        lv_obj_set_size(w_.cardSwitch[i], SWITCH_W, SWITCH_H);
        lv_obj_align(w_.cardSwitch[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Status bar like the header: save state, WiFi and REMOTE/LOCKED on the left, CONFIG at
    // the right edge (A-503).
    lv_obj_t* bar = lv_obj_create(root_);
    lv_obj_set_pos(bar, 0, BAR_Y);
    lv_obj_set_size(bar, SCREEN_WIDTH, BAR_H);
    lv_obj_set_style_bg_color(bar, Colors::headerBg(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    w_.saveState = addLabel_(bar, "", &lv_font_montserrat_14, Colors::muted(), 0, 0);
    lv_obj_align(w_.saveState, LV_ALIGN_LEFT_MID, MARGIN + 2, 0);
    // Screen lock (U-13): filled label like the states, so it reads from a distance.
    w_.lockState = addLabel_(bar, "LOCKED", &lv_font_montserrat_14, Colors::textDark(), 0, 0);
    styleState_(w_.lockState);
    lv_obj_set_style_bg_color(w_.lockState, Colors::warn(), 0);
    lv_obj_set_style_bg_opa(w_.lockState, LV_OPA_COVER, 0);
    lv_obj_align(w_.lockState, LV_ALIGN_LEFT_MID, STATE_X, 0);
    // WiFi: symbol (green connected, gray searching) or AP (warning color when open).
    w_.wifiState = addLabel_(bar, "", &lv_font_montserrat_12, Colors::muted(), 0, 0);
    lv_obj_align(w_.wifiState, LV_ALIGN_LEFT_MID, WIFI_X, 0);
    // REMOTE <source> after a remote command; LOCKED takes its place while locked.
    w_.remoteState = addLabel_(bar, "", &lv_font_montserrat_12, Colors::textDark(), 0, 0);
    styleState_(w_.remoteState);
    lv_obj_set_style_bg_color(w_.remoteState, Colors::ch3(), 0);
    lv_obj_set_style_bg_opa(w_.remoteState, LV_OPA_COVER, 0);
    lv_obj_align(w_.remoteState, LV_ALIGN_LEFT_MID, STATE_X, 0);
    addButton_(bar, "CONFIG", CONFIG_X, 1, CONFIG_W, BAR_H - 2, Colors::config(), Action::Navigate,
               (uint8_t)PageId::Config);
}

// Detail line after the state label: at DETAIL_X, further right after a wider state.
static void placeDetail(lv_obj_t* detail, lv_obj_t* state) {
    lv_point_t size;
    lv_txt_get_size(&size, lv_label_get_text(state), &lv_font_montserrat_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int x = LV_MAX(DETAIL_X, size.x + 2 * 2 + 6); // state padding 2 px per side
    lv_obj_set_x(detail, x);
    lv_obj_set_width(detail, DIVIDER_X - 4 - x);
}

// The requested frequency of a card. In Integer mode the output lies on a raster; "~" marks
// a deviation from 1 Hz on. The label is right-aligned, so the digits stay aligned. From
// 100 MHz on "~100.000 000 MHz" (187 px) does not fit the 183 px column; the mode line shows
// "Integer" there anyway.
static void setCardFrequency(lv_obj_t* label, const AppController& app, uint8_t ch, uint64_t requested) {
    const double actual = app.actualHz(ch);
    const bool approx = app.system().tuning_mode == TuningMode::Integer && app.hardwareReady() && actual > 0 &&
                        requested < 100000000ULL &&
                        fabs(FrequencyUtils::deviation(actual, requested)) >= FrequencyUtils::DEVIATION_WARN_HZ;
    char buf[24];
    buf[0] = '~';
    FrequencyUtils::format(requested, buf + 1, sizeof(buf) - 1);
    Ui::setText(label, approx ? buf : buf + 1);
}

void Gui::refreshMain_() {
    const AppController& app = *app_;
    char buf[48];

    for (uint8_t ch = 1; ch <= 2; ++ch) {
        const uint8_t i = ch - 1;
        const Channel& c = app.channel(ch);
        // A coupled CH2 shows the frequency it is set to (CH1 + offset, E-213).
        setCardFrequency(w_.cardFreq[i], app, ch, app.frequency(ch));
        showState_(w_.cardState[i], StatusText::channel(app.channelStatus(ch)));
        // Special modes first (they change the signal); a long line ends in "...".
        const bool iqMaster = app.ch3().master == ch && app.ch3Status() != Ch3Status::Off;
        char extra[40] = "";
        if (ch == 2 && app.ch2Coupled()) {
            char off[20];
            FrequencyUtils::formatOffsetCompact(app.ch2Coupling().offset, off, sizeof(off));
            snprintf(extra, sizeof(extra), "CH1 %s%s", off, iqMaster ? " | IQ master" : "");
        } else if (iqMaster) {
            snprintf(extra, sizeof(extra), "IQ master");
        }
        showModes_(w_.cardDetail[i], ch, extra, iqMaster && !(ch == 2 && app.ch2Coupled()) ? "IQ master of CH3" : nullptr);
        placeDetail(w_.cardDetail[i], w_.cardState[i]);
        if (c.enabled) lv_obj_add_state(w_.cardSwitch[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(w_.cardSwitch[i], LV_STATE_CHECKED);
    }

    const Channel3& c3 = app.ch3();
    // CLK2 has an actual frequency only while IQ runs.
    setCardFrequency(w_.cardFreq[2], app, 3, app.ch3Frequency());
    char iqLimit[12], tooLow[24], tooHigh[24];
    FrequencyUtils::formatIqLimit(app.iqMinHz(), true, 2, false, iqLimit, sizeof(iqLimit));
    snprintf(tooLow, sizeof(tooLow), " / < %s MHz", iqLimit);
    FrequencyUtils::formatIqLimit(app.iqMaxHz(), false, 2, false, iqLimit, sizeof(iqLimit));
    snprintf(tooHigh, sizeof(tooHigh), " / > %s MHz", iqLimit);
    const Ch3Status st3 = app.ch3Status();
    showState_(w_.cardState[2], StatusText::ch3(st3));
    const char* reason = "";
    switch (st3) {
        case Ch3Status::MasterOff: reason = " / master off"; break;
        case Ch3Status::FreqTooLow: reason = tooLow; break;
        case Ch3Status::FreqTooHigh: reason = tooHigh; break;
        case Ch3Status::Error: reason = " / setup failed"; break;
        case Ch3Status::Off:
        case Ch3Status::Active:
        case Ch3Status::NoDevice: break;
    }
    snprintf(buf, sizeof(buf), "CH%u / %u\xC2\xB0%s", c3.master, c3.phase, reason);
    Ui::setText(w_.cardDetail[2], buf);
    placeDetail(w_.cardDetail[2], w_.cardState[2]);
    if (c3.enabled) lv_obj_add_state(w_.cardSwitch[2], LV_STATE_CHECKED);
    else lv_obj_clear_state(w_.cardSwitch[2], LV_STATE_CHECKED);
    refreshSaveState_();
    refreshStatusBar_();
}

bool Gui::remoteShown_() const {
    return app_->lastRemote() != Source::Touch && millis() - app_->lastRemoteMs() < REMOTE_INDICATOR_MS;
}

StatusView Gui::statusView_() {
    StatusView v;
    memset(&v, 0, sizeof(v));
    if (remoteShown_()) v.remote = (uint8_t)((uint8_t)app_->lastRemote() + 1);
    if (!services_.wifi) return v;
    const WifiManager::Status& w = services_.wifi->status();
    switch (currentPage_) {
        case PageId::Main: // status bar: WiFi symbol or AP
            v.wifiMode = (uint8_t)w.mode;
            v.staConnected = w.staConnected;
            v.apActive = w.apActive;
            v.apOpen = w.apOpen;
            break;
        case PageId::Config:
            v.wifiMode = (uint8_t)w.mode; // tab Net: WiFi dropdown
            if (!w_.wifiInfo) break;      // the info lines exist on the Info tab only
            v.staConnected = w.staConnected;
            v.apActive = w.apActive;
            v.apOpen = w.apOpen;
            v.fallback = w.fallback;
            v.needsOpenConfirmation = w.needsOpenConfirmation;
            v.apStations = w.apStations;
            v.staIp = w.staIp;
            rssiShown_ = StatusViewRules::rssiShown(rssiShown_, w.staConnected ? w.rssi : 0);
            v.rssi = rssiShown_;
            if (StatusViewRules::heapDue(millis(), heapSampleMs_, heapSampled_)) sampleHeap_();
            v.heapStep = StatusViewRules::heapStep(heapFree_);
            v.webClients = services_.webClients ? services_.webClients() : 0;
            v.scpiClient = services_.scpiClient ? services_.scpiClient() : 0;
            break;
        case PageId::Wifi:
        case PageId::WifiScan:
        case PageId::TextEntry:
        case PageId::WifiAp:
        case PageId::WifiDevice: // status line, scan progress, connection feedback, AP state
            v.wifiMode = (uint8_t)w.mode;
            v.staConnected = w.staConnected;
            v.apActive = w.apActive;
            v.apOpen = w.apOpen;
            v.fallback = w.fallback;
            v.needsOpenConfirmation = w.needsOpenConfirmation;
            v.connecting = w.connecting;
            v.apStations = w.apStations;
            v.lastFailure = (uint8_t)w.lastFailure;
            v.staIp = w.staIp;
            v.scanState = (uint8_t)services_.wifi->scanState();
            v.scanCount = services_.wifi->scanList().count();
            {
                const uint16_t p = wifiPages_.pendingS();
                v.netPendingS = (uint8_t)(p > 255 ? 255 : p);
            }
            break;
        default: break;
    }
    return v;
}

void Gui::refreshStatusBar_() {
    if (!w_.wifiState) return;
    // WiFi
    const char* text = "";
    lv_color_t color = Colors::muted();
    if (services_.wifi) {
        const WifiManager::Status& w = services_.wifi->status();
        if (w.apActive) {
            text = w.apOpen ? "AP open" : "AP";
            color = w.apOpen ? Colors::warn() : Colors::ok();
        } else if (w.mode == WifiMode::Client) {
            text = LV_SYMBOL_WIFI;
            color = w.staConnected ? Colors::ok() : Colors::muted();
        }
    }
    Ui::setText(w_.wifiState, text);
    Ui::setTextColor(w_.wifiState, color);
    // LOCKED, else REMOTE <source> for a while after a remote command.
    const bool locked = lock_.locked();
    Ui::setHidden(w_.lockState, !locked);
    if (!locked && remoteShown_()) {
        const Source s = app_->lastRemote();
        Ui::setText(w_.remoteState, s == Source::Web ? "REMOTE WEB" : s == Source::Scpi ? "REMOTE SCPI" : "REMOTE USB");
        Ui::setHidden(w_.remoteState, false);
    } else {
        Ui::setHidden(w_.remoteState, true);
    }
}
