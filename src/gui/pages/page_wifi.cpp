// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../gui.h"
#include "../gui_layout.h"
#include <net/net_settings.h>
#include <net/request_guard.h>
#include <net/wifi_policy.h>
#include <utils/ipv4.h>
#include <utils/logger.h>
#include <utils/sanitize.h>
#include <string.h>

using namespace Layout;

// WIFI pages (A-504, S53b): overview with the two stored networks, scan list with
// pages of ROWS entries (no scrolling) and a text entry with the on-screen keyboard. Foreign
// SSIDs reach LVGL only sanitized (A-709); keys are never shown once stored.
static constexpr int NET_Y = 44;
static constexpr int NET_H = 42;
static constexpr int NET_PITCH = NET_H + 4;
static constexpr int STATUS_Y = NET_Y + 3 * NET_PITCH + 2; // two networks and the AP card
static constexpr int ROWS = 4;
static constexpr int ROW_Y = 42;
static constexpr int ROW_H = 34;
static constexpr int ROW_PITCH = ROW_H + 2;
static constexpr int NAV_Y = ROW_Y + ROWS * ROW_PITCH + 4;
static constexpr int NAV_H = 36;
static constexpr int ENTRY_TA_Y = 42;
static constexpr int ENTRY_TA_H = 38;
static constexpr int KB_Y = ENTRY_TA_Y + ENTRY_TA_H + 4;
static_assert(NAV_Y + NAV_H <= SCREEN_HEIGHT, "scan list fits without scrolling");
static_assert(STATUS_Y + 40 <= SCREEN_HEIGHT, "status lines fit");

// --- Frame (AF49b) ----------------------------------------------------------

void WifiPages::create(PageId page) {
    switch (page) {
        case PageId::Wifi: createWifi_(); break;
        case PageId::WifiScan: createWifiScan_(); break;
        case PageId::TextEntry: createEntry_(); break;
        case PageId::WifiAp: createWifiAp_(); break;
        case PageId::WifiDevice: createWifiDevice_(); break;
        default: break;
    }
}

void WifiPages::refresh(PageId page) {
    switch (page) {
        case PageId::Wifi: refreshWifi_(); break;
        case PageId::WifiScan: refreshWifiScan_(); break;
        case PageId::TextEntry: refreshEntry_(); break;
        case PageId::WifiAp: refreshWifiAp_(); break;
        case PageId::WifiDevice: refreshWifiDevice_(); break;
        default: break;
    }
}

void WifiPages::update() {
    const bool calibrating = gui_.currentPage_ == PageId::TouchCal;
    // A-702: an open fallback AP starts only after the question at the touch was answered. While
    // another dialog is open the question waits (CQ-1); once it is no longer needed it goes.
    if (svc_.wifi && svc_.wifi->status().needsOpenConfirmation) {
        if (!openFallbackAsked_ && !calibrating) {
            openFallbackAsked_ = true;
            gui_.confirm_(Gui::Confirm::OpenFallback);
        }
    } else {
        if (openFallbackAsked_) gui_.dropConfirm_(Gui::Confirm::OpenFallback);
        openFallbackAsked_ = false;
    }
    // A-712: a network change from the web without web password is confirmed here as well.
    if (svc_.netConfirmNeeded && svc_.netConfirmNeeded()) {
        if (!netConfirmAsked_ && !calibrating) {
            netConfirmAsked_ = true;
            gui_.confirm_(Gui::Confirm::WebNetwork);
        }
    } else {
        if (netConfirmAsked_) gui_.dropConfirm_(Gui::Confirm::WebNetwork);
        netConfirmAsked_ = false;
    }
    // AF16: with a web password the touch need not confirm, but the device shows the open change
    // with Revert (concept 2). The question also comes when a touch change is refused.
    const bool pending = svc_.netPendingS && svc_.netPendingS() > 0;
    if (pending && svc_.wifi && NetConfig::webPasswordSet(svc_.wifi->settings())) {
        if (!netPendingAsked_ && !calibrating) {
            netPendingAsked_ = true;
            gui_.confirm_(Gui::Confirm::WebPending);
        }
    } else if (!pending) {
        if (netPendingAsked_) netPendingAsked_ = false;
        gui_.dropConfirm_(Gui::Confirm::WebPending); // no-op when it is not queued
    }
    pollWebPassword_();
}

// --- Confirmations ------------------------------------------------------------

void WifiPages::answerDeleteNetwork(bool yes, uint8_t slot) {
    if (!yes || !svc_.wifi) return;
    NetSettings s = svc_.wifi->settings();
    NetConfig::clearNetwork(s, slot);
    commitNet(s, NetApply::Restart);
}

void WifiPages::answerOpenNetwork(bool yes, uint8_t) {
    if (yes && storeNetwork_(entrySsid_, "")) gui_.requestNavigate_(PageId::Wifi);
}

void WifiPages::answerApOpen(bool yes, uint8_t) {
    if (yes && svc_.wifi) storeAp_(svc_.wifi->settings().apPassword, true);
}

void WifiPages::answerOpenFallback(bool yes, uint8_t) {
    if (yes && svc_.wifi) svc_.wifi->confirmOpenFallback();
}

