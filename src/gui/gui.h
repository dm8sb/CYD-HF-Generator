#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <constants.h>
#include <core/app_controller.h>
#include <drivers/touch.h>

// Pages are built once per navigation. Actions change the AppController state and then
// refresh_() updates the existing widgets, so no widget is deleted inside its own event.
class Gui {
public:
    bool begin(AppController& app, TouchDriver& touch);
    void update();
    // One dialog at a time; further messages are queued, identical ones dropped.
    void showMessage(const char* title, const char* text);
    void navigate(PageId page);

private:
    enum class Action : uint8_t {
        Navigate,       // arg: PageId
        ChannelSwitch,  // arg: channel 1..3
        FreqUp,
        FreqDown,
        FreqTap,        // tap on the frequency: digit selects the step, unit opens the numpad
        SetStep,        // arg: DELTA_STEPS index
        StepUp,
        StepDown,
        Ch3Master,      // arg: 1/2
        Ch3Phase,       // arg: phase / 90
        CorrUp,
        CorrDown,
        CorrStepCycle,
        Backlight,
        Drive,
        NumpadKey
    };

    struct Widgets {
        lv_obj_t* hdrSwitch;
        lv_obj_t* hdrState;
        lv_obj_t* freq;
        lv_obj_t* info;
        lv_obj_t* chips[DELTA_STEP_COUNT];
        lv_obj_t* masterChips[2];
        lv_obj_t* cardFreq[3];
        lv_obj_t* cardState[3];
        lv_obj_t* cardDetail[3];
        lv_obj_t* cardSwitch[3];
        lv_obj_t* blValue;
        lv_obj_t* stepValue;
        lv_obj_t* corrValue;
        lv_obj_t* corrStep;
        lv_obj_t* saveState;
        lv_obj_t* ta;
    };

    void createMain_();
    void createChannelPage_(uint8_t ch);
    void createCH3_();
    void createConfig_();
    void createNumpad_(uint8_t ch);

    void refresh_();
    void refreshMain_();
    void refreshChannel_();
    void refreshCH3_();
    void refreshConfig_();
    void refreshSaveState_();

    void createHeader_(const char* title, lv_color_t color, uint8_t switchChannel);
    void setHeaderState_(bool on);
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
    void openMessage_(const char* title, const char* text);
    void requestNavigate_(PageId page);

    static Gui* from_(lv_obj_t* obj);
    static void eventCb_(lv_event_t* e);
    static void msgboxEventCb_(lv_event_t* e);
    static void asyncNavigateCb_(void* gui);
    static void nextMessageCb_(void* gui);
    static void flushCb_(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    static void touchCb_(lv_indev_drv_t* drv, lv_indev_data_t* data);

    TFT_eSPI tft_;
    TouchDriver* touch_ = nullptr;
    AppController* app_ = nullptr;
    PageId currentPage_ = PageId::Main;
    PageId pendingPage_ = PageId::Main;
    bool navPending_ = false;
    uint8_t channel_ = 1;       // channel shown on the CH1/CH2 page and edited by the numpad
    uint8_t corrStepIdx_ = 1;   // index into CORRECTION_STEPS
    uint32_t lastUpdateMs_ = 0;
    bool lastHwReady_ = false;
    char freqText_[24] {};      // plain frequency text on the channel page, for hit testing
    Widgets w_ {};
    lv_obj_t* root_ = nullptr;
    lv_obj_t* msgbox_ = nullptr;

    struct Message {
        char title[24];
        char text[160];
    };
    static constexpr uint8_t MSG_QUEUE_LEN = 3;
    char msgText_[160] {};      // text of the open dialog
    Message msgQueue_[MSG_QUEUE_LEN] {};
    uint8_t msgCount_ = 0;

    lv_disp_draw_buf_t drawBuf_;
    static lv_color_t buf1_[SCREEN_WIDTH * 24];
    lv_disp_drv_t dispDrv_;
    lv_indev_drv_t indevDrv_;
};

