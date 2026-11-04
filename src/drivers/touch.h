#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <config.h>
#include <types.h>
#include "touch_filter.h"
#if TEST_TOUCH_LOG
#include <utils/touch_test.h>
#endif

class TouchDriver {
public:
    TouchDriver();
    // stored: cal comes from the NVS (touch calibration of this board, U-26).
    bool begin(const TouchCalibration& cal, bool stored);
    void setCalibration(const TouchCalibration& cal, bool stored);
    const TouchCalibration& calibration() const { return cal_; }
    bool calibrated() const { return calibrated_; }
    // Raw position of a plausible touch for the calibration wizard.
    bool readRaw(int16_t* rawX, int16_t* rawY);
    void readLvgl(lv_indev_drv_t* drv, lv_indev_data_t* data);
    // Pressed with a plausible raw position (a touch at boot). A missing or defective touch
    // controller reads saturated values that would look like a permanent press.
    bool pressed();

private:
    SPIClass spi_;
    XPT2046_Touchscreen touch_;
    // Calibration, plausibility, jitter filter and dropout bridge (host-tested).
    TouchFilter::Filter filter_ {TouchCalibration {}};
    TouchCalibration cal_ {};
    bool calibrated_ = false;
#if TEST_TOUCH_LOG
    TouchTest::Recorder recorder_;
#endif
};
