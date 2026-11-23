// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <config.h>
#include <constants.h>
#include <core/app_controller.h>
#include <drivers/display_panel.h>
#include <drivers/touch.h>
#include <gui/gui_action.h>
#include <gui/gui_services.h>
#include <gui/wifi_pages.h>
#include <net/wifi_manager.h>
#include <net/web_state.h>
#include <utils/input_logic.h>
#include <utils/message_queue.h>
#include <utils/status_text.h>
#include <utils/status_view.h>

// Part and state of a style: LVGL 8 keeps them in two unnamed enums, and C++20 deprecates `|`
// between those (GCC 14 on Arduino 3.x warns).
inline lv_style_selector_t partState(uint32_t part, uint32_t state) { return part | state; }
#include <utils/touch_cal.h>

// Pages are built once per navigation. Actions change the AppController state and then
// refresh_() updates the existing widgets, so no widget is deleted inside its own event.
class Gui {
public:
    // CONFIG tabs: Signal, Freq, Net, Device, Info.
    static constexpr uint8_t CONFIG_TAB_COUNT = 5;
    // The display must already be initialized (DisplayPanel::begin()).
    void begin(AppController& app, TouchDriver& touch, DisplayPanel& display);
    void update();
    // Redraws the current page from the controller state (e.g. after the boot programming).
    void refresh() { refresh_(); }
    // One dialog at a time; further messages are queued, identical ones dropped.
    void showMessage(const char* title, const char* text, uint8_t tag = 0);
    // Controller events (V-3): a dialog, except for events of remote commands.
    static void onAppEvent(const Event& e, void* gui);
    void navigate(PageId page);
    // The page shown (the status LED follows it, AF51b).
    PageId page() const { return currentPage_; }
    // Touch calibration wizard (U-26). firstStart: no calibration stored; a timeout continues
    // with the default calibration on MAIN (otherwise back to CONFIG, unchanged).
    void calibrateTouch(bool firstStart);
    // Callbacks and the WiFi manager (AF49c); set by main once after begin().
    void setServices(const GuiServices& services) { services_ = services; }

private:
    friend class WifiPages;
    typedef GuiAction Action;

    struct Widgets {
        lv_obj_t* hdrSwitch;
        lv_obj_t* hdrState;
        lv_obj_t* freq;
        lv_obj_t* info;
        lv_obj_t* modes;       // special modes of the channel (U-2)
        lv_obj_t* chips[DELTA_STEP_COUNT];
        lv_obj_t* masterChips[2];
        lv_obj_t* cplChips[2]; // CH2 page: Free / CH1 + offset (E-213)
        lv_obj_t* cardFreq[3];
        lv_obj_t* cardState[3];
        lv_obj_t* cardDetail[3];
        lv_obj_t* cardSwitch[3];
        lv_obj_t* blValue;
        lv_obj_t* corrValue;
        lv_obj_t* corrStep;
        lv_obj_t* driveDd;
        lv_obj_t* tuningDd;
        lv_obj_t* spreadDd;
        lv_obj_t* offStateDd;
        lv_obj_t* si5351Info;
        lv_obj_t* pllInfo;     // CONFIG info card: PLL lock (A-226)
        lv_obj_t* saveState;   // CONFIG header, MAIN status bar: save errors only (E-313)
        lv_obj_t* ta;          // numpad
        lv_obj_t* calCross[2]; // touch calibration: horizontal and vertical line
        lv_obj_t* calText;
        lv_obj_t* calStatus;
        lv_obj_t* touchInfo;   // CONFIG info card
        lv_obj_t* lockState;   // MAIN status bar: LOCKED
        lv_obj_t* npError;     // numpad: why the last key was rejected (U-9)
        lv_obj_t* wifiDd;      // CONFIG
        lv_obj_t* remoteDd;
        lv_obj_t* wifiInfo;    // CONFIG info card
        lv_obj_t* heapInfo;
        lv_obj_t* wifiState;   // MAIN status bar: WiFi symbol or AP
        lv_obj_t* remoteState; // MAIN status bar: REMOTE <source>
        lv_obj_t* remoteInfo;  // CONFIG info card: web and SCPI clients
    };

    void createMain_();
    void createChannelPage_(uint8_t ch);
    void createCH3_();
    void createConfig_();
    void createNumpad_(uint8_t ch);
    lv_color_t numpadColor_() const;
    void createTouchCal_();
    // Feeds raw touch samples to the wizard and moves the cross (every loop on that page).
    void tickTouchCal_();
    void showCalTarget_();
    // Display and touch follow the controller's rotation (E-314): MADCTL, touch mirroring and a
    // full redraw; the calibration restarts in the new rotation. No-op while unchanged.
    void applyRotation_();
#if TEST_HIT_MAP
    // Test build only (MESSPLAN M-126): target crosses instead of the GUI.
    void createHitMap_();
    static void hitMapCb_(lv_event_t* e);
#endif