// Network change and Web change pending: Keep (yes) or Revert.
void WifiPages::answerWebChange(bool yes, uint8_t) {
    if (svc_.netConfirmAnswer) svc_.netConfirmAnswer(yes);
}

void WifiPages::answerRemoveWebPassword(bool yes, uint8_t) {
    if (!yes) return;
    storeWebPassword_("");
    Logger::info("GUI", "Web password removed");
    gui_.requestNavigate_(PageId::WifiDevice);
}

// The same header as the channel pages and CONFIG: back at the top left, then the title.
void WifiPages::addBackTitle_(PageId back, const char* title, lv_color_t color) {
    gui_.createHeader_(title, color, 0, back);
}

// --- Overview ---------------------------------------------------------------

void WifiPages::createWifi_() {
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    addBackTitle_(PageId::Config, "WIFI", Colors::config());
    gui_.addButton_(gui_.root_, "Device", SCREEN_WIDTH - 4 - 84, BACK_Y, 84, BACK_H, btnColor, Action::Navigate,
                    (uint8_t)PageId::WifiDevice);
    for (uint8_t slot = 0; slot < 2; ++slot) {
        lv_obj_t* card = gui_.addCard_(gui_.root_, MARGIN, NET_Y + slot * NET_PITCH, CONTENT_W, NET_H, Colors::config());
        lv_obj_set_style_pad_all(card, 4, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        w_.netText[slot] = gui_.addLabel_(card, "", &lv_font_montserrat_16, Colors::text(), 2, 0);
        lv_obj_set_width(w_.netText[slot], 170);
        lv_label_set_long_mode(w_.netText[slot], LV_LABEL_LONG_DOT);
        w_.netSub[slot] = gui_.addLabel_(card, "", &lv_font_montserrat_12, Colors::muted(), 2, 21);
        gui_.addButton_(card, "Change", 178, 0, 72, NET_H - 10, btnColor, Action::WifiEdit, slot);
        gui_.addButton_(card, LV_SYMBOL_TRASH, 254, 0, 40, NET_H - 10, Colors::gray(), Action::WifiDelete, slot);
    }
    // Access point: name and state, "Show" opens the page with the QR code (S53c).
    lv_obj_t* apCard = gui_.addCard_(gui_.root_, MARGIN, NET_Y + 2 * NET_PITCH, CONTENT_W, NET_H, Colors::config());
    lv_obj_set_style_pad_all(apCard, 4, 0);
    lv_obj_set_style_border_width(apCard, 1, 0);
    w_.apText = gui_.addLabel_(apCard, "", &lv_font_montserrat_16, Colors::text(), 2, 0);
    w_.apSub = gui_.addLabel_(apCard, "", &lv_font_montserrat_12, Colors::muted(), 2, 19);
    gui_.addButton_(apCard, "Show", 178, 0, 116, NET_H - 10, btnColor, Action::Navigate, (uint8_t)PageId::WifiAp);
    w_.wifiStatus = gui_.addLabel_(gui_.root_, "", &lv_font_montserrat_14, Colors::muted(), MARGIN + 2, STATUS_Y);
    lv_obj_set_width(w_.wifiStatus, CONTENT_W - 4);
    lv_label_set_long_mode(w_.wifiStatus, LV_LABEL_LONG_WRAP);
}

void WifiPages::refreshWifi_() {
    if (!svc_.wifi) return;
    const NetSettings& s = svc_.wifi->settings();
    char buf[128];
    for (uint8_t slot = 0; slot < 2; ++slot) {
        const WifiNetwork& n = s.networks[slot];
        if (n.ssid[0]) {
            char shown[33];
            Sanitize::text(n.ssid, strlen(n.ssid), shown, sizeof(shown));
            snprintf(buf, sizeof(buf), "%u  %s", (unsigned)(slot + 1), shown);
            Ui::setText(w_.netSub[slot], n.key[0] ? "Password set" : "Open network (no password)");
            Ui::setTextColor(w_.netSub[slot], n.key[0] ? Colors::muted() : Colors::warn());
        } else {
            snprintf(buf, sizeof(buf), "%u  -", (unsigned)(slot + 1));
            Ui::setText(w_.netSub[slot], "Empty");
            Ui::setTextColor(w_.netSub[slot], Colors::muted());
        }
        Ui::setText(w_.netText[slot], buf);
    }
    snprintf(buf, sizeof(buf), "AP  %s", svc_.wifi->status().apSsid);
    Ui::setText(w_.apText, buf);
    const bool apRuns = svc_.wifi->status().apActive;
    snprintf(buf, sizeof(buf), "%s, %s", s.apOpen ? "open" : "WPA2", apRuns ? "active" : "off");
    Ui::setText(w_.apSub, buf);
    Ui::setTextColor(w_.apSub, s.apOpen ? Colors::warn() : Colors::muted());
    // Feedback (A-504): what the WiFi does now and why the last attempt failed.
    const WifiManager::Status& w = svc_.wifi->status();
    StatusText::WifiLine line;
    line.pendingS = svc_.netPendingS ? svc_.netPendingS() : 0;
    line.mode = s.mode;
    line.staConnected = w.staConnected;
    line.ssid = w.ssid;
    line.staIp = w.staIp;
    line.deviceName = s.deviceName;
    line.apSsid = w.apSsid;
    line.apActive = w.apActive;
    line.connecting = w.connecting;
    line.failure = w.lastFailure;
    line.networkStored = s.networks[0].ssid[0] || s.networks[1].ssid[0];
    const lv_color_t color = Colors::forTone(StatusText::wifiLine(line, buf, sizeof(buf)));
    Ui::setText(w_.wifiStatus, buf);
    Ui::setTextColor(w_.wifiStatus, color);
}

// --- Scan list --------------------------------------------------------------

void WifiPages::createWifiScan_() {
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    char title[16];
    snprintf(title, sizeof(title), "Network %u", (unsigned)(wifiSlot_ + 1));
    addBackTitle_(PageId::Wifi, title, Colors::config());
    gui_.addButton_(gui_.root_, "Scan", SCREEN_WIDTH - 4 - 66, BACK_Y, 66, BACK_H, btnColor, Action::WifiRescan);
    for (uint8_t i = 0; i < ROWS; ++i) {
        lv_obj_t* row = gui_.addButton_(gui_.root_, "", MARGIN, ROW_Y + i * ROW_PITCH, CONTENT_W, ROW_H, Colors::cardBg(),
                                   Action::WifiPick, i);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, Colors::outline(), 0);
        lv_obj_t* name = lv_obj_get_child(row, 0); // the button label: the SSID
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name, Colors::text(), 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 6, 0);
        lv_obj_set_width(name, 200);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        w_.scanName[i] = name;
        w_.scanInfo[i] = gui_.addLabel_(row, "", &lv_font_montserrat_12, Colors::muted(), 0, 0);
        lv_obj_align(w_.scanInfo[i], LV_ALIGN_RIGHT_MID, -6, 0);
        w_.scanRow[i] = row;
    }
    gui_.addButton_(gui_.root_, LV_SYMBOL_UP, MARGIN, NAV_Y, 50, NAV_H, Colors::gray(), Action::WifiScanPage, 0);
    w_.scanPage = gui_.addLabel_(gui_.root_, "", &lv_font_montserrat_14, Colors::muted(), 0, 0);
    lv_obj_align(w_.scanPage, LV_ALIGN_TOP_LEFT, MARGIN + 58, NAV_Y + 9);
    gui_.addButton_(gui_.root_, LV_SYMBOL_DOWN, MARGIN + 110, NAV_Y, 50, NAV_H, Colors::gray(), Action::WifiScanPage, 1);
    gui_.addButton_(gui_.root_, "Other...", SCREEN_WIDTH - 4 - 120, NAV_Y, 120, NAV_H, btnColor, Action::WifiOther);
    w_.scanState = gui_.addLabel_(gui_.root_, "", &lv_font_montserrat_16, Colors::muted(), 0, 0);
    lv_obj_align(w_.scanState, LV_ALIGN_TOP_MID, 0, ROW_Y + 2 * ROW_PITCH);
}

