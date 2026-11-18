// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lvgl.h>
#include <types.h>
#include <gui/gui_action.h>
#include <net/net_settings.h>
#include <net/web_state.h>
#include <net/wifi_manager.h>
#include <utils/input_logic.h>

class Gui;

// The WIFI pages (AF49b, CQ-3): overview, scan list, text entry, access point and device page,
// with their own widgets and state. Gui builds the frame (navigation, header, buttons, dialogs)
// and hands the WIFI actions, page builds and refreshes to this class; everything runs in
// loop() (LVGL), like Gui. Implemented in pages/page_wifi.cpp.
class WifiPages {
public:
    explicit WifiPages(Gui& gui) : gui_(gui) {}

    void setWifi(WifiManager& wifi, bool (*save)(const NetSettings&)) {
        wifi_ = &wifi;
        saveNet_ = save;
    }
    void setNetConfirm(bool (*needed)(), void (*answer)(bool keep), uint16_t (*pendingS)()) {
        netConfirmNeeded_ = needed;
        netConfirmAnswer_ = answer;
        netPendingS_ = pendingS;
    }
    void setWebPassword(bool (*start)(const char* password, size_t len), WebPasswordJob::Poll (*poll)(char* record)) {
        webPwStart_ = start;
        webPwPoll_ = poll;
    }

    // Seconds an open web network change still waits (0 = none, AF16).
    uint16_t pendingS() const { return netPendingS_ ? netPendingS_() : 0; }

    // Pages: Wifi, WifiScan, TextEntry, WifiAp, WifiDevice.
    void create(PageId page);
    void refresh(PageId page);
    // A new page is built: the widgets of the old one are gone.
    void clearWidgets() { w_ = Widgets{}; }
    void handleAction(GuiAction action, uint8_t arg, lv_obj_t* obj);
    // Every loop(): questions at the touch (open fallback AP W-S2, web change W-S12 / AF16) and
    // a finished web password hash.
    void update();

    // The only touch path for network settings (AF15, CQ-5): saves s, reports a failed save,
    // applies it (Restart: the WiFi restarts; Ap: only the access point; WebPassword: nothing
    // restarts, the web server ends its sessions) and wipes s (keys and passwords).
    // Refused while a web change is open (AF16): asks "Web change pending" and returns false.
    enum class NetApply : uint8_t { Restart, Ap, WebPassword };
    bool commitNet(NetSettings& s, NetApply how);

    // Answers of the WIFI confirmations (Gui::CONFIRMS_; yes = the action button).
    void answerDeleteNetwork(bool yes, uint8_t slot);
    void answerOpenNetwork(bool yes, uint8_t);
    void answerApOpen(bool yes, uint8_t);
    void answerOpenFallback(bool yes, uint8_t);
    void answerWebChange(bool yes, uint8_t);
    void answerRemoveWebPassword(bool yes, uint8_t);

private:
    typedef GuiAction Action;

    struct Widgets {
        lv_obj_t* netText[2];  // overview
        lv_obj_t* netSub[2];
        lv_obj_t* wifiStatus;
        lv_obj_t* apText;      // overview: AP card
        lv_obj_t* apSub;
        lv_obj_t* scanRow[5];  // scan list
        lv_obj_t* scanName[5];
        lv_obj_t* scanInfo[5];
        lv_obj_t* scanPage;
        lv_obj_t* scanState;
        lv_obj_t* ta;          // text entry
        lv_obj_t* entryShow;
        lv_obj_t* entryInfo;
        lv_obj_t* apQr;        // AP page
        lv_obj_t* apName;
        lv_obj_t* apPass;
        lv_obj_t* apOpenBtn;
        lv_obj_t* apState;
        lv_obj_t* devName;     // device page
        lv_obj_t* devIpMode;
        lv_obj_t* devField[4];
        lv_obj_t* devWebPw;
    };

    void addBackTitle_(PageId back, const char* title, lv_color_t color);
    void createWifi_();
    void refreshWifi_();
    void createWifiScan_();
    void refreshWifiScan_();
    void createEntry_();
    void refreshEntry_();
    // Opens the text entry for purpose (AF47); prefill: the text it starts with, also the
    // network a password is for (nullptr keeps the last one).
    void openEntry_(InputLogic::EntryPurpose purpose, PageId back, const char* prefill);
    // OK on the keyboard; true when the page changes or a confirmation opened.
    bool submitEntry_();
    bool storeNetwork_(const char* ssid, const char* key);
    void createWifiAp_();
    void refreshWifiAp_();
    // Stores AP password / open flag (saved, running AP restarts).
    void storeAp_(const char* password, bool open);
    void createWifiDevice_();
    void refreshWifiDevice_();
    // Web password (S55b): stores a record ("" = none) without restarting the WiFi.
    void storeWebPassword_(const char* record);
    // In update(): collects a finished hash.
    void pollWebPassword_();
    // A web network change is open: shows "Web change pending" (Revert / Keep) and returns true.
    bool netChangeBlocked_();

    Gui& gui_;
    Widgets w_ {};
    WifiManager* wifi_ = nullptr;
    bool (*saveNet_)(const NetSettings&) = nullptr;
    bool (*netConfirmNeeded_)() = nullptr;
    void (*netConfirmAnswer_)(bool keep) = nullptr;
    uint16_t (*netPendingS_)() = nullptr;
    bool (*webPwStart_)(const char*, size_t) = nullptr;
    WebPasswordJob::Poll (*webPwPoll_)(char*) = nullptr;
    // State (RAM only)
    bool openFallbackAsked_ = false; // W-S2 question shown for the current need
    bool netConfirmAsked_ = false;   // W-S12 question shown for the current web change
    bool netPendingAsked_ = false;   // AF16 hint shown for the current web change (with web password)
    bool webPwHashing_ = false;      // the device page shows "Saving..."
    uint8_t wifiSlot_ = 0;
    uint8_t scanPage_ = 0;
    InputLogic::EntryPurpose entryPurpose_ = InputLogic::EntryPurpose::Password;
    uint8_t entryField_ = 0; // IpField: 0 address, 1 mask, 2 gateway, 3 DNS
    PageId entryBack_ = PageId::WifiScan;
    char entrySsid_[33] {};  // network of the password entry, start text of name and IP fields
    char entryError_[24] {};
    uint32_t apQrHash_ = 0;  // QR text shown on the AP page (0 = none yet)
};
