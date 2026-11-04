#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <config.h>
#include <constants.h>
#include <core/app_controller.h>
#include <drivers/display_panel.h>
#include <drivers/touch.h>
#include <utils/input_logic.h>
#include <utils/message_queue.h>
#include <utils/status_text.h>
#include <utils/touch_cal.h>

// Pages are built once per navigation. Actions change the AppController state and then
// refresh_() updates the existing widgets, so no widget is deleted inside its own event.
class Gui {
public:
    // The display must already be initialized (DisplayPanel::begin()).
    void begin(AppController& app, TouchDriver& touch, DisplayPanel& display);
    void update();
    // Redraws the current page from the controller state (e.g. after the boot programming).
    void refresh() { refresh_(); }
    // One dialog at a time; further messages are queued, identical ones dropped.
    void showMessage(const char* title, const char* text);
    void navigate(PageId page);
    // Touch calibration wizard (U-26). firstStart: no calibration stored; a timeout continues
    // with the default calibration on MAIN (otherwise back to CONFIG, unchanged).
    void calibrateTouch(bool firstStart);
    // Saves a successful calibration (SettingsService, set by main).
    void setTouchCalibrationStore(bool (*store)(const TouchCalibration&)) { storeTouchCal_ = store; }

private:
    enum class Action : uint8_t {
        Navigate,       // arg: PageId
        ChannelSwitch,  // arg: channel 1..3
        FreqUp,
        FreqDown,
        FreqTap,        // tap on the frequency: digit selects the step, unit opens the numpad
        SetStep,        // arg: DELTA_STEPS index
        Ch3Master,      // arg: 1/2
        Ch3Phase,       // arg: phase / 90
        CorrUp,
        CorrDown,
        CorrStepCycle,
        BacklightDown,
        BacklightUp,
        Drive,          // dropdowns: the selected index is the value
        Tuning,
        Spread,
        OffState,
        NumpadKey,
        TouchCal        // CONFIG: start the touch calibration
    };

    struct Widgets {
        lv_obj_t* hdrSwitch;
        lv_obj_t* hdrState;
        lv_obj_t* freq;
        lv_obj_t* info;
        lv_obj_t* modes;       // special modes of the channel (U-2)
        lv_obj_t* chips[DELTA_STEP_COUNT];
        lv_obj_t* masterChips[2];
        lv_obj_t* cardFreq[3];
        lv_obj_t* cardState[3];
        lv_obj_t* cardDetail[3];
        lv_obj_t* cardSwitch[3];
        lv_obj_t* cfgList;      // scroll container of the CONFIG rows
        lv_obj_t* blValue;
        lv_obj_t* corrValue;
        lv_obj_t* corrStep;
        lv_obj_t* driveDd;
        lv_obj_t* tuningDd;
        lv_obj_t* spreadDd;
        lv_obj_t* offStateDd;
        lv_obj_t* si5351Info;
        lv_obj_t* saveState;   // CONFIG header, MAIN status bar
        lv_obj_t* ta;
        lv_obj_t* calCross[2]; // touch calibration: horizontal and vertical line
        lv_obj_t* calText;
        lv_obj_t* calStatus;
        lv_obj_t* touchInfo;   // CONFIG info card
        lv_obj_t* npError;     // numpad: why the last key was rejected (U-9)
    };

    void createMain_();
    void createChannelPage_(uint8_t ch);
    void createCH3_();
    void createConfig_();
    void createNumpad_(uint8_t ch);
    void createTouchCal_();
    // Feeds raw touch samples to the wizard and moves the cross (every loop on that page).
    void tickTouchCal_();
    void showCalTarget_();
#if TEST_HIT_MAP
    // Test build only (MESSPLAN B8): target crosses instead of the GUI.
    void createHitMap_();
    static void hitMapCb_(lv_event_t* e);
#endif

    void refresh_();
    void refreshMain_();
    void refreshChannel_();
    void refreshCH3_();
    void refreshConfig_();
    void refreshSaveState_();
    uint8_t hardwareState_() const;

    // Returns the header bar for page-specific additions.
    lv_obj_t* createHeader_(const char* title, lv_color_t color, uint8_t switchChannel);
    // Switch = requested state; the label shows ERR in red when the output failed.
    // Switch = request, text = actual state (as on MAIN).
    void setHeaderState_(bool on, const StatusText::Text& state);
    // w_.info = actual output frequency of ch 1..3; deviations from 1 Hz on in the warning color.
    // Out line of a channel page; prefix (e.g. "Off | ") goes before it (U-17).
    void showOutput_(uint8_t ch, uint64_t requested, const char* prefix = "");
    // CONFIG: a row card at position `index` of the list (title optional), and a row with a dropdown.
    lv_obj_t* addConfigRow_(lv_obj_t* list, uint8_t index, int h, const char* title);
    lv_obj_t* addOptionRow_(lv_obj_t* list, uint8_t index, const char* title, const char* options, Action action);
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
    void showModes_(lv_obj_t* label, uint8_t ch);
    // State label (MAIN cards, header): padding and corners once, text and fill per refresh.
    static void styleState_(lv_obj_t* label);
    static void showState_(lv_obj_t* label, const StatusText::Text& state);
    // Off state High needs a confirmation: a switched-off output then carries DC.
    void confirmOffStateHigh_();
    static void confirmEventCb_(lv_event_t* e);
    static void flushCb_(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    static void touchCb_(lv_indev_drv_t* drv, lv_indev_data_t* data);

    DisplayPanel* display_ = nullptr;
    TouchDriver* touch_ = nullptr;
    AppController* app_ = nullptr;
    PageId currentPage_ = PageId::Main;
    PageId pendingPage_ = PageId::Main;
    bool navPending_ = false;
    uint8_t channel_ = 1;       // channel shown on the CH1/CH2 page and edited by the numpad
    uint8_t corrStepIdx_ = 1;   // index into CORRECTION_STEPS
    lv_coord_t configScrollY_ = 0; // CONFIG is rebuilt on every visit; RAM only
    uint32_t lastUpdateMs_ = 0;
    uint8_t lastHwState_ = 0;
    uint16_t lastConnections_ = 0;
    char freqText_[24] {};      // plain frequency text on the channel page, for hit testing
    InputLogic::Numpad numpad_; // numpad input (InputLogic, host-tested)
    Widgets w_ {};
    lv_obj_t* root_ = nullptr;
    lv_obj_t* msgbox_ = nullptr;   // dialog that can still be tapped or closed
    lv_obj_t* shownBox_ = nullptr; // box of the open message, also while it closes

    MessageQueue messages_;     // open dialog and waiting ones (host-tested)
    TouchCal::Wizard calWizard_; // touch calibration (host-tested)
    bool calFirstStart_ = false;
    uint8_t calShownTarget_ = 0xFF;
    TouchCal::Phase calShownPhase_ = TouchCal::Phase::Touch;
    bool (*storeTouchCal_)(const TouchCalibration&) = nullptr;

    lv_disp_draw_buf_t drawBuf_;
    static lv_color_t buf1_[SCREEN_WIDTH * 24];
    lv_disp_drv_t dispDrv_;
    lv_indev_drv_t indevDrv_;
};

