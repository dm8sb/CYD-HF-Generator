// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>
#include <types.h>

// Touch processing without the XPT2046 library or LVGL (audit T-14): calibration mapping,
// plausibility of raw values, jitter filter and bridging of short pressure dropouts.
// TouchDriver feeds it with the library samples.
namespace TouchFilter {

// A missing or defective controller reads saturated values (0 / 4095) that would look like a
// permanent press.
bool plausibleRaw(int16_t rawX, int16_t rawY);
// Raw values to screen coordinates (clamped); false for an unusable calibration.
bool mapPoint(const TouchCalibration& cal, int16_t rawX, int16_t rawY, int16_t* x, int16_t* y);
// Screen turned by 180 degrees (E-53): x' = 319 - x, y' = 239 - y. The calibration stays in raw
// coordinates of the upright screen, so it serves both rotations.
void rotate180(int16_t* x, int16_t* y);

struct Sample {
    bool touched;   // the controller reports a touch
    int16_t rawX;
    int16_t rawY;
    int16_t z;      // pressure
};

struct Output {
    bool pressed;
    int16_t x;
    int16_t y;
    bool calibrationError; // logged by the driver
};

class Filter {
public:
    // rotated: the screen is turned by 180 degrees; positions are mirrored after the calibration.
    explicit Filter(const TouchCalibration& cal, bool rotated = false) : cal_(cal), rotated_(rotated) {}
    // One read of the input device at time nowMs.
    Output update(const Sample& s, uint32_t nowMs);

private:
    TouchCalibration cal_;
    bool rotated_;
    bool lastPressed_ = false;
    int16_t x_ = 0;
    int16_t y_ = 0;
    uint32_t lastTouchMs_ = 0;
};

}
