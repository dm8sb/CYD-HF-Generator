// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

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