void WifiPages::refreshWifiScan_() {
    if (!svc_.wifi) return;
    const ScanList& list = svc_.wifi->scanList();
    const WifiManager::ScanState st = svc_.wifi->scanState();
    const uint8_t pages = InputLogic::scanPages(list.count(), ROWS);
    scanPage_ = InputLogic::clampScanPage(scanPage_, list.count(), ROWS);
    const bool showList = st == WifiManager::ScanState::Done && list.count() > 0;
    for (uint8_t i = 0; i < ROWS; ++i) {
        uint8_t idx = 0;
        if (showList && InputLogic::scanIndex(scanPage_, i, list.count(), ROWS, &idx)) {
            const ScanEntry& e = list.at(idx);
            Ui::setText(w_.scanName[i], e.shown);
            char info[24];
            snprintf(info, sizeof(info), "%d dBm %s", (int)e.rssi, e.secure ? "" : "open");
            Ui::setText(w_.scanInfo[i], info);
            Ui::setTextColor(w_.scanInfo[i], e.secure ? Colors::muted() : Colors::warn());
            Ui::setHidden(w_.scanRow[i], false);
        } else {
            Ui::setHidden(w_.scanRow[i], true);
        }
    }
    const char* state = "";
    if (st == WifiManager::ScanState::Running) state = "Scanning...";
    else if (st == WifiManager::ScanState::Failed) state = "Scan failed. Tap Scan.";
    else if (st == WifiManager::ScanState::Done && list.count() == 0) state = "No network found";
    Ui::setText(w_.scanState, state);
    char page[12];
    if (showList) snprintf(page, sizeof(page), "%u/%u", (unsigned)(scanPage_ + 1), (unsigned)pages);
    else page[0] = '\0';
    Ui::setText(w_.scanPage, page);
}

// --- Text entry -------------------------------------------------------------