    void refresh_();
    void refreshMain_();
    void refreshChannel_();
    void refreshCH3_();
    void refreshConfig_();
    void refreshSaveState_();
    // MAIN status bar: WiFi and REMOTE (A-503).
    void refreshStatusBar_();
    // What the current page shows besides the controller state (AF12): a redraw happens only
    // when it differs from the view of the last redraw. Samples the heap for the info card.
    StatusView statusView_();
    void sampleHeap_();
    bool remoteShown_() const;

    // Returns the header bar for page-specific additions.
    lv_obj_t* createHeader_(const char* title, lv_color_t color, uint8_t switchChannel, PageId back = PageId::Main);
    // Switch = requested state; the label shows ERR in red when the output failed.
    // Switch = request, text = actual state (as on MAIN).
    void setHeaderState_(bool on, const StatusText::Text& state);
    // w_.info = actual output frequency of ch 1..3; deviations from 1 Hz on in the warning color.
    // Out line of a channel page; prefix (e.g. "Off | ") goes before it (U-17).
    void showOutput_(uint8_t ch, uint64_t requested, const char* prefix = "");
    // CONFIG: a row card at position `index` of the tab (title optional), and a row with a dropdown.
    lv_obj_t* addConfigRow_(lv_obj_t* parent, uint8_t index, int h, const char* title);
    lv_obj_t* addOptionRow_(lv_obj_t* parent, uint8_t index, const char* title, const char* options, Action action);
    lv_obj_t* addButton_(lv_obj_t* parent, const char* txt, int x, int y, int w, int h, lv_color_t color,
                         Action action, uint8_t arg = 0, bool repeat = false);
    lv_obj_t* addChip_(lv_obj_t* parent, const char* txt, int x, int y, int w, int h, lv_color_t color,
                       Action action, uint8_t arg);
    lv_obj_t* addCard_(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t borderColor);
    lv_obj_t* addLabel_(lv_obj_t* parent, const char* txt, const lv_font_t* font, lv_color_t color, int x, int y);
    lv_obj_t* addSwitch_(lv_obj_t* parent, uint8_t ch, lv_color_t color);
    void on_(lv_obj_t* obj, lv_event_code_t code, Action action, uint8_t arg);

    void handleAction_(Action action, uint8_t arg, lv_obj_t* obj);
    void handleNumpadKey_(lv_obj_t* matrix);
    void handleFreqTap_(lv_obj_t* label);
    // Creates the dialog for the open message of messages_.
    void openMessage_();
    // Closes the open dialog and drops queued ones about the SI5351 connection.
    void dismissLinkMessages_();
    void requestNavigate_(PageId page);

