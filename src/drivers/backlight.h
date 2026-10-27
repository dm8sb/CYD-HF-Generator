#pragma once

#include <Arduino.h>

class BacklightDriver {
public:
    // Starts with the backlight off.
    void begin();
    void setPercent(uint8_t percent);
    uint8_t percent() const { return percent_; }
private:
    uint8_t percent_ = 0;
};