void WifiPages::createEntry_() {
    using InputLogic::EntryPurpose;
    const InputLogic::EntrySpec spec = InputLogic::entrySpec(entryPurpose_);
    char title[48];
    InputLogic::entryTitle(entryPurpose_, entryField_, entrySsid_, title, sizeof(title));
    addBackTitle_(entryBack_, title, Colors::config());
    w_.ta = lv_textarea_create(gui_.root_);
    lv_obj_set_pos(w_.ta, 4, ENTRY_TA_Y);
    lv_obj_set_size(w_.ta, spec.secret ? SCREEN_WIDTH - 4 - 4 - 76 : SCREEN_WIDTH - 8, ENTRY_TA_H);
    lv_textarea_set_one_line(w_.ta, true);
    lv_textarea_set_max_length(w_.ta, spec.maxLen);
    if (spec.accepted) lv_textarea_set_accepted_chars(w_.ta, spec.accepted);
    lv_textarea_set_password_mode(w_.ta, spec.masked);
    lv_obj_set_style_text_font(w_.ta, &lv_font_montserrat_16, 0);
    lv_obj_set_style_border_color(w_.ta, Colors::config(), 0);
    lv_obj_add_state(w_.ta, LV_STATE_FOCUSED); // shows the cursor
    gui_.on_(w_.ta, LV_EVENT_VALUE_CHANGED, Action::EntryChanged, 0);
    if (spec.secret) {
        w_.entryShow = gui_.addButton_(gui_.root_, "Show", SCREEN_WIDTH - 4 - 76, ENTRY_TA_Y, 76, ENTRY_TA_H, Colors::gray(),
                                  Action::EntryShow, 0);
    }
    // Counter and error in the title row would collide with long titles: they sit on the keyboard top.
    w_.entryInfo = gui_.addLabel_(gui_.root_, "", &lv_font_montserrat_12, Colors::muted(), 0, 0);
    lv_obj_align(w_.entryInfo, LV_ALIGN_TOP_RIGHT, -6, ENTRY_TA_Y + ENTRY_TA_H + 1);
    lv_obj_add_flag(w_.entryInfo, LV_OBJ_FLAG_FLOATING);

    lv_obj_t* kb = lv_keyboard_create(gui_.root_);
    // lv_keyboard_create() aligns the keyboard to the bottom; a position would only add an offset
    // and push it off the screen. Size it and keep the bottom alignment.
    lv_obj_set_size(kb, SCREEN_WIDTH, SCREEN_HEIGHT - KB_Y);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, w_.ta);
    if (spec.numeric) lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    gui_.on_(kb, LV_EVENT_READY, Action::EntryDone, 0);
    gui_.on_(kb, LV_EVENT_CANCEL, Action::Navigate, (uint8_t)entryBack_);
    lv_obj_move_foreground(w_.entryInfo);
    // Ssid, device name and IP fields start with the current text (entrySsid_ holds it).
    if (spec.prefill && entrySsid_[0]) lv_textarea_set_text(w_.ta, entrySsid_);
    if (spec.secret && !spec.masked && w_.entryShow) Ui::setText(lv_obj_get_child(w_.entryShow, 0), "Hide");
}

void WifiPages::refreshEntry_() {
    if (!w_.ta) return;
    const bool pwSet = svc_.wifi && NetConfig::webPasswordSet(svc_.wifi->settings());
    char buf[48];
    InputLogic::entryInfo(entryPurpose_, strlen(lv_textarea_get_text(w_.ta)), entryError_, pwSet, buf, sizeof(buf));
    Ui::setText(w_.entryInfo, buf);
    Ui::setTextColor(w_.entryInfo, entryError_[0] ? Colors::error() : Colors::muted());
}

void WifiPages::openEntry_(InputLogic::EntryPurpose purpose, PageId back, const char* prefill) {
    if (prefill) snprintf(entrySsid_, sizeof(entrySsid_), "%s", prefill);
    entryPurpose_ = purpose;
    entryBack_ = back;
    entryError_[0] = '\0';
    gui_.requestNavigate_(PageId::TextEntry);
}

// --- Actions ----------------------------------------------------------------

bool WifiPages::storeNetwork_(const char* ssid, const char* key) {
    if (!svc_.wifi) return false;
    NetSettings s = svc_.wifi->settings();
    if (NetConfig::setNetwork(s, wifiSlot_, ssid, key) != NetConfig::KeyResult::Ok) {
        memset(&s, 0, sizeof(s)); // no key copy left on the stack
        return false;
    }
    if (!commitNet(s, NetApply::Restart)) return false;
    Logger::info("GUI", "WiFi network stored");
    return true;
}

