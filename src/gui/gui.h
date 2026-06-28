#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <constants.h>
#include <core/app_controller.h>
#include <drivers/touch.h>

class Gui {
public:
    bool begin(AppController& app, TouchDriver& touch);
    void update();
    void showMessage(const char* title, const char* text);
    void navigate(PageId page, uint8_t selectedChannel = 0);

    static void btnEvent_(lv_event_t* e);
    static void sliderEvent_(lv_event_t* e);
    static void switchEvent_(lv_event_t* e);
    static void dropdownEvent_(lv_event_t* e);
    static void taEvent_(lv_event_t* e);

private:
    void createMain_();
    void createChannelPage_(uint8_t ch);
    void createCH3_();
    void createConfig_();
    void createNumpad_(uint8_t ch);
    void createHeader_(const char* title, lv_color_t color, bool showBack = true, bool showSwitch = false, const char* switchAction = nullptr, bool switchState = false);
    lv_obj_t* addButton_(lv_obj_t* parent, const char* txt, int x, int y, int w, int h, const char* action, lv_color_t color);
    lv_obj_t* addCard_(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t borderColor);
    void setLedForPage_(PageId page);
    void refreshCurrent_();
    

    static void flushCb_(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    static void touchCb_(lv_indev_drv_t* drv, lv_indev_data_t* data);
    TFT_eSPI tft_;
    TouchDriver* touch_ = nullptr;
    AppController* app_ = nullptr;
    PageId currentPage_ = PageId::Main;
    uint8_t selectedChannel_ = 0;

    lv_disp_draw_buf_t drawBuf_;
    static lv_color_t buf1_[SCREEN_WIDTH * 24];
    lv_disp_drv_t dispDrv_;
    lv_indev_drv_t indevDrv_;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* status_ = nullptr;

};

extern Gui GUI;
