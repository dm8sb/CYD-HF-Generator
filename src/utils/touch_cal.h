#pragma once

#include <stdint.h>
#include <types.h>

// Touch calibration (audit U-26): the fixed calibration put the hits up to 19 px off on both
// boards. Four crosses near the corners give the raw range per axis, a cross in the middle
// checks it. Pure functions and a wizard state machine, host-tested in test_touch_cal; the
// GUI only shows the crosses and feeds raw samples.
namespace TouchCal {

struct Point {
    int16_t x, y;
};
struct RawPoint {
    int16_t x, y;
};

// Crosses this far from the edges: top left, top right, bottom right, bottom left, center.
static constexpr int16_t INSET = 20;
static constexpr uint8_t POINT_COUNT = 5;
extern const Point TARGETS[POINT_COUNT];

// Hold a cross this long; the first SETTLE_MS are not averaged (finger still moving).
static constexpr uint32_t HOLD_MS = 500;
static constexpr uint32_t SETTLE_MS = 100;
static constexpr uint32_t DROPOUT_MS = 60;   // shorter pressure dropouts keep the press
static constexpr uint32_t RELEASE_MS = 150;  // released after this long without touch
static constexpr int16_t CHECK_TOLERANCE_PX = 8;
static constexpr uint32_t CAL_TIMEOUT_MS = 30000;

// Raw range per axis from the four corner points (TARGETS[0..3]); false if they do not form a
// rectangle (a slipped finger, swapped axes, no range).
bool compute(const RawPoint corners[4], TouchCalibration* out);
// A stored calibration is usable: both axes have a range of at least 1500 raw counts and the
// limits are within -4000..8000.
bool plausible(const TouchCalibration& cal);

enum class Phase : uint8_t { Touch, Release, Done, Failed };
enum class Failure : uint8_t { None, Inconsistent, Check };

class Wizard {
public:
    void start(uint32_t nowMs);
    // One raw read: touched = the controller reports a plausible touch.
    void update(bool touched, int16_t rawX, int16_t rawY, uint32_t nowMs);
    Phase phase() const { return phase_; }
    uint8_t target() const { return target_; }
    Failure failure() const { return failure_; }
    const TouchCalibration& result() const { return cal_; }
    // Deviation of the check cross with the new calibration (Done and Failed::Check).
    int16_t checkDx() const { return dx_; }
    int16_t checkDy() const { return dy_; }
    // No touch for CAL_TIMEOUT_MS.
    bool timedOut(uint32_t nowMs) const { return nowMs - lastActivityMs_ >= CAL_TIMEOUT_MS; }

private:
    void finish_();

    Phase phase_ = Phase::Touch;
    Failure failure_ = Failure::None;
    uint8_t target_ = 0;
    bool pressing_ = false;
    uint32_t pressStartMs_ = 0;
    uint32_t lastTouchMs_ = 0;
    uint32_t lastActivityMs_ = 0;
    int32_t sumX_ = 0;
    int32_t sumY_ = 0;
    uint16_t count_ = 0;
    RawPoint raw_[POINT_COUNT] {};
    TouchCalibration cal_ {};
    int16_t dx_ = 0;
    int16_t dy_ = 0;
};

}