// OK on the keyboard: InputLogic checks the text, the page carries out the step. true: the
// page changes (or a confirmation redraws), false: stay and redraw (error shown).
bool WifiPages::submitEntry_() {
    using InputLogic::EntryStep;
    const char* text = lv_textarea_get_text(w_.ta);
    const bool pwSet = svc_.wifi && NetConfig::webPasswordSet(svc_.wifi->settings());
    const InputLogic::EntryResult r = InputLogic::submitEntry(entryPurpose_, text, pwSet);
    switch (r.step) {
        case EntryStep::Invalid:
            snprintf(entryError_, sizeof(entryError_), "%s", r.error);
            return false;
        case EntryStep::StoreName: {
            if (!svc_.wifi) return false;
            NetSettings s = svc_.wifi->settings();
            snprintf(s.deviceName, sizeof(s.deviceName), "%s", text);
            commitNet(s, NetApply::Restart);
            gui_.requestNavigate_(PageId::WifiDevice);
            return true;
        }
        case EntryStep::StoreIp: {
            if (!svc_.wifi) return false;
            NetSettings s = svc_.wifi->settings();
            uint32_t* fields[4] = {&s.staticIp.ip, &s.staticIp.mask, &s.staticIp.gateway, &s.staticIp.dns};
            *fields[entryField_ & 3] = r.ip;
            // While static IP is on, an implausible combination is refused (A-712).
            const NetConfig::IpCheck c = NetConfig::checkStaticIp(s.staticIp);
            if (s.staticIp.enabled && c != NetConfig::IpCheck::Ok) {
                memset(&s, 0, sizeof(s));
                gui_.showMessage("Static IP", NetConfig::ipCheckText(c));
                return false;
            }
            commitNet(s, NetApply::Restart);
            gui_.requestNavigate_(PageId::WifiDevice);
            return true;
        }
        case EntryStep::ConfirmRemove:
            gui_.confirm_(Gui::Confirm::RemoveWebPassword);
            return true; // gui_.confirm_() redraws
        case EntryStep::HashWebPassword:
            if (netChangeBlocked_()) return false; // before the hash: the password stays in the field
            // Hashed by a worker (~1.3 s); the device page shows "Saving..." until then.
            if (webPwHashing_ || !svc_.webPasswordStart || !svc_.webPasswordStart(text, strlen(text))) {
                snprintf(entryError_, sizeof(entryError_), "%s", InputLogic::ENTRY_BUSY);
                return false;
            }
            webPwHashing_ = true;
            lv_textarea_set_text(w_.ta, "");
            gui_.requestNavigate_(PageId::WifiDevice);
            return true;
        case EntryStep::StoreApPassword:
            storeAp_(text, false);
            lv_textarea_set_text(w_.ta, "");
            gui_.requestNavigate_(PageId::WifiAp);
            return true;
        case EntryStep::AskPassword:
            openEntry_(InputLogic::EntryPurpose::Password, entryBack_, text);
            return true;
        case EntryStep::StoreNetwork:
            if (!storeNetwork_(entrySsid_, text)) {
                snprintf(entryError_, sizeof(entryError_), "%s", InputLogic::ENTRY_NOT_ACCEPTED);
                return false;
            }
            lv_textarea_set_text(w_.ta, "");
            gui_.requestNavigate_(PageId::Wifi);
            return true;
    }
    return false;
}

void WifiPages::handleAction(Action action, uint8_t arg, lv_obj_t* obj) {
    switch (action) {
        case Action::WifiEdit:
            wifiSlot_ = arg;
            scanPage_ = 0;
            if (svc_.wifi) svc_.wifi->startScan();
            gui_.requestNavigate_(PageId::WifiScan);
            return;
        case Action::WifiDelete:
            if (!svc_.wifi || !svc_.wifi->settings().networks[arg].ssid[0]) return;
            wifiSlot_ = arg;
            gui_.confirm_(Gui::Confirm::DeleteNetwork, arg);
            return;
        case Action::WifiRescan:
            scanPage_ = 0;
            if (svc_.wifi) svc_.wifi->startScan();
            break;
        case Action::WifiScanPage:
            scanPage_ = InputLogic::stepScanPage(scanPage_, arg == 1, svc_.wifi ? svc_.wifi->scanList().count() : 0, ROWS);
            break;
        case Action::WifiPick: {
            if (!svc_.wifi) return;
            uint8_t idx = 0;
            if (!InputLogic::scanIndex(scanPage_, arg, svc_.wifi->scanList().count(), ROWS, &idx)) return;
            const ScanEntry& e = svc_.wifi->scanList().at(idx);
            if (!e.secure) {
                snprintf(entrySsid_, sizeof(entrySsid_), "%s", e.ssid);
                gui_.confirm_(Gui::Confirm::OpenNetwork);
                return;
            }
            openEntry_(InputLogic::EntryPurpose::Password, PageId::WifiScan, e.ssid);
            return;
        }
        case Action::WifiOther:
            openEntry_(InputLogic::EntryPurpose::Ssid, PageId::WifiScan, "");
            return;
        case Action::EntryShow: {
            const bool hidden = lv_textarea_get_password_mode(w_.ta);
            lv_textarea_set_password_mode(w_.ta, !hidden);
            Ui::setText(lv_obj_get_child(obj, 0), hidden ? "Hide" : "Show");
            break;
        }
        case Action::EntryChanged:
            entryError_[0] = '\0';
            break;
        case Action::EntryDone:
            if (submitEntry_()) return;
            break;
        case Action::ApRegenerate: {
            if (!svc_.wifi) return;
            char pw[NetConfig::AP_PASSWORD_LEN + 1];
            svc_.wifi->newApPassword(pw);
            storeAp_(pw, svc_.wifi->settings().apOpen);
            memset(pw, 0, sizeof(pw));
            break;
        }
        case Action::ApSet:
            openEntry_(InputLogic::EntryPurpose::ApPassword, PageId::WifiAp, nullptr);
            return;
        case Action::DevName:
            if (!svc_.wifi) return;
            openEntry_(InputLogic::EntryPurpose::DeviceName, PageId::WifiDevice, svc_.wifi->settings().deviceName);
            return;
        case Action::DevWebPassword:
            if (!svc_.wifi || webPwHashing_) return;
            openEntry_(InputLogic::EntryPurpose::WebPassword, PageId::WifiDevice, "");
            return;
        case Action::DevIpField: {
            if (!svc_.wifi) return;
            const StaticIp& ip = svc_.wifi->settings().staticIp;
            const uint32_t values[4] = {ip.ip, ip.mask, ip.gateway, ip.dns};
            char text[16] = "";
            if (values[arg & 3]) Ipv4::format(values[arg & 3], text, sizeof(text));
            entryField_ = arg & 3;
            openEntry_(InputLogic::EntryPurpose::IpField, PageId::WifiDevice, text);
            return;
        }
        case Action::DevIpMode: {
            if (!svc_.wifi) return;
            NetSettings s = svc_.wifi->settings();
            const bool wantStatic = lv_dropdown_get_selected(obj) == 1;
            if (wantStatic == s.staticIp.enabled) break;
            if (wantStatic) {
                // Empty fields take the current DHCP lease, so usually only the address changes.
                const WifiManager::Status& w = svc_.wifi->status();
                if (!s.staticIp.ip && w.staConnected) {
                    s.staticIp.ip = w.staIp;
                    s.staticIp.mask = w.staMask;
                    s.staticIp.gateway = w.staGateway;
                    s.staticIp.dns = w.staDns;
                }
                const NetConfig::IpCheck c = NetConfig::checkStaticIp(s.staticIp);
                if (c != NetConfig::IpCheck::Ok) {
                    // Keep the values for editing, stay on DHCP.
                    s.staticIp.enabled = false;
                    commitNet(s, NetApply::Restart);
                    gui_.showMessage("Static IP", NetConfig::ipCheckText(c));
                    break;
                }
            }
            s.staticIp.enabled = wantStatic;
            commitNet(s, NetApply::Restart);
            break;
        }
        case Action::ApOpenToggle:
            if (!svc_.wifi) return;
            if (svc_.wifi->settings().apOpen) storeAp_(svc_.wifi->settings().apPassword, false);
            else gui_.confirm_(Gui::Confirm::ApOpen);
            break;
        default:
            break;
    }
    gui_.refresh_();
}

