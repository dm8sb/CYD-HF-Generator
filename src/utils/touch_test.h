#pragma once

#include <stddef.h>
#include <stdint.h>

// Touch test builds (MESSPLAN B8, B9, B12): one record per touch for the touch log and the
// targets of the hit map. Pure functions, host-tested in test_touch_filter; the firmware uses
// them only with TEST_TOUCH_LOG / TEST_HIT_MAP (config.h), never in the release build.
namespace TouchTest {

// One touch from the first to the last pressed read of the input device.
struct Record {
    int16_t x0, y0;        // first filtered point
    int16_t x1, y1;        // last filtered point (LVGL releases here)
    int16_t zMax;          // highest pressure
    uint32_t durationMs;   // first to last pressed read
    uint32_t gapBeforeMs;  // since the previous touch ended; a short gap is a dropout > bridge
    uint16_t dropouts;     // bridged pressure dropouts inside the touch
    uint32_t maxDropoutMs; // longest bridged dropout
};

class Recorder {
public:
    // Per read: rawTouched = the controller reports a touch, pressed/x/y = filter output.
    // Returns true when a touch ended; *out then holds it.
    bool update(bool rawTouched, int16_t z, bool pressed, int16_t x, int16_t y, uint32_t nowMs, Record* out);

private:
    bool active_ = false;
    bool inDropout_ = false;
    bool hadTouch_ = false;
    uint32_t dropoutStart_ = 0;
    uint32_t lastPressedMs_ = 0;
    uint32_t lastEndMs_ = 0;
    uint32_t startMs_ = 0;
    Record r_ {};
};

// "x=120 y=45 to 121,46 z=812 t=134ms gap=820ms drop=1/18ms" (key=value for the evaluation).
void formatRecord(const Record& r, char* out, size_t outSize);

struct Point {
    int16_t x, y;
};
// Hit map: the four corners, 4 px from the edges (corners and edge centers) and 8 inside.
static constexpr uint8_t TARGET_COUNT = 20;
extern const Point TARGETS[TARGET_COUNT];
// Index of the target nearest to (x, y); dx, dy = hit minus target.
uint8_t nearestTarget(int16_t x, int16_t y, int16_t* dx, int16_t* dy);

}
