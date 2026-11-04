#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <types.h>
#include "touch_filter.h"

class TouchDriver {
public:
    TouchDriver();
    bool begin(const TouchCalibration& cal);
    void readLvgl(lv_indev_drv_t* drv, lv_indev_data_t* data);
    // Pressed with a plausible raw position, e.g. for the boot gesture. A missing or defective
    // touch controller reads saturated values that would look like a permanent press.
    bool pressed();

private:
    SPIClass spi_;
    XPT2046_Touchscreen touch_;
    // Calibration, plausibility, jitter filter and dropout bridge (host-tested).
    TouchFilter::Filter filter_ {TouchCalibration {}};
};
