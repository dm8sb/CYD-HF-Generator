// Touch calibration (audit U-26): wizard steps, calculation from four crosses near the
// corners, plausibility and the check cross in the middle.

#include <unity.h>
#include <constants.h>
#include <drivers/touch_filter.h>
#include <utils/touch_cal.h>
#include <Preferences.h>
#include <services/settings.h>

using namespace TouchCal;

void setUp() { Fake::nvs().reset(); }
void tearDown() {}

// A panel model: raw = offset + screen * slope (as measured on the original CYD: the fixed
// calibration put the hits 11-19 px too far left).
struct Model {
    float x0, sx, y0, sy;
    int16_t rawX(int16_t x) const { return (int16_t)(x0 + sx * x + 0.5f); }
    int16_t rawY(int16_t y) const { return (int16_t)(y0 + sy * y + 0.5f); }
};
static const Model CYD {520.0f, 10.6f, 330.0f, 14.3f};

// Holds each target for holdMs (10 ms reads), releases for 200 ms.
static void tapAll(Wizard& w, const Model& m, uint32_t& t, uint32_t holdMs = 600, int16_t errX = 0) {
    for (uint8_t i = 0; i < POINT_COUNT && w.phase() != Phase::Done && w.phase() != Phase::Failed; ++i) {
        const Point p = TARGETS[w.target()];
        const int16_t ex = w.target() == POINT_COUNT - 1 ? errX : 0;
        for (uint32_t k = 0; k < holdMs; k += 10, t += 10) w.update(true, m.rawX(p.x + ex), m.rawY(p.y), t);
        for (uint32_t k = 0; k < 200; k += 10, t += 10) w.update(false, 0, 0, t);
    }
}

static void test_targets_near_the_corners_and_center() {
    TEST_ASSERT_EQUAL_UINT8(5, POINT_COUNT);
    TEST_ASSERT_EQUAL_INT16(INSET, TARGETS[0].x);
    TEST_ASSERT_EQUAL_INT16(SCREEN_WIDTH - 1 - INSET, TARGETS[1].x);
    TEST_ASSERT_EQUAL_INT16(SCREEN_HEIGHT - 1 - INSET, TARGETS[2].y);
    TEST_ASSERT_EQUAL_INT16(SCREEN_WIDTH / 2, TARGETS[4].x);
    TEST_ASSERT_EQUAL_INT16(SCREEN_HEIGHT / 2, TARGETS[4].y);
}

static void test_compute_maps_the_targets_back() {
    RawPoint raw[4];
    for (uint8_t i = 0; i < 4; ++i) raw[i] = RawPoint {CYD.rawX(TARGETS[i].x), CYD.rawY(TARGETS[i].y)};
    TouchCalibration cal {};
    TEST_ASSERT_TRUE(compute(raw, &cal));
    TEST_ASSERT_TRUE(plausible(cal));
    // Every screen position comes back within 1 px, the corners included.
    const int16_t xs[] = {0, 20, 80, 160, 240, 299, 319};
    const int16_t ys[] = {0, 20, 120, 219, 239};
    for (int16_t x : xs) {
        for (int16_t y : ys) {
            int16_t mx = 0, my = 0;
            TEST_ASSERT_TRUE(TouchFilter::mapPoint(cal, CYD.rawX(x), CYD.rawY(y), &mx, &my));
            TEST_ASSERT_INT16_WITHIN(1, x, mx);
            TEST_ASSERT_INT16_WITHIN(1, y, my);
        }
    }
}

static void test_compute_handles_an_inverted_axis() {
    const Model inverted {3900.0f, -11.0f, 330.0f, 14.3f};
    RawPoint raw[4];
    for (uint8_t i = 0; i < 4; ++i) raw[i] = RawPoint {inverted.rawX(TARGETS[i].x), inverted.rawY(TARGETS[i].y)};
    TouchCalibration cal {};
    TEST_ASSERT_TRUE(compute(raw, &cal));
    int16_t mx = 0, my = 0;
    TouchFilter::mapPoint(cal, inverted.rawX(80), inverted.rawY(60), &mx, &my);
    TEST_ASSERT_INT16_WITHIN(1, 80, mx);
    TEST_ASSERT_INT16_WITHIN(1, 60, my);
}

static void test_compute_rejects_inconsistent_points() {
    RawPoint raw[4];
    for (uint8_t i = 0; i < 4; ++i) raw[i] = RawPoint {CYD.rawX(TARGETS[i].x), CYD.rawY(TARGETS[i].y)};
    TouchCalibration cal {};
    // One corner tapped far off (the finger slipped to the middle).
    RawPoint bad[4] = {raw[0], raw[1], raw[2], RawPoint {CYD.rawX(160), CYD.rawY(120)}};
    TEST_ASSERT_FALSE(compute(bad, &cal));
    // All four at the same place: no range.
    RawPoint same[4] = {raw[0], raw[0], raw[0], raw[0]};
    TEST_ASSERT_FALSE(compute(same, &cal));
    // Axes swapped: x on the screen moves raw y.
    RawPoint swapped[4];
    for (uint8_t i = 0; i < 4; ++i) swapped[i] = RawPoint {raw[i].y, raw[i].x};
    TEST_ASSERT_FALSE(compute(swapped, &cal));
}

static void test_plausible_calibration() {
    TEST_ASSERT_TRUE(plausible(TOUCH_CAL_DEFAULT));
    TEST_ASSERT_FALSE(plausible(TouchCalibration {300, 400, 300, 3800}));   // range too small
    TEST_ASSERT_FALSE(plausible(TouchCalibration {300, 3800, 3800, 3800})); // no y range
    TEST_ASSERT_FALSE(plausible(TouchCalibration {-9000, 3800, 300, 3800}));
}

