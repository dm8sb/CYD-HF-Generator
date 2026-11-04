// Touch processing without the XPT2046 library (audit T-14): calibration, plausibility,
// jitter filter, dropout bridge and the boot gesture threshold.

#include <unity.h>
#include <constants.h>
#include <drivers/touch_filter.h>

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
}

static void test_filter_rejects_weak_pressure_and_bad_calibration() {
    Filter f(TOUCH_CAL_DEFAULT);
    TEST_ASSERT_FALSE(f.update(touch(2000, 2000, 100), 0).pressed);
    Filter g(TouchCalibration {300, 300, 300, 3800});
    const Output o = g.update(touch(2000, 2000), 0);
    TEST_ASSERT_FALSE(o.pressed);
    TEST_ASSERT_TRUE(o.calibrationError);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plausible_raw_values);
    RUN_TEST(test_map_point_corners_and_clamping);
    RUN_TEST(test_filter_smooths_and_starts_at_the_new_press);
    RUN_TEST(test_filter_bridges_short_dropouts);
    RUN_TEST(test_filter_rejects_weak_pressure_and_bad_calibration);
    return UNITY_END();
}
