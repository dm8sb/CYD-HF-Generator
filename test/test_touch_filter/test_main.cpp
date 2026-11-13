// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Touch processing without the XPT2046 library (audit T-14): calibration, plausibility,
// jitter filter and dropout bridge; records and hit map targets of the touch test builds.

#include <unity.h>
#include <constants.h>
#include <drivers/touch_filter.h>
#include <string.h>
#include <utils/touch_test.h>

using namespace TouchFilter;

void setUp() {}
void tearDown() {}

static Sample touch(int16_t x, int16_t y, int16_t z = 1000) { return Sample {true, x, y, z}; }
static const Sample RELEASED {false, 0, 0, 0};

static void test_plausible_raw_values() {
    TEST_ASSERT_TRUE(plausibleRaw(2000, 2000));
    TEST_ASSERT_FALSE(plausibleRaw(0, 2000));      // saturated: controller missing or defective
    TEST_ASSERT_FALSE(plausibleRaw(2000, 4095));
    TEST_ASSERT_FALSE(plausibleRaw(4095, 4095));
}

static void test_map_point_corners_and_clamping() {
    int16_t x = -1, y = -1;
    TEST_ASSERT_TRUE(mapPoint(TOUCH_CAL_DEFAULT, 300, 300, &x, &y));
    TEST_ASSERT_EQUAL_INT16(0, x);
    TEST_ASSERT_EQUAL_INT16(0, y);
    TEST_ASSERT_TRUE(mapPoint(TOUCH_CAL_DEFAULT, 3800, 3800, &x, &y));
    TEST_ASSERT_EQUAL_INT16(SCREEN_WIDTH - 1, x);
    TEST_ASSERT_EQUAL_INT16(SCREEN_HEIGHT - 1, y);
    // Outside the calibration the point is clamped to the screen (corner taps, audit U-5).
    TEST_ASSERT_TRUE(mapPoint(TOUCH_CAL_DEFAULT, 100, 4000, &x, &y));
    TEST_ASSERT_EQUAL_INT16(0, x);
    TEST_ASSERT_EQUAL_INT16(SCREEN_HEIGHT - 1, y);
    const TouchCalibration broken {300, 300, 300, 3800};
    TEST_ASSERT_FALSE(mapPoint(broken, 1000, 1000, &x, &y));
}

static void test_filter_smooths_and_starts_at_the_new_press() {
    Filter f(TOUCH_CAL_DEFAULT);
    Output o = f.update(touch(300, 300), 0);
    TEST_ASSERT_TRUE(o.pressed);
    TEST_ASSERT_EQUAL_INT16(0, o.x);
    o = f.update(touch(3800, 300), 10); // jump: moves a quarter of the way
    TEST_ASSERT_EQUAL_INT16((SCREEN_WIDTH - 1) / 4, o.x);
    // Released for longer than the bridge, a new press starts at its own position.
    f.update(RELEASED, 20);
    o = f.update(RELEASED, 20 + TOUCH_RELEASE_BRIDGE_MS + 10);
    TEST_ASSERT_FALSE(o.pressed);
    o = f.update(touch(3800, 3800), 200);
    TEST_ASSERT_EQUAL_INT16(SCREEN_WIDTH - 1, o.x);
    TEST_ASSERT_EQUAL_INT16(SCREEN_HEIGHT - 1, o.y);
}

static void test_filter_bridges_short_dropouts() {
    Filter f(TOUCH_CAL_DEFAULT);
    f.update(touch(2000, 2000), 100);
    Output o = f.update(RELEASED, 100 + TOUCH_RELEASE_BRIDGE_MS - 1);
    TEST_ASSERT_TRUE(o.pressed); // still pressed, at the last position
    o = f.update(RELEASED, 100 + TOUCH_RELEASE_BRIDGE_MS);
    TEST_ASSERT_FALSE(o.pressed);
    // No bridge without a previous press.
    Filter g(TOUCH_CAL_DEFAULT);
    TEST_ASSERT_FALSE(g.update(RELEASED, 5).pressed);
    // Audit U-20: dropouts up to 60-80 ms must not end a hold.
    TEST_ASSERT_TRUE(TOUCH_RELEASE_BRIDGE_MS >= 60 && TOUCH_RELEASE_BRIDGE_MS <= 80);
    // Weak pressure during a press is a dropout too, not a release.
    Filter h(TOUCH_CAL_DEFAULT);
    h.update(touch(2000, 2000), 0);
    TEST_ASSERT_TRUE(h.update(touch(2000, 2000, 100), 20).pressed);
    TEST_ASSERT_TRUE(h.update(touch(2000, 2000), 40).pressed);
    TEST_ASSERT_TRUE(h.update(touch(2000, 2000, 100), 50).pressed);
    TEST_ASSERT_FALSE(h.update(touch(2000, 2000, 100), 40 + TOUCH_RELEASE_BRIDGE_MS).pressed);
}

