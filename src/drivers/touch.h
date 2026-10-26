#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <types.h>

class TouchDriver {
public:
    TouchDriver();
    bool begin(const TouchCalibration& cal);
    bool readLvgl(lv_indev_drv_t* drv, lv_indev_data_t* data);

private:
    bool mapPoint(int16_t rx, int16_t ry, lv_coord_t* x, lv_coord_t* y);

    SPIClass spi_;
    XPT2046_Touchscreen touch_;
    TouchCalibration cal_ {};
    bool lastPressed_ = false;
    lv_coord_t filtX_ = 0;
    lv_coord_t filtY_ = 0;
    uint32_t lastTouchMs_ = 0;
};