static void test_wizard_completes_with_a_good_check() {
    Wizard w;
    uint32_t t = 0;
    w.start(t);
    TEST_ASSERT_EQUAL(Phase::Touch, w.phase());
    TEST_ASSERT_EQUAL_UINT8(0, w.target());
    tapAll(w, CYD, t, 600, 3);
    TEST_ASSERT_EQUAL(Phase::Done, w.phase());
    TEST_ASSERT_INT16_WITHIN(1, 3, w.checkDx());
    int16_t mx = 0, my = 0;
    TouchFilter::mapPoint(w.result(), CYD.rawX(300), CYD.rawY(10), &mx, &my);
    TEST_ASSERT_INT16_WITHIN(1, 300, mx);
    TEST_ASSERT_INT16_WITHIN(1, 10, my);
}

static void test_wizard_fails_when_the_check_is_off() {
    Wizard w;
    uint32_t t = 0;
    w.start(t);
    tapAll(w, CYD, t, 600, CHECK_TOLERANCE_PX + 4);
    TEST_ASSERT_EQUAL(Phase::Failed, w.phase());
    TEST_ASSERT_EQUAL(Failure::Check, w.failure());
}

static void test_wizard_needs_a_held_touch() {
    Wizard w;
    uint32_t t = 0;
    w.start(t);
    // A short tap (200 ms) does not count; the target stays.
    for (uint32_t k = 0; k < 200; k += 10, t += 10) w.update(true, CYD.rawX(20), CYD.rawY(20), t);
    for (uint32_t k = 0; k < 200; k += 10, t += 10) w.update(false, 0, 0, t);
    TEST_ASSERT_EQUAL(Phase::Touch, w.phase());
    TEST_ASSERT_EQUAL_UINT8(0, w.target());
    // Held with a 40 ms pressure dropout: counts, then waits for the release.
    for (uint32_t k = 0; k < 300; k += 10, t += 10) w.update(true, CYD.rawX(20), CYD.rawY(20), t);
    for (uint32_t k = 0; k < 40; k += 10, t += 10) w.update(false, 0, 0, t);
    for (uint32_t k = 0; k < 300; k += 10, t += 10) w.update(true, CYD.rawX(20), CYD.rawY(20), t);
    TEST_ASSERT_EQUAL(Phase::Release, w.phase());
    for (uint32_t k = 0; k < 200; k += 10, t += 10) w.update(false, 0, 0, t);
    TEST_ASSERT_EQUAL(Phase::Touch, w.phase());
    TEST_ASSERT_EQUAL_UINT8(1, w.target());
}

static void test_wizard_times_out_without_touch() {
    Wizard w;
    w.start(1000);
    TEST_ASSERT_FALSE(w.timedOut(1000 + CAL_TIMEOUT_MS - 1));
    TEST_ASSERT_TRUE(w.timedOut(1000 + CAL_TIMEOUT_MS));
    // A touch restarts the timeout.
    w.update(true, CYD.rawX(20), CYD.rawY(20), 1000 + CAL_TIMEOUT_MS - 10);
    TEST_ASSERT_FALSE(w.timedOut(1000 + CAL_TIMEOUT_MS + 100));
}

// Stored per board in NVS (keys tc_x0, tc_x1, tc_y0, tc_y1).
static void test_calibration_is_stored_and_loaded() {
    SettingsService s;
    TEST_ASSERT_TRUE(s.begin());
    TouchCalibration c {};
    TEST_ASSERT_FALSE(s.loadTouchCalibration(&c)); // nothing stored: the wizard runs
    const TouchCalibration cal {500, 3900, 3700, 250};
    TEST_ASSERT_TRUE(s.saveTouchCalibration(cal));
    TEST_ASSERT_TRUE(s.loadTouchCalibration(&c));
    TEST_ASSERT_EQUAL_INT32(500, c.x_min);
    TEST_ASSERT_EQUAL_INT32(3900, c.x_max);
    TEST_ASSERT_EQUAL_INT32(3700, c.y_min);
    TEST_ASSERT_EQUAL_INT32(250, c.y_max);
    // A broken value is not used.
    Preferences p;
    p.putInt("tc_x1", 600);
    TEST_ASSERT_FALSE(s.loadTouchCalibration(&c));
}

static void test_calibration_without_nvs() {
    Fake::nvs().available = false;
    SettingsService s;
    TEST_ASSERT_FALSE(s.begin());
    TouchCalibration c {};
    TEST_ASSERT_FALSE(s.loadTouchCalibration(&c));
    TEST_ASSERT_FALSE(s.saveTouchCalibration(TOUCH_CAL_DEFAULT));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_targets_near_the_corners_and_center);
    RUN_TEST(test_compute_maps_the_targets_back);
    RUN_TEST(test_compute_handles_an_inverted_axis);
    RUN_TEST(test_compute_rejects_inconsistent_points);
    RUN_TEST(test_plausible_calibration);
    RUN_TEST(test_wizard_completes_with_a_good_check);
    RUN_TEST(test_wizard_fails_when_the_check_is_off);
    RUN_TEST(test_wizard_needs_a_held_touch);
    RUN_TEST(test_wizard_times_out_without_touch);
    RUN_TEST(test_calibration_is_stored_and_loaded);
    RUN_TEST(test_calibration_without_nvs);
    return UNITY_END();
}