static void test_filter_rejects_weak_pressure_and_bad_calibration() {
    Filter f(TOUCH_CAL_DEFAULT);
    TEST_ASSERT_FALSE(f.update(touch(2000, 2000, 100), 0).pressed);
    Filter g(TouchCalibration {300, 300, 300, 3800});
    const Output o = g.update(touch(2000, 2000), 0);
    TEST_ASSERT_FALSE(o.pressed);
    TEST_ASSERT_TRUE(o.calibrationError);
}

// ---------------------------------------------------------------------------
// Touch test builds (MESSPLAN B8, B9, B12)

static void test_recorder_measures_a_touch_with_a_bridged_dropout() {
    TouchTest::Recorder rec;
    TouchTest::Record r {};
    // 10 ms reads: pressed 0..40 ms, raw dropout at 50/60 ms (bridged), pressed 70..90 ms.
    for (uint32_t t = 0; t <= 40; t += 10) TEST_ASSERT_FALSE(rec.update(true, (int16_t)(500 + t), true, 100, 50, t, &r));
    TEST_ASSERT_FALSE(rec.update(false, 0, true, 100, 50, 50, &r));
    TEST_ASSERT_FALSE(rec.update(false, 0, true, 100, 50, 60, &r));
    for (uint32_t t = 70; t <= 90; t += 10) TEST_ASSERT_FALSE(rec.update(true, 400, true, 102, 52, t, &r));
    TEST_ASSERT_TRUE(rec.update(false, 0, false, 102, 52, 130, &r));
    TEST_ASSERT_EQUAL_INT16(100, r.x0);
    TEST_ASSERT_EQUAL_INT16(50, r.y0);
    TEST_ASSERT_EQUAL_INT16(102, r.x1);
    TEST_ASSERT_EQUAL_INT16(52, r.y1);
    TEST_ASSERT_EQUAL_INT16(540, r.zMax);
    TEST_ASSERT_EQUAL_UINT32(90, r.durationMs);
    TEST_ASSERT_EQUAL_UINT16(1, r.dropouts);
    TEST_ASSERT_EQUAL_UINT32(20, r.maxDropoutMs);
    TEST_ASSERT_EQUAL_UINT32(0, r.gapBeforeMs); // first touch
    // Released reads produce nothing; the next touch reports the gap to the last one.
    TEST_ASSERT_FALSE(rec.update(false, 0, false, 0, 0, 140, &r));
    TEST_ASSERT_FALSE(rec.update(true, 600, true, 10, 10, 150, &r));
    TEST_ASSERT_TRUE(rec.update(false, 0, false, 10, 10, 160, &r));
    TEST_ASSERT_EQUAL_UINT32(60, r.gapBeforeMs); // 90 -> 150 ms: a dropout longer than the bridge
    TEST_ASSERT_EQUAL_UINT32(0, r.durationMs);
    TEST_ASSERT_EQUAL_UINT16(0, r.dropouts);
}

static void test_record_line() {
    TouchTest::Record r {100, 50, 102, 52, 540, 90, 820, 1, 20};
    char buf[96];
    TouchTest::formatRecord(r, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("x=100 y=50 to 102,52 z=540 t=90ms gap=820ms drop=1/20ms", buf);
    char small[8];
    TouchTest::formatRecord(r, small, sizeof(small));
    TEST_ASSERT_TRUE(strlen(small) < sizeof(small));
}

static void test_hit_map_targets_and_nearest() {
    using namespace TouchTest;
    for (uint8_t i = 0; i < TARGET_COUNT; ++i) {
        TEST_ASSERT_TRUE(TARGETS[i].x >= 0 && TARGETS[i].x < SCREEN_WIDTH);
        TEST_ASSERT_TRUE(TARGETS[i].y >= 0 && TARGETS[i].y < SCREEN_HEIGHT);
        int16_t dx = 9, dy = 9;
        TEST_ASSERT_EQUAL_UINT8(i, nearestTarget(TARGETS[i].x, TARGETS[i].y, &dx, &dy));
        TEST_ASSERT_EQUAL_INT16(0, dx);
        TEST_ASSERT_EQUAL_INT16(0, dy);
    }
    // A clamped corner tap near (319, 0) belongs to the corner, not to the 4 px target.
    int16_t dx = 0, dy = 0;
    const uint8_t i = nearestTarget(318, 1, &dx, &dy);
    TEST_ASSERT_EQUAL_INT16(319, TARGETS[i].x);
    TEST_ASSERT_EQUAL_INT16(0, TARGETS[i].y);
    TEST_ASSERT_EQUAL_INT16(-1, dx);
    TEST_ASSERT_EQUAL_INT16(1, dy);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plausible_raw_values);
    RUN_TEST(test_map_point_corners_and_clamping);
    RUN_TEST(test_filter_smooths_and_starts_at_the_new_press);
    RUN_TEST(test_filter_bridges_short_dropouts);
    RUN_TEST(test_filter_rejects_weak_pressure_and_bad_calibration);
    RUN_TEST(test_recorder_measures_a_touch_with_a_bridged_dropout);
    RUN_TEST(test_record_line);
    RUN_TEST(test_hit_map_targets_and_nearest);
    return UNITY_END();
}
