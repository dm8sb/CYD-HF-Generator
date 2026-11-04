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
    explicit Filter(const TouchCalibration& cal) : cal_(cal) {}
    // One read of the input device at time nowMs.
    Output update(const Sample& s, uint32_t nowMs);

private:
    TouchCalibration cal_;
    bool lastPressed_ = false;
    int16_t x_ = 0;
    int16_t y_ = 0;
    uint32_t lastTouchMs_ = 0;
};

}
