#pragma once

#include <Arduino.h>

struct Channel {
    uint64_t frequency;
    bool enabled;
    uint8_t drive_strength; // mA: 2, 4, 6, 8
};

struct Channel3 {
    bool enabled;
    uint8_t master;     // 1 or 2
    uint16_t phase;     // 0/90/180/270
    uint8_t drive_strength; // mA: 2, 4, 6, 8
};

struct SystemSettings {
    uint32_t delta_step;
    int32_t freq_correction;
    uint8_t backlight;  // 0-100%
    uint8_t default_drive_strength;
};

struct TouchCalibration {
    int32_t x_min;
    int32_t x_max;
    int32_t y_min;
    int32_t y_max;
};

struct PersistentState {
    Channel ch1;
    Channel ch2;
    Channel3 ch3;
    SystemSettings sys;
    TouchCalibration touch_cal;
};

enum class PageId : uint8_t {
    Main,
    CH1,
    CH2,
    CH3,
    Config,
    Numpad
};
