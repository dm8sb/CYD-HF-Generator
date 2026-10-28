#pragma once

#include <Arduino.h>

class BacklightDriver {
public:
    // Starts with the backlight off.
    void begin();
    void setPercent(uint8_t percent);
private:
    uint8_t percent_ = 0;
};