// --- Access point (S53c) ----------------------------------------------------

static constexpr int QR_X = MARGIN;
static constexpr int QR_Y = 46;
static constexpr int QR_SIZE = 116;  // the code itself; a white margin of QR_QUIET around it
static constexpr int QR_QUIET = 4;
static constexpr int AP_TEXT_X = QR_X + QR_SIZE + 2 * QR_QUIET + 10;
static constexpr int AP_BTN_Y = QR_Y + QR_SIZE + 2 * QR_QUIET + 6;
static constexpr int AP_BTN_H = 36;
static_assert(AP_BTN_Y + AP_BTN_H + 16 <= SCREEN_HEIGHT, "buttons and state line fit");
// The AP state line names these times.
static_assert(WifiPolicy::FALLBACK_AFTER_MS == 60000, "state line says 60 s");
static_assert(WifiPolicy::OPEN_AP_IDLE_OFF_MS == 15 * 60000, "state line says 15 min");

void WifiPages::storeAp_(const char* password, bool open) {
    if (!svc_.wifi) return;
    NetSettings s = svc_.wifi->settings();
    snprintf(s.apPassword, sizeof(s.apPassword), "%s", password ? password : "");
    s.apOpen = open;
    if (commitNet(s, NetApply::Ap)) Logger::info("GUI", open ? "AP stored: open" : "AP stored: WPA2");
}

