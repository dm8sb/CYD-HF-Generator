// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "touch_test.h"

#include <stdio.h>
#include <stdlib.h>

namespace TouchTest {

bool Recorder::update(bool rawTouched, int16_t z, bool pressed, int16_t x, int16_t y, uint32_t nowMs, Record* out) {
    if (pressed) {
        if (!active_) {
            active_ = true;
            inDropout_ = false;
            startMs_ = nowMs;
            r_ = Record {};
            r_.x0 = x;
            r_.y0 = y;
            r_.gapBeforeMs = hadTouch_ ? nowMs - lastEndMs_ : 0;
        }
        if (!rawTouched) {
            // The filter bridges the dropout; it ends with the next touched read.
            if (!inDropout_) {
                inDropout_ = true;
                dropoutStart_ = nowMs;
            }
        } else {
            if (inDropout_) {
                inDropout_ = false;
                const uint32_t len = nowMs - dropoutStart_;
                ++r_.dropouts;
                if (len > r_.maxDropoutMs) r_.maxDropoutMs = len;
            }
            if (z > r_.zMax) r_.zMax = z;
            r_.x1 = x;
            r_.y1 = y;
            lastPressedMs_ = nowMs;
        }
        return false;
    }
    if (!active_) return false;
    active_ = false;
    hadTouch_ = true;
    lastEndMs_ = lastPressedMs_;
    r_.durationMs = lastPressedMs_ - startMs_;
    if (out) *out = r_;
    return true;
}

void formatRecord(const Record& r, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    snprintf(out, outSize, "x=%d y=%d to %d,%d z=%d t=%lums gap=%lums drop=%u/%lums", r.x0, r.y0, r.x1, r.y1, r.zMax,
             (unsigned long)r.durationMs, (unsigned long)r.gapBeforeMs, (unsigned)r.dropouts,
             (unsigned long)r.maxDropoutMs);
}

const Point TARGETS[TARGET_COUNT] = {
    // corners (the touch mapping clamps to 0 / 319 / 239)
    {0, 0}, {319, 0}, {0, 239}, {319, 239},
    // 4 px from the edges: corners
    {4, 4}, {315, 4}, {4, 235}, {315, 235},
    // 4 px from the edges: edge centers
    {160, 4}, {160, 235}, {4, 120}, {315, 120},
    // inside
    {80, 60}, {160, 60}, {240, 60}, {80, 120}, {240, 120}, {80, 180}, {160, 180}, {240, 180},
};

uint8_t nearestTarget(int16_t x, int16_t y, int16_t* dx, int16_t* dy) {
    uint8_t best = 0;
    int32_t bestD = 0x7FFFFFFF;
    for (uint8_t i = 0; i < TARGET_COUNT; ++i) {
        const int32_t ex = x - TARGETS[i].x;
        const int32_t ey = y - TARGETS[i].y;
        const int32_t d = ex * ex + ey * ey;
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    if (dx) *dx = (int16_t)(x - TARGETS[best].x);
    if (dy) *dy = (int16_t)(y - TARGETS[best].y);
    return best;
}

}
