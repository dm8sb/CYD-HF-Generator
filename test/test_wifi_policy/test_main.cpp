// WiFi policy (W-E1, W-S2, W-S3, D14): which interfaces run for the observed state.

#include <unity.h>
#include <net/wifi_policy.h>

using WifiPolicy::Inputs;
using WifiPolicy::Targets;

void setUp() {}
void tearDown() {}

static Inputs in(WifiMode mode) { return Inputs {mode, false, 0, false, false, false}; }

static void test_off_runs_nothing() {
    Inputs i = in(WifiMode::Off);
    i.msWithoutIp = 10 * WifiPolicy::FALLBACK_AFTER_MS;
    const Targets t = WifiPolicy::decide(i);
    TEST_ASSERT_FALSE(t.sta);
    TEST_ASSERT_FALSE(t.ap);
}

static void test_access_point_mode() {
    Targets t = WifiPolicy::decide(in(WifiMode::AccessPoint));
    TEST_ASSERT_FALSE(t.sta);
    TEST_ASSERT_TRUE(t.ap);
    TEST_ASSERT_FALSE(t.apOpen);
    TEST_ASSERT_FALSE(t.fallback);
    Inputs i = in(WifiMode::AccessPoint);
    i.apOpen = true;
    t = WifiPolicy::decide(i);
    TEST_ASSERT_TRUE(t.ap);
    TEST_ASSERT_TRUE(t.apOpen);
    // W-S2: an open AP without station for 15 min goes off; a closed one stays.
    i.openApTimedOut = true;
    TEST_ASSERT_FALSE(WifiPolicy::decide(i).ap);
    i.apOpen = false;
    TEST_ASSERT_TRUE(WifiPolicy::decide(i).ap);
}

static void test_client_fallback_after_60_s() {
    Inputs i = in(WifiMode::Client);
    i.msWithoutIp = WifiPolicy::FALLBACK_AFTER_MS - 1;
    Targets t = WifiPolicy::decide(i);
    TEST_ASSERT_TRUE(t.sta);
    TEST_ASSERT_FALSE(t.ap);
    i.msWithoutIp = WifiPolicy::FALLBACK_AFTER_MS;
    t = WifiPolicy::decide(i);
    TEST_ASSERT_TRUE(t.sta); // keeps searching
    TEST_ASSERT_TRUE(t.ap);
    TEST_ASSERT_TRUE(t.fallback);
    TEST_ASSERT_FALSE(t.apOpen);
}

// W-S3 / D14: as soon as the client has an address the AP goes, whatever else is set.
static void test_ap_never_with_client_address() {
    Inputs i = in(WifiMode::Client);
    i.staHasIp = true;
    i.msWithoutIp = 10 * WifiPolicy::FALLBACK_AFTER_MS;
    i.apOpen = true;
    i.openFallbackConfirmed = true;
    const Targets t = WifiPolicy::decide(i);
    TEST_ASSERT_TRUE(t.sta);
    TEST_ASSERT_FALSE(t.ap);
    TEST_ASSERT_FALSE(t.needsOpenConfirmation);
}

// W-E1 / W-S2: an open fallback AP only after the touch confirmed it.
static void test_open_fallback_needs_confirmation() {
    Inputs i = in(WifiMode::Client);
    i.msWithoutIp = WifiPolicy::FALLBACK_AFTER_MS;
    i.apOpen = true;
    Targets t = WifiPolicy::decide(i);
    TEST_ASSERT_FALSE(t.ap);
    TEST_ASSERT_TRUE(t.needsOpenConfirmation);
    i.openFallbackConfirmed = true;
    t = WifiPolicy::decide(i);
    TEST_ASSERT_TRUE(t.ap);
    TEST_ASSERT_TRUE(t.apOpen);
    TEST_ASSERT_FALSE(t.needsOpenConfirmation);
    i.openApTimedOut = true;
    TEST_ASSERT_FALSE(WifiPolicy::decide(i).ap);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_off_runs_nothing);
    RUN_TEST(test_access_point_mode);
    RUN_TEST(test_client_fallback_after_60_s);
    RUN_TEST(test_ap_never_with_client_address);
    RUN_TEST(test_open_fallback_needs_confirmation);
    return UNITY_END();
}