    static Gui* from_(lv_obj_t* obj);
    static void eventCb_(lv_event_t* e);
    static void msgboxEventCb_(lv_event_t* e);
    static void asyncNavigateCb_(void* gui);
    static void nextMessageCb_(void* gui);
    // Special modes of channel ch (1..3) into a label, colored by tone (U-2).
    // extra and note are appended with " | "; alone replaces extra when there are no modes.
    // The line is built in a buffer and set once: a LONG_DOT label shortens its own text to "..."
    // while its width is not laid out yet, so it must never be read back and extended.
    void showModes_(lv_obj_t* label, uint8_t ch, const char* extra = "", const char* alone = nullptr,
                    const char* note = "");
    // State label (MAIN cards, header): padding and corners once, text and fill per refresh.
    static void styleState_(lv_obj_t* label);
    static void showState_(lv_obj_t* label, const StatusText::Text& state);
    // Off state High needs a confirmation: a switched-off output then carries DC.
    // Confirmation dialogs with Cancel and one action button in the warning color.
    enum class Confirm : uint8_t {
        OffStateHigh,
        ResetDefaults,
        FactoryReset,
        DeleteNetwork,
        OpenNetwork,
        ApOpen,
        OpenFallback,
        WebNetwork,
        RemoveWebPassword,
        WebPending
    };
    static constexpr uint8_t CONFIRM_COUNT = (uint8_t)Confirm::WebPending + 1;
    // One row of the confirmation table (AF49a): title, text (text1 instead for payload != 0, if
    // set), buttons {cancel, action, ""} and what the answer does (yes = the action button).
    struct ConfirmSpec {
        const char* title;
        const char* text;
        const char* text1;
        const char* const* buttons;
        void (Gui::*answer)(bool yes, uint8_t payload);           // or:
        void (WifiPages::*wifiAnswer)(bool yes, uint8_t payload); // the WIFI confirmations
    };
    static const ConfirmSpec CONFIRMS_[CONFIRM_COUNT];
    void answerOffStateHigh_(bool yes, uint8_t);
    void answerResetDefaults_(bool yes, uint8_t);
    void answerFactoryReset_(bool yes, uint8_t);
    // Queues the question (ConfirmQueue: one open, the rest waits) and shows it when it is its
    // turn; payload: the network slot for DeleteNetwork.
    void confirm_(Confirm what, uint8_t payload = 0);
    // Creates the box of the open confirmation unless it exists or the calibration is shown.
    void showConfirm_();
    // The question is no longer needed (fallback AP, web change): closed without an action.
    void dropConfirm_(Confirm what);
    static void confirmEventCb_(lv_event_t* e);
    static void nextConfirmCb_(void* gui);
    // Screen lock events from the touch callback (hint while pressed, LOCKED in the status bar).
    void handleLockEvent_(InputLogic::ScreenLock::Event e);
    static void flushCb_(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    static void touchCb_(lv_indev_drv_t* drv, lv_indev_data_t* data);

    GuiServices services_;
    WifiPages wifiPages_ {*this, services_}; // the WIFI pages (AF49b)
    DisplayPanel* display_ = nullptr;
    TouchDriver* touch_ = nullptr;
    AppController* app_ = nullptr;
    PageId currentPage_ = PageId::Main;
    PageId pendingPage_ = PageId::Main;
    bool navPending_ = false;
    uint8_t channel_ = 1;       // channel shown on the CH1/CH2 page and edited by the numpad
    uint8_t corrStepIdx_ = 1;   // index into CORRECTION_STEPS
    uint8_t configTab_ = 0;     // CONFIG tab shown on the next visit; RAM only
    uint32_t lastUpdateMs_ = 0;
    uint32_t lastRevision_ = 0;   // AppController::revision() of the last redraw
    uint16_t shownRotation_ = 0;  // rotation applied to display and touch (degrees)
    uint16_t lastConnections_ = 0;
    StatusView lastView_ {};
    int8_t rssiShown_ = 0;          // CONFIG info card, with hysteresis
    bool heapSampled_ = false;      // info card heap line, sampled every 5 s (AF12)
    uint32_t heapSampleMs_ = 0;
    uint32_t heapFree_ = 0, heapMin_ = 0, heapBlock_ = 0, lvglFree_ = 0; // GuiServices::heapInfo, LVGL
    uint32_t refreshCount_ = 0;     // debug build: redraws and flushes per minute
    uint32_t flushCount_ = 0;
    uint32_t countLogMs_ = 0;
    char freqText_[24] {};      // plain frequency text on the channel page, for hit testing
    InputLogic::Numpad numpad_; // numpad input (InputLogic, host-tested)
    Widgets w_ {};
    lv_obj_t* root_ = nullptr;
    lv_obj_t* msgbox_ = nullptr;   // dialog that can still be tapped or closed
    lv_obj_t* shownBox_ = nullptr; // box of the open message, also while it closes

    MessageQueue messages_;     // open dialog and waiting ones (host-tested)
    TouchCal::Wizard calWizard_; // touch calibration (host-tested)
    bool calFirstStart_ = false;
    bool numpadPpm_ = false;     // numpad opened from CONFIG: correction entry (U-11)
    bool numpadOffset_ = false;  // numpad for a coupled CH2: offset entry with sign (E-213)
    PageId numpadReturn_ = PageId::CH1; // page the frequency numpad returns to
    uint32_t pressStartMs_ = 0;  // start of the current press of a repeat button (U-20)
    uint32_t lastStepMs_ = 0;    // last step of the current hold (slow phase, U-20)
    lv_event_code_t lastCode_ = LV_EVENT_ALL;
    uint8_t calShownTarget_ = 0xFF;
    TouchCal::Phase calShownPhase_ = TouchCal::Phase::Touch;
    InputLogic::ScreenLock lock_; // RAM only: a restart unlocks
    lv_obj_t* lockHint_ = nullptr; // on the top layer, shown while a locked screen is touched
    ConfirmQueue confirms_;          // open confirmation and waiting ones (host-tested)
    lv_obj_t* confirmBox_ = nullptr; // box of the open confirmation; user data = its generation

    lv_disp_draw_buf_t drawBuf_;
    static lv_color_t buf1_[SCREEN_WIDTH * 24];
    lv_disp_drv_t dispDrv_;
    lv_indev_drv_t indevDrv_;
};

