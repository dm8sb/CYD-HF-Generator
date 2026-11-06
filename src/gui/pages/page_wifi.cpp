#include "../gui.h"
#include "../gui_layout.h"
#include <net/net_settings.h>
#include <utils/logger.h>
#include <utils/sanitize.h>
#include <string.h>

using namespace Layout;

// WIFI pages (ARBEITSPLAN 3.4, S53b): overview with the two stored networks, scan list with
// pages of ROWS entries (no scrolling) and a text entry with the on-screen keyboard. Foreign
// SSIDs reach LVGL only sanitized (W-S9); keys are never shown once stored.
static constexpr int BACK_W = 48;
static constexpr int TITLE_X = 62;
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

void Gui::addBackTitle_(PageId back, const char* title, lv_color_t color) {
    addButton_(root_, LV_SYMBOL_LEFT, 4, 3, BACK_W, 36, Colors::gray(), Action::Navigate, (uint8_t)back);
    lv_obj_t* t = addLabel_(root_, title, &lv_font_montserrat_20, color, TITLE_X, 10);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_width(t, SCREEN_WIDTH - TITLE_X - 8);
}

// --- Overview ---------------------------------------------------------------

void Gui::createWifi_() {
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    addBackTitle_(PageId::Config, "WIFI", Colors::config());
    for (uint8_t slot = 0; slot < 2; ++slot) {
        lv_obj_t* card = addCard_(root_, MARGIN, NET_Y + slot * NET_PITCH, CONTENT_W, NET_H, Colors::config());
        lv_obj_set_style_pad_all(card, 4, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        w_.netText[slot] = addLabel_(card, "", &lv_font_montserrat_16, Colors::text(), 2, 0);
        lv_obj_set_width(w_.netText[slot], 170);
        lv_label_set_long_mode(w_.netText[slot], LV_LABEL_LONG_DOT);
        w_.netSub[slot] = addLabel_(card, "", &lv_font_montserrat_12, Colors::muted(), 2, 21);
        addButton_(card, "Change", 178, 0, 72, NET_H - 10, btnColor, Action::WifiEdit, slot);
        addButton_(card, LV_SYMBOL_TRASH, 254, 0, 40, NET_H - 10, Colors::gray(), Action::WifiDelete, slot);
    }
    // Access point: name and state, "Show" opens the page with the QR code (S53c).
    lv_obj_t* apCard = addCard_(root_, MARGIN, NET_Y + 2 * NET_PITCH, CONTENT_W, NET_H, Colors::config());
    lv_obj_set_style_pad_all(apCard, 4, 0);
    lv_obj_set_style_border_width(apCard, 1, 0);
    w_.apText = addLabel_(apCard, "", &lv_font_montserrat_16, Colors::text(), 2, 0);
    w_.apSub = addLabel_(apCard, "", &lv_font_montserrat_12, Colors::muted(), 2, 19);
    addButton_(apCard, "Show", 178, 0, 116, NET_H - 10, btnColor, Action::Navigate, (uint8_t)PageId::WifiAp);
    w_.wifiStatus = addLabel_(root_, "", &lv_font_montserrat_14, Colors::muted(), MARGIN + 2, STATUS_Y);
    lv_obj_set_width(w_.wifiStatus, CONTENT_W - 4);
    lv_label_set_long_mode(w_.wifiStatus, LV_LABEL_LONG_WRAP);
}

void Gui::refreshWifi_() {
    if (!wifi_) return;
    const NetSettings& s = wifi_->settings();
    char buf[128];
    for (uint8_t slot = 0; slot < 2; ++slot) {
        const WifiNetwork& n = s.networks[slot];
        if (n.ssid[0]) {
            char shown[33];
            Sanitize::text(n.ssid, strlen(n.ssid), shown, sizeof(shown));
            snprintf(buf, sizeof(buf), "%u  %s", (unsigned)(slot + 1), shown);
            lv_label_set_text(w_.netSub[slot], n.key[0] ? "Password set" : "Open network (no password)");
            lv_obj_set_style_text_color(w_.netSub[slot], n.key[0] ? Colors::muted() : Colors::warn(), 0);
        } else {
            snprintf(buf, sizeof(buf), "%u  -", (unsigned)(slot + 1));
            lv_label_set_text(w_.netSub[slot], "Empty");
            lv_obj_set_style_text_color(w_.netSub[slot], Colors::muted(), 0);
        }
        lv_label_set_text(w_.netText[slot], buf);
    }
    snprintf(buf, sizeof(buf), "AP  %s", wifi_->status().apSsid);
    lv_label_set_text(w_.apText, buf);
    const bool apRuns = wifi_->status().apActive;
    snprintf(buf, sizeof(buf), "%s, %s", s.apOpen ? "open" : "WPA2", apRuns ? "active" : "off");
    lv_label_set_text(w_.apSub, buf);
    lv_obj_set_style_text_color(w_.apSub, s.apOpen ? Colors::warn() : Colors::muted(), 0);
    // Feedback (ARBEITSPLAN 3.4): what the WiFi does now and why the last attempt failed.
    const WifiManager::Status& w = wifi_->status();
    lv_color_t color = Colors::muted();
    if (s.mode == WifiMode::Off) {
        snprintf(buf, sizeof(buf), "WiFi is Off. Choose Client in CONFIG > WiFi.");
    } else if (w.staConnected) {
        char ssid[33];
        char ip[16];
        Sanitize::text(w.ssid, strlen(w.ssid), ssid, sizeof(ssid));
        NetConfig::formatIpv4(w.staIp, ip, sizeof(ip));
        snprintf(buf, sizeof(buf), "Connected %s\n%s, %s.local", ssid, ip, s.deviceName);
        color = Colors::ok();
    } else if (s.mode == WifiMode::AccessPoint) {
        snprintf(buf, sizeof(buf), "Access point %s active", w.apSsid);
    } else if (w.apActive) {
        snprintf(buf, sizeof(buf), "Fallback: AP %s active\nStill searching", w.apSsid);
        color = Colors::warn();
    } else if (w.connecting) {
        snprintf(buf, sizeof(buf), "Connecting...");
    } else if (w.lastFailure == WifiManager::Failure::WrongPassword) {
        snprintf(buf, sizeof(buf), "Wrong password");
        color = Colors::error();
    } else if (w.lastFailure == WifiManager::Failure::NotFound) {
        snprintf(buf, sizeof(buf), "Network not found");
        color = Colors::warn();
    } else if (w.lastFailure == WifiManager::Failure::Other) {
        snprintf(buf, sizeof(buf), "Connection failed");
        color = Colors::warn();
    } else if (!s.networks[0].ssid[0] && !s.networks[1].ssid[0]) {
        snprintf(buf, sizeof(buf), "No network stored. Tap Change.");
    } else {
        snprintf(buf, sizeof(buf), "Searching...");
    }
    lv_label_set_text(w_.wifiStatus, buf);
    lv_obj_set_style_text_color(w_.wifiStatus, color, 0);
}

// --- Scan list --------------------------------------------------------------

void Gui::createWifiScan_() {
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    char title[16];
    snprintf(title, sizeof(title), "Network %u", (unsigned)(wifiSlot_ + 1));
    addBackTitle_(PageId::Wifi, title, Colors::config());
    addButton_(root_, "Scan", SCREEN_WIDTH - 4 - 66, 3, 66, 36, btnColor, Action::WifiRescan);
    for (uint8_t i = 0; i < ROWS; ++i) {
        lv_obj_t* row = addButton_(root_, "", MARGIN, ROW_Y + i * ROW_PITCH, CONTENT_W, ROW_H, Colors::cardBg(),
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
        w_.scanInfo[i] = addLabel_(row, "", &lv_font_montserrat_12, Colors::muted(), 0, 0);
        lv_obj_align(w_.scanInfo[i], LV_ALIGN_RIGHT_MID, -6, 0);
        w_.scanRow[i] = row;
    }
    addButton_(root_, LV_SYMBOL_LEFT, MARGIN, NAV_Y, 50, NAV_H, Colors::gray(), Action::WifiScanPage, 0);
    w_.scanPage = addLabel_(root_, "", &lv_font_montserrat_14, Colors::muted(), 0, 0);
    lv_obj_align(w_.scanPage, LV_ALIGN_TOP_LEFT, MARGIN + 58, NAV_Y + 9);
    addButton_(root_, LV_SYMBOL_RIGHT, MARGIN + 110, NAV_Y, 50, NAV_H, Colors::gray(), Action::WifiScanPage, 1);
    addButton_(root_, "Other...", SCREEN_WIDTH - 4 - 120, NAV_Y, 120, NAV_H, btnColor, Action::WifiOther);
    w_.scanState = addLabel_(root_, "", &lv_font_montserrat_16, Colors::muted(), 0, 0);
    lv_obj_align(w_.scanState, LV_ALIGN_TOP_MID, 0, ROW_Y + 2 * ROW_PITCH);
}

void Gui::refreshWifiScan_() {
    if (!wifi_) return;
    const ScanList& list = wifi_->scanList();
    const WifiManager::ScanState st = wifi_->scanState();
    const uint8_t pages = (uint8_t)((list.count() + ROWS - 1) / ROWS);
    if (scanPage_ >= pages) scanPage_ = pages ? (uint8_t)(pages - 1) : 0;
    const bool showList = st == WifiManager::ScanState::Done && list.count() > 0;
    for (uint8_t i = 0; i < ROWS; ++i) {
        const uint8_t idx = (uint8_t)(scanPage_ * ROWS + i);
        if (showList && idx < list.count()) {
            const ScanEntry& e = list.at(idx);
            lv_label_set_text(w_.scanName[i], e.shown);
            char info[24];
            snprintf(info, sizeof(info), "%d dBm %s", (int)e.rssi, e.secure ? "" : "open");
            lv_label_set_text(w_.scanInfo[i], info);
            lv_obj_set_style_text_color(w_.scanInfo[i], e.secure ? Colors::muted() : Colors::warn(), 0);
            lv_obj_clear_flag(w_.scanRow[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(w_.scanRow[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    const char* state = "";
    if (st == WifiManager::ScanState::Running) state = "Scanning...";
    else if (st == WifiManager::ScanState::Failed) state = "Scan failed. Tap Scan.";
    else if (st == WifiManager::ScanState::Done && list.count() == 0) state = "No network found";
    lv_label_set_text(w_.scanState, state);
    char page[12];
    if (showList) snprintf(page, sizeof(page), "%u/%u", (unsigned)(scanPage_ + 1), (unsigned)pages);
    else page[0] = '\0';
    lv_label_set_text(w_.scanPage, page);
}

// --- Text entry -------------------------------------------------------------

void Gui::createEntry_() {
    const bool apPass = entryPurpose_ == EntryPurpose::ApPassword;
    const bool password = entryPurpose_ == EntryPurpose::Password || apPass;
    char title[48];
    if (apPass) {
        snprintf(title, sizeof(title), "AP password");
    } else if (password) {
        char shown[33];
        Sanitize::text(entrySsid_, strlen(entrySsid_), shown, sizeof(shown));
        snprintf(title, sizeof(title), "Password %s", shown);
    } else {
        snprintf(title, sizeof(title), "Network name");
    }
    addBackTitle_(entryBack_, title, Colors::config());
    w_.ta = lv_textarea_create(root_);
    lv_obj_set_pos(w_.ta, 4, ENTRY_TA_Y);
    lv_obj_set_size(w_.ta, password ? SCREEN_WIDTH - 4 - 4 - 76 : SCREEN_WIDTH - 8, ENTRY_TA_H);
    lv_textarea_set_one_line(w_.ta, true);
    lv_textarea_set_max_length(w_.ta, password ? 63 : 32);
    // The AP password is shown on the AP page anyway: typed in clear.
    lv_textarea_set_password_mode(w_.ta, password && !apPass);
    lv_obj_set_style_text_font(w_.ta, &lv_font_montserrat_16, 0);
    lv_obj_set_style_border_color(w_.ta, Colors::config(), 0);
    lv_obj_add_state(w_.ta, LV_STATE_FOCUSED); // shows the cursor
    on_(w_.ta, LV_EVENT_VALUE_CHANGED, Action::EntryChanged, 0);
    if (password) {
        w_.entryShow = addButton_(root_, "Show", SCREEN_WIDTH - 4 - 76, ENTRY_TA_Y, 76, ENTRY_TA_H, Colors::gray(),
                                  Action::EntryShow, 0);
    }
    // Counter and error in the title row would collide with long titles: they sit on the keyboard top.
    w_.entryInfo = addLabel_(root_, "", &lv_font_montserrat_12, Colors::muted(), 0, 0);
    lv_obj_align(w_.entryInfo, LV_ALIGN_TOP_RIGHT, -6, ENTRY_TA_Y + ENTRY_TA_H + 1);
    lv_obj_add_flag(w_.entryInfo, LV_OBJ_FLAG_FLOATING);

    lv_obj_t* kb = lv_keyboard_create(root_);
    // lv_keyboard_create() aligns the keyboard to the bottom; a position would only add an offset
    // and push it off the screen. Size it and keep the bottom alignment.
    lv_obj_set_size(kb, SCREEN_WIDTH, SCREEN_HEIGHT - KB_Y);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, w_.ta);
    on_(kb, LV_EVENT_READY, Action::EntryDone, 0);
    on_(kb, LV_EVENT_CANCEL, Action::Navigate, (uint8_t)entryBack_);
    lv_obj_move_foreground(w_.entryInfo);
    if (!password && entrySsid_[0]) lv_textarea_set_text(w_.ta, entrySsid_);
    if (apPass && w_.entryShow) lv_label_set_text(lv_obj_get_child(w_.entryShow, 0), "Hide");
}

void Gui::refreshEntry_() {
    if (!w_.ta) return;
    const bool password = entryPurpose_ != EntryPurpose::Ssid;
    const size_t len = strlen(lv_textarea_get_text(w_.ta));
    char buf[48];
    if (entryError_[0]) snprintf(buf, sizeof(buf), "%s", entryError_);
    else snprintf(buf, sizeof(buf), "%u/%u", (unsigned)len, password ? 63u : 32u);
    lv_label_set_text(w_.entryInfo, buf);
    lv_obj_set_style_text_color(w_.entryInfo, entryError_[0] ? Colors::error() : Colors::muted(), 0);
}

// --- Actions ----------------------------------------------------------------

bool Gui::storeNetwork_(const char* ssid, const char* key) {
    if (!wifi_) return false;
    NetSettings s = wifi_->settings();
    if (NetConfig::setNetwork(s, wifiSlot_, ssid, key) != NetConfig::KeyResult::Ok) return false;
    if (saveNet_ && !saveNet_(s)) showMessage("WiFi", "Saving the WiFi settings failed.");
    wifi_->apply(s);
    memset(&s, 0, sizeof(s)); // no key copy left on the stack
    Logger::info("GUI", "WiFi network stored");
    return true;
}

void Gui::handleWifiAction_(Action action, uint8_t arg, lv_obj_t* obj) {
    switch (action) {
        case Action::WifiEdit:
            wifiSlot_ = arg;
            scanPage_ = 0;
            if (wifi_) wifi_->startScan();
            requestNavigate_(PageId::WifiScan);
            return;
        case Action::WifiDelete:
            if (!wifi_ || !wifi_->settings().networks[arg].ssid[0]) return;
            wifiSlot_ = arg;
            confirm_(Confirm::DeleteNetwork);
            return;
        case Action::WifiRescan:
            scanPage_ = 0;
            if (wifi_) wifi_->startScan();
            break;
        case Action::WifiScanPage:
            if (arg == 0 && scanPage_ > 0) --scanPage_;
            else if (arg == 1 && wifi_ && (uint8_t)((scanPage_ + 1) * ROWS) < wifi_->scanList().count()) ++scanPage_;
            break;
        case Action::WifiPick: {
            if (!wifi_) return;
            const uint8_t idx = (uint8_t)(scanPage_ * ROWS + arg);
            if (idx >= wifi_->scanList().count()) return;
            const ScanEntry& e = wifi_->scanList().at(idx);
            snprintf(entrySsid_, sizeof(entrySsid_), "%s", e.ssid);
            if (!e.secure) {
                confirm_(Confirm::OpenNetwork);
                return;
            }
            entryPurpose_ = EntryPurpose::Password;
            entryBack_ = PageId::WifiScan;
            entryError_[0] = '\0';
            requestNavigate_(PageId::TextEntry);
            return;
        }
        case Action::WifiOther:
            entrySsid_[0] = '\0';
            entryPurpose_ = EntryPurpose::Ssid;
            entryBack_ = PageId::WifiScan;
            entryError_[0] = '\0';
            requestNavigate_(PageId::TextEntry);
            return;
        case Action::EntryShow: {
            const bool hidden = lv_textarea_get_password_mode(w_.ta);
            lv_textarea_set_password_mode(w_.ta, !hidden);
            lv_label_set_text(lv_obj_get_child(obj, 0), hidden ? "Hide" : "Show");
            break;
        }
        case Action::EntryChanged:
            entryError_[0] = '\0';
            break;
        case Action::EntryDone: {
            const char* text = lv_textarea_get_text(w_.ta);
            if (entryPurpose_ == EntryPurpose::ApPassword) {
                if (!NetConfig::validPassphrase(text)) {
                    snprintf(entryError_, sizeof(entryError_), "8-63 characters");
                    break;
                }
                storeAp_(text, false);
                lv_textarea_set_text(w_.ta, "");
                requestNavigate_(PageId::WifiAp);
                return;
            }
            if (entryPurpose_ == EntryPurpose::Ssid) {
                if (!NetConfig::validSsid(text)) {
                    snprintf(entryError_, sizeof(entryError_), "1-32 characters");
                    break;
                }
                snprintf(entrySsid_, sizeof(entrySsid_), "%s", text);
                entryPurpose_ = EntryPurpose::Password;
                requestNavigate_(PageId::TextEntry);
                return;
            }
            // A hidden network entered by name may be open (empty password).
            if (text[0] && !NetConfig::validPassphrase(text)) {
                snprintf(entryError_, sizeof(entryError_), "8-63 characters");
                break;
            }
            if (!storeNetwork_(entrySsid_, text)) {
                snprintf(entryError_, sizeof(entryError_), "Not accepted");
                break;
            }
            lv_textarea_set_text(w_.ta, "");
            requestNavigate_(PageId::Wifi);
            return;
        }
        case Action::ApRegenerate: {
            if (!wifi_) return;
            char pw[NetConfig::AP_PASSWORD_LEN + 1];
            wifi_->newApPassword(pw);
            storeAp_(pw, wifi_->settings().apOpen);
            memset(pw, 0, sizeof(pw));
            break;
        }
        case Action::ApSet:
            entryPurpose_ = EntryPurpose::ApPassword;
            entryBack_ = PageId::WifiAp;
            entryError_[0] = '\0';
            requestNavigate_(PageId::TextEntry);
            return;
        case Action::ApOpenToggle:
            if (!wifi_) return;
            if (wifi_->settings().apOpen) storeAp_(wifi_->settings().apPassword, false);
            else confirm_(Confirm::ApOpen);
            break;
        default:
            break;
    }
    refresh_();
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

void Gui::storeAp_(const char* password, bool open) {
    if (!wifi_) return;
    NetSettings s = wifi_->settings();
    snprintf(s.apPassword, sizeof(s.apPassword), "%s", password ? password : "");
    s.apOpen = open;
    if (saveNet_ && !saveNet_(s)) showMessage("WiFi", "Saving the WiFi settings failed.");
    wifi_->applyAp(s);
    memset(&s, 0, sizeof(s));
    Logger::info("GUI", open ? "AP stored: open" : "AP stored: WPA2");
}

void Gui::createWifiAp_() {
    const lv_color_t btnColor = lv_color_hex(0xF9A825);
    addBackTitle_(PageId::Wifi, "Access point", Colors::config());
    // The phone needs a light quiet zone around the code.
    lv_obj_t* frame = lv_obj_create(root_);
    lv_obj_remove_style_all(frame);
    lv_obj_set_pos(frame, QR_X, QR_Y);
    lv_obj_set_size(frame, QR_SIZE + 2 * QR_QUIET, QR_SIZE + 2 * QR_QUIET);
    lv_obj_set_style_bg_color(frame, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    w_.apQr = lv_qrcode_create(frame, QR_SIZE, lv_color_black(), lv_color_white());
    lv_obj_set_pos(w_.apQr, QR_QUIET, QR_QUIET);
    addLabel_(root_, "Name", &lv_font_montserrat_12, Colors::muted(), AP_TEXT_X, QR_Y);
    w_.apName = addLabel_(root_, "", &lv_font_montserrat_16, Colors::text(), AP_TEXT_X, QR_Y + 14);
    addLabel_(root_, "Password", &lv_font_montserrat_12, Colors::muted(), AP_TEXT_X, QR_Y + 40);
    w_.apPass = addLabel_(root_, "", &lv_font_montserrat_16, Colors::text(), AP_TEXT_X, QR_Y + 54);
    lv_obj_set_width(w_.apPass, SCREEN_WIDTH - AP_TEXT_X - 4);
    lv_label_set_long_mode(w_.apPass, LV_LABEL_LONG_WRAP);
    addLabel_(root_, "Address", &lv_font_montserrat_12, Colors::muted(), AP_TEXT_X, QR_Y + 96);
    addLabel_(root_, "192.168.4.1", &lv_font_montserrat_16, Colors::text(), AP_TEXT_X, QR_Y + 110);
    addButton_(root_, "Regenerate", MARGIN, AP_BTN_Y, 104, AP_BTN_H, btnColor, Action::ApRegenerate);
    addButton_(root_, "Set...", MARGIN + 108, AP_BTN_Y, 70, AP_BTN_H, btnColor, Action::ApSet);
    w_.apOpenBtn = addButton_(root_, "", MARGIN + 182, AP_BTN_Y, CONTENT_W - 182, AP_BTN_H, Colors::gray(),
                              Action::ApOpenToggle);
    w_.apState = addLabel_(root_, "", &lv_font_montserrat_12, Colors::muted(), MARGIN + 2, AP_BTN_Y + AP_BTN_H + 2);
    // A password exists from the first visit on, so the QR code always works (W-E3).
    if (wifi_ && !NetConfig::validPassphrase(wifi_->settings().apPassword)) {
        char pw[NetConfig::AP_PASSWORD_LEN + 1];
        wifi_->newApPassword(pw);
        storeAp_(pw, wifi_->settings().apOpen);
        memset(pw, 0, sizeof(pw));
    }
}

void Gui::refreshWifiAp_() {
    if (!wifi_) return;
    const NetSettings& s = wifi_->settings();
    const WifiManager::Status& w = wifi_->status();
    lv_label_set_text(w_.apName, w.apSsid);
    char qr[160];
    if (NetConfig::wifiQrText(w.apSsid, s.apPassword, s.apOpen, qr, sizeof(qr))) {
        lv_qrcode_update(w_.apQr, qr, strlen(qr));
        lv_obj_clear_flag(w_.apQr, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w_.apQr, LV_OBJ_FLAG_HIDDEN);
    }
    memset(qr, 0, sizeof(qr));
    lv_label_set_text(w_.apPass, s.apOpen ? "none (open)" : s.apPassword);
    lv_obj_set_style_text_color(w_.apPass, s.apOpen ? Colors::warn() : Colors::text(), 0);
    lv_label_set_text(lv_obj_get_child(w_.apOpenBtn, 0), s.apOpen ? "Secure" : "Open...");
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
    lv_label_set_text(w_.apState, buf);
}