void WifiPages::createWifiAp_() {
    apQrHash_ = 0; // a new code widget
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    addBackTitle_(PageId::Wifi, "Access point", Colors::config());
    // The phone needs a light quiet zone around the code.
    lv_obj_t* frame = lv_obj_create(gui_.root_);
    lv_obj_remove_style_all(frame);
    lv_obj_set_pos(frame, QR_X, QR_Y);
    lv_obj_set_size(frame, QR_SIZE + 2 * QR_QUIET, QR_SIZE + 2 * QR_QUIET);
    lv_obj_set_style_bg_color(frame, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    w_.apQr = lv_qrcode_create(frame, QR_SIZE, lv_color_black(), lv_color_white());
    lv_obj_set_pos(w_.apQr, QR_QUIET, QR_QUIET);
    gui_.addLabel_(gui_.root_, "Name", &lv_font_montserrat_12, Colors::muted(), AP_TEXT_X, QR_Y);
    w_.apName = gui_.addLabel_(gui_.root_, "", &lv_font_montserrat_16, Colors::text(), AP_TEXT_X, QR_Y + 14);
    gui_.addLabel_(gui_.root_, "Password", &lv_font_montserrat_12, Colors::muted(), AP_TEXT_X, QR_Y + 40);
    w_.apPass = gui_.addLabel_(gui_.root_, "", &lv_font_montserrat_16, Colors::text(), AP_TEXT_X, QR_Y + 54);
    lv_obj_set_width(w_.apPass, SCREEN_WIDTH - AP_TEXT_X - 4);
    lv_label_set_long_mode(w_.apPass, LV_LABEL_LONG_WRAP);
    gui_.addLabel_(gui_.root_, "Address", &lv_font_montserrat_12, Colors::muted(), AP_TEXT_X, QR_Y + 96);
    gui_.addLabel_(gui_.root_, "192.168.4.1", &lv_font_montserrat_16, Colors::text(), AP_TEXT_X, QR_Y + 110);
    gui_.addButton_(gui_.root_, "Regenerate", MARGIN, AP_BTN_Y, 104, AP_BTN_H, btnColor, Action::ApRegenerate);
    gui_.addButton_(gui_.root_, "Set...", MARGIN + 108, AP_BTN_Y, 70, AP_BTN_H, btnColor, Action::ApSet);
    w_.apOpenBtn = gui_.addButton_(gui_.root_, "", MARGIN + 182, AP_BTN_Y, CONTENT_W - 182, AP_BTN_H, Colors::gray(),
                              Action::ApOpenToggle);
    w_.apState = gui_.addLabel_(gui_.root_, "", &lv_font_montserrat_12, Colors::muted(), MARGIN + 2, AP_BTN_Y + AP_BTN_H + 2);
    // A password exists from the first visit on, so the QR code always works (E-503); not while a
    // web change is open (AF16: it would be refused).
    if (svc_.wifi && !NetConfig::validPassphrase(svc_.wifi->settings().apPassword) && !(svc_.netPendingS && svc_.netPendingS())) {
        char pw[NetConfig::AP_PASSWORD_LEN + 1];
        svc_.wifi->newApPassword(pw);
        storeAp_(pw, svc_.wifi->settings().apOpen);
        memset(pw, 0, sizeof(pw));
    }
}

void WifiPages::refreshWifiAp_() {
    if (!svc_.wifi) return;
    const NetSettings& s = svc_.wifi->settings();
    const WifiManager::Status& w = svc_.wifi->status();
    Ui::setText(w_.apName, w.apSsid);
    char qr[160];
    if (NetConfig::wifiQrText(w.apSsid, s.apPassword, s.apOpen, qr, sizeof(qr))) {
        // Rebuilt only for a new text (AF12): the code is the largest area of the page.
        uint32_t h = 2166136261u;
        for (const char* c = qr; *c; ++c) h = (h ^ (uint8_t)*c) * 16777619u;
        if (h == 0) h = 1;
        if (h != apQrHash_) {
            lv_qrcode_update(w_.apQr, qr, strlen(qr));
            apQrHash_ = h;
        }
        Ui::setHidden(w_.apQr, false);
    } else {
        Ui::setHidden(w_.apQr, true);
    }
    memset(qr, 0, sizeof(qr));
    Ui::setText(w_.apPass, s.apOpen ? "none (open)" : s.apPassword);
    Ui::setTextColor(w_.apPass, s.apOpen ? Colors::warn() : Colors::text());
    Ui::setText(lv_obj_get_child(w_.apOpenBtn, 0), s.apOpen ? "Secure" : "Open...");
    char buf[64];
    if (w.apActive) {
        snprintf(buf, sizeof(buf), "Active, %u connected%s", (unsigned)w.apStations, w.fallback ? " (fallback)" : "");
    } else if (s.mode == WifiMode::Client) {
        snprintf(buf, sizeof(buf), "Off; starts as fallback after 60 s without network");
    } else if (s.mode == WifiMode::AccessPoint) {
        snprintf(buf, sizeof(buf), s.apOpen ? "Off (open AP had no device for 15 min)" : "Starting...");
    } else {
        snprintf(buf, sizeof(buf), "Off. Choose Access point in CONFIG > WiFi.");
    }
    Ui::setText(w_.apState, buf);
}

// --- Device (S53d) ----------------------------------------------------------

// Seven rows: name, IP mode, the four static IP fields and the web password (S55b). The rows
// are 26 px high (30 px until S55b) so that the seventh fits below the back button.
static constexpr int DEV_ROWS = 7;
static constexpr int DEV_Y = 42;
static constexpr int DEV_H = 26;
static constexpr int DEV_PITCH = DEV_H + 2;
static constexpr int DEV_LABEL_DY = (DEV_H - 16) / 2; // montserrat 14: 16 px line
static constexpr int DEV_VALUE_X = 104;
static constexpr int DEV_VALUE_W = CONTENT_W + MARGIN - DEV_VALUE_X;
static_assert(DEV_Y >= 3 + 36 + 2, "first row below the back button");
static_assert(DEV_Y + DEV_ROWS * DEV_PITCH <= SCREEN_HEIGHT, "seven rows fit");

// The device page shows a stored web password only as set / none (A-713: never as text).
static const char* webPasswordText(bool set, bool hashing) {
    if (hashing) return "Saving...";
    return set ? "Set (login needed)" : "None (no login)";
}

void WifiPages::storeWebPassword_(const char* record) {
    if (!svc_.wifi) return;
    NetSettings s = svc_.wifi->settings();
    snprintf(s.webPassword, sizeof(s.webPassword), "%s", record ? record : "");
    commitNet(s, NetApply::WebPassword);
}

void WifiPages::pollWebPassword_() {
    if (!webPwHashing_ || !svc_.webPasswordPoll) return;
    char record[WebPassword::RECORD_SIZE];
    const WebPasswordJob::Poll p = svc_.webPasswordPoll(record);
    if (p == WebPasswordJob::Poll::Busy) return;
    webPwHashing_ = false;
    if (p == WebPasswordJob::Poll::Done) {
        storeWebPassword_(record);
        Logger::info("GUI", "Web password set");
    } else {
        gui_.showMessage("Web password", "Setting the web password failed. Try again.");
    }
    memset(record, 0, sizeof(record));
    gui_.refresh_();
}

bool WifiPages::netChangeBlocked_() {
    if (!svc_.netPendingS || svc_.netPendingS() == 0) return false;
    Logger::info("GUI", "Network change refused: web change pending");
    gui_.confirm_(Gui::Confirm::WebPending);
    return true;
}

bool WifiPages::commitNet(NetSettings& s, NetApply how) {
    // One owner (AF16): while a web change waits, only the web change itself is saved.
    if (!svc_.wifi || netChangeBlocked_()) {
        memset(&s, 0, sizeof(s));
        return false;
    }
    if (svc_.saveNet && !svc_.saveNet(s)) gui_.showMessage("WiFi", "Saving the WiFi settings failed.");
    switch (how) {
        case NetApply::Restart: svc_.wifi->apply(s); break;
        case NetApply::Ap: svc_.wifi->applyAp(s); break;
        case NetApply::WebPassword: svc_.wifi->applyWebPassword(s.webPassword); break;
    }
    memset(&s, 0, sizeof(s)); // no copy of keys and passwords left on the stack
    return true;
}

void WifiPages::createWifiDevice_() {
    addBackTitle_(PageId::Wifi, "Device", Colors::config());
    static const char* const NAMES[DEV_ROWS] = {"Name", "IP", "Address", "Mask", "Gateway", "DNS", "Web pw."};
    for (uint8_t i = 0; i < DEV_ROWS; ++i) {
        const int y = DEV_Y + i * DEV_PITCH;
        gui_.addLabel_(gui_.root_, NAMES[i], &lv_font_montserrat_14, Colors::text(), MARGIN + 2, y + DEV_LABEL_DY);
        if (i == 1) {
            w_.devIpMode = lv_dropdown_create(gui_.root_);
            lv_dropdown_set_options(w_.devIpMode, "DHCP\nStatic");
            lv_obj_set_pos(w_.devIpMode, DEV_VALUE_X, y);
            lv_obj_set_size(w_.devIpMode, DEV_VALUE_W, DEV_H);
            lv_obj_set_style_pad_ver(w_.devIpMode, 4, 0);
            gui_.on_(w_.devIpMode, LV_EVENT_VALUE_CHANGED, Action::DevIpMode, 0);
            continue;
        }
        // The value is a tappable field that opens the keyboard.
        const bool web = i == DEV_ROWS - 1;
        const Action action = i == 0 ? Action::DevName : web ? Action::DevWebPassword : Action::DevIpField;
        lv_obj_t* b = gui_.addButton_(gui_.root_, "", DEV_VALUE_X, y, DEV_VALUE_W, DEV_H, Colors::cardBg(), action,
                                 (uint8_t)(i >= 2 && !web ? i - 2 : 0));
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, Colors::outline(), 0);
        lv_obj_t* l = lv_obj_get_child(b, 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, Colors::text(), 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 6, 0);
        if (i == 0) w_.devName = l;
        else if (web) w_.devWebPw = l;
        else w_.devField[i - 2] = l;
    }
}

