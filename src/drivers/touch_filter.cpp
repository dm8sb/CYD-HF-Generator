#include "touch_filter.h"

#include <constants.h>

namespace TouchFilter {

// Below this pressure a sample counts as released (the library threshold is higher, so this
// only guards against a stale sample).
static constexpr int16_t MIN_PRESSURE = 150;

bool plausibleRaw(int16_t rawX, int16_t rawY) { return rawX > 0 && rawX < 4095 && rawY > 0 && rawY < 4095; }

// Like Arduino map() with long arithmetic.
static long mapRange(long v, long inMin, long inMax, long outMin, long outMax) {
    return (v - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

bool mapPoint(const TouchCalibration& cal, int16_t rawX, int16_t rawY, int16_t* x, int16_t* y) {
    if (cal.x_max == cal.x_min || cal.y_max == cal.y_min) return false;
    long mx = mapRange(rawX, cal.x_min, cal.x_max, 0, SCREEN_WIDTH - 1);
    long my = mapRange(rawY, cal.y_min, cal.y_max, 0, SCREEN_HEIGHT - 1);
    if (mx < 0) mx = 0;
    if (mx >= SCREEN_WIDTH) mx = SCREEN_WIDTH - 1;
    if (my < 0) my = 0;
    if (my >= SCREEN_HEIGHT) my = SCREEN_HEIGHT - 1;
    *x = (int16_t)mx;
    *y = (int16_t)my;
    return true;
}

bool bootGestureHeld(uint8_t pressedSamples) { return pressedSamples >= BOOT_GESTURE_MIN_HELD; }

Output Filter::update(const Sample& s, uint32_t nowMs) {
    Output o {false, x_, y_, false};
    if (!s.touched) {
        // A short pressure dropout keeps the press, so a slide or hold is not cut into taps.
        if (lastPressed_ && nowMs - lastTouchMs_ < TOUCH_RELEASE_BRIDGE_MS) {
            o.pressed = true;
            return o;
        }
        lastPressed_ = false;
        return o;
    }
    if (s.z < MIN_PRESSURE) return o;
    int16_t x = 0, y = 0;
    if (!mapPoint(cal_, s.rawX, s.rawY, &x, &y)) {
        o.calibrationError = true;
        return o;
    }
    // IIR jitter filter; a new press starts at its own position.
    if (!lastPressed_) {
        x_ = x;
        y_ = y;
    } else {
        x_ = (int16_t)((3 * x_ + x) / 4);
        y_ = (int16_t)((3 * y_ + y) / 4);
    }
    lastTouchMs_ = nowMs;
    lastPressed_ = true;
    o.pressed = true;
    o.x = x_;
    o.y = y_;
    return o;
}

}
