// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "touch_cal.h"

#include <constants.h>
#include <drivers/touch_filter.h>
#include <stdlib.h>

namespace TouchCal {

const Point TARGETS[POINT_COUNT] = {
    {INSET, INSET},
    {SCREEN_WIDTH - 1 - INSET, INSET},
    {SCREEN_WIDTH - 1 - INSET, SCREEN_HEIGHT - 1 - INSET},
    {INSET, SCREEN_HEIGHT - 1 - INSET},
    {SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2},
};

// Between the crosses the panel spans about 3000 raw counts per axis; far less means the
// corners were not tapped.
static constexpr int32_t MIN_SPAN = 1000;
static constexpr int32_t MIN_RANGE = 1500;

bool compute(const RawPoint c[4], TouchCalibration* out) {
    // Order: top left, top right, bottom right, bottom left.
    const int32_t left = (c[0].x + c[3].x) / 2;
    const int32_t right = (c[1].x + c[2].x) / 2;
    const int32_t top = (c[0].y + c[1].y) / 2;
    const int32_t bottom = (c[3].y + c[2].y) / 2;
    const int32_t spanX = right - left;
    const int32_t spanY = bottom - top;
    if (labs(spanX) < MIN_SPAN || labs(spanY) < MIN_SPAN) return false;
    // The two crosses of a side must agree (a slipped finger, swapped axes).
    const int32_t tolX = labs(spanX) / 8;
    const int32_t tolY = labs(spanY) / 8;
    if (labs(c[0].x - c[3].x) > tolX || labs(c[1].x - c[2].x) > tolX) return false;
    if (labs(c[0].y - c[1].y) > tolY || labs(c[3].y - c[2].y) > tolY) return false;
    if (!out) return true;
    // Extend from the crosses to the screen edges (mapPoint maps x_min..x_max to 0..width-1).
    const float sx = (float)spanX / (float)(SCREEN_WIDTH - 1 - 2 * INSET);
    const float sy = (float)spanY / (float)(SCREEN_HEIGHT - 1 - 2 * INSET);
    out->x_min = (int32_t)((float)left - INSET * sx + (sx > 0 ? 0.5f : -0.5f));
    out->x_max = (int32_t)((float)right + INSET * sx + (sx > 0 ? 0.5f : -0.5f));
    out->y_min = (int32_t)((float)top - INSET * sy + (sy > 0 ? 0.5f : -0.5f));
    out->y_max = (int32_t)((float)bottom + INSET * sy + (sy > 0 ? 0.5f : -0.5f));
    return plausible(*out);
}

bool plausible(const TouchCalibration& cal) {
    const int32_t v[4] = {cal.x_min, cal.x_max, cal.y_min, cal.y_max};
    for (int32_t x : v) {
        if (x < -4000 || x > 8000) return false;
    }
    return labs(cal.x_max - cal.x_min) >= MIN_RANGE && labs(cal.y_max - cal.y_min) >= MIN_RANGE;
}

void Wizard::start(uint32_t nowMs) {
    *this = Wizard {};
    lastActivityMs_ = nowMs;
    lastTouchMs_ = nowMs;
}

void Wizard::update(bool touched, int16_t rawX, int16_t rawY, uint32_t nowMs) {
    if (touched) lastActivityMs_ = nowMs;
    switch (phase_) {
        case Phase::Touch:
            if (touched) {
                if (!pressing_) {
                    pressing_ = true;
                    pressStartMs_ = nowMs;
                    sumX_ = sumY_ = 0;
                    count_ = 0;
                }
                lastTouchMs_ = nowMs;
                if (nowMs - pressStartMs_ >= SETTLE_MS) {
                    sumX_ += rawX;
                    sumY_ += rawY;
                    ++count_;
                }
                if (nowMs - pressStartMs_ >= HOLD_MS && count_ >= 5) {
                    raw_[target_] = RawPoint {(int16_t)(sumX_ / count_), (int16_t)(sumY_ / count_)};
                    phase_ = Phase::Release;
                }
            } else if (pressing_ && nowMs - lastTouchMs_ > DROPOUT_MS) {
                pressing_ = false; // released too early: this cross again
            }
            break;
        case Phase::Release:
            if (touched) {
                lastTouchMs_ = nowMs;
            } else if (nowMs - lastTouchMs_ >= RELEASE_MS) {
                pressing_ = false;
                if (++target_ == POINT_COUNT) finish_();
                else phase_ = Phase::Touch;
            }
            break;
        case Phase::Done:
        case Phase::Failed:
            break;
    }
}

void Wizard::finish_() {
    if (!compute(raw_, &cal_)) {
        phase_ = Phase::Failed;
        failure_ = Failure::Inconsistent;
        return;
    }
    int16_t mx = 0, my = 0;
    TouchFilter::mapPoint(cal_, raw_[4].x, raw_[4].y, &mx, &my);
    dx_ = (int16_t)(mx - TARGETS[4].x);
    dy_ = (int16_t)(my - TARGETS[4].y);
    const bool ok = abs(dx_) <= CHECK_TOLERANCE_PX && abs(dy_) <= CHECK_TOLERANCE_PX;
    phase_ = ok ? Phase::Done : Phase::Failed;
    failure_ = ok ? Failure::None : Failure::Check;
}

}