void WifiPages::refreshWifiDevice_() {
    if (!svc_.wifi) return;
    const NetSettings& s = svc_.wifi->settings();
    const WifiManager::Status& w = svc_.wifi->status();
    char buf[48];
    snprintf(buf, sizeof(buf), "%s.local", s.deviceName);
    Ui::setText(w_.devName, buf);
    if (lv_dropdown_get_selected(w_.devIpMode) != (s.staticIp.enabled ? 1u : 0u)) {
        lv_dropdown_set_selected(w_.devIpMode, s.staticIp.enabled ? 1 : 0);
    }
    // Static: the stored values; DHCP: the current lease in gray (tap to prepare static values).
    const uint32_t stored[4] = {s.staticIp.ip, s.staticIp.mask, s.staticIp.gateway, s.staticIp.dns};
    const uint32_t lease[4] = {w.staIp, w.staMask, w.staGateway, w.staDns};
    for (uint8_t i = 0; i < 4; ++i) {
        const uint32_t v = s.staticIp.enabled ? stored[i] : (stored[i] ? stored[i] : lease[i]);
        if (v) Ipv4::format(v, buf, sizeof(buf));
        else snprintf(buf, sizeof(buf), i == 3 ? "= gateway" : "-");
        Ui::setText(w_.devField[i], buf);
        Ui::setTextColor(w_.devField[i], s.staticIp.enabled ? Colors::text() : Colors::muted());
    }
    const bool pwSet = NetConfig::webPasswordSet(s);
    Ui::setText(w_.devWebPw, webPasswordText(pwSet, webPwHashing_));
    // No password: everyone in the network can operate the device in the browser (warning color).
    Ui::setTextColor(w_.devWebPw, pwSet || webPwHashing_ ? Colors::text() : Colors::warn());
}
