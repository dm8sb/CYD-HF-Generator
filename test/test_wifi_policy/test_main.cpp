// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

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

static void test_choose_slot() {
    using WifiPolicy::NOT_SEEN;
    const bool both[2] = {true, true};
    const bool first[2] = {true, false};
    const bool none[2] = {false, false};
    int r[2] = {-70, -60};
    TEST_ASSERT_EQUAL_INT(1, WifiPolicy::chooseSlot(both, r, -1, 0)); // strongest
    // The stronger one just failed (wrong password): the other one gets its turn.
    TEST_ASSERT_EQUAL_INT(0, WifiPolicy::chooseSlot(both, r, 1, 0));
    TEST_ASSERT_EQUAL_INT(1, WifiPolicy::chooseSlot(both, r, 0, 0));
    // Only one seen: that one, also after it failed.
    r[0] = NOT_SEEN;
    TEST_ASSERT_EQUAL_INT(1, WifiPolicy::chooseSlot(both, r, 1, 0));
    // None seen (hidden): round robin over the stored ones.
    r[1] = NOT_SEEN;
    TEST_ASSERT_EQUAL_INT(0, WifiPolicy::chooseSlot(both, r, -1, 0));
    TEST_ASSERT_EQUAL_INT(1, WifiPolicy::chooseSlot(both, r, -1, 1));
    TEST_ASSERT_EQUAL_INT(0, WifiPolicy::chooseSlot(first, r, -1, 1));
    TEST_ASSERT_EQUAL_INT(-1, WifiPolicy::chooseSlot(none, r, -1, 0));
}

// AF11: driver disconnect reasons (WIFI_REASON_*) as feedback on the WIFI page.
static void test_classify_failure() {
    using WifiPolicy::Failure;
    namespace R = WifiPolicy::Reason;
    TEST_ASSERT_EQUAL_INT((int)Failure::NotFound, (int)WifiPolicy::classifyFailure(R::NO_AP_FOUND));
    TEST_ASSERT_EQUAL_INT((int)Failure::WrongPassword, (int)WifiPolicy::classifyFailure(R::AUTH_EXPIRE));
    TEST_ASSERT_EQUAL_INT((int)Failure::WrongPassword, (int)WifiPolicy::classifyFailure(R::FOURWAY_HANDSHAKE_TIMEOUT));
    TEST_ASSERT_EQUAL_INT((int)Failure::WrongPassword, (int)WifiPolicy::classifyFailure(R::AUTH_FAIL));
    TEST_ASSERT_EQUAL_INT((int)Failure::WrongPassword, (int)WifiPolicy::classifyFailure(R::HANDSHAKE_TIMEOUT));
    // No disconnect event within the timeout, or any other reason.
    TEST_ASSERT_EQUAL_INT((int)Failure::Other, (int)WifiPolicy::classifyFailure(0));
    TEST_ASSERT_EQUAL_INT((int)Failure::Other, (int)WifiPolicy::classifyFailure(8));
    TEST_ASSERT_EQUAL_INT((int)Failure::Other, (int)WifiPolicy::classifyFailure(203));
    TEST_ASSERT_EQUAL_INT((int)Failure::Other, (int)WifiPolicy::classifyFailure(0xFFFF));
    TEST_ASSERT_EQUAL_UINT16(201, R::NO_AP_FOUND);
}

// AF11: RSSI and AP stations are read from the driver at most once per second.
static void test_status_poll_rate() {
    TEST_ASSERT_TRUE(WifiPolicy::pollDue(1000, 0, false));      // never polled
    TEST_ASSERT_FALSE(WifiPolicy::pollDue(999, 0, true));
    TEST_ASSERT_TRUE(WifiPolicy::pollDue(1000, 0, true));
    TEST_ASSERT_FALSE(WifiPolicy::pollDue(10, 0xFFFFFF00u, true)); // 266 ms across the millis() wrap
    TEST_ASSERT_TRUE(WifiPolicy::pollDue(800, 0xFFFFFF00u, true));
}

// --- AF13: connect and scan timing (WifiPolicy::Connector) ---------------------------------

using WifiPolicy::Connector;

static void test_merge_seen_strongest_per_ssid() {
    const char* stored[2] = {"Lab", "Home"};
    int rssi[2] = {WifiPolicy::NOT_SEEN, WifiPolicy::NOT_SEEN};
    TEST_ASSERT_EQUAL_UINT8(0, WifiPolicy::mergeSeen(stored, "Other", -40, rssi));
    TEST_ASSERT_EQUAL_UINT8(1, WifiPolicy::mergeSeen(stored, "Lab", -80, rssi));
    TEST_ASSERT_EQUAL_UINT8(1, WifiPolicy::mergeSeen(stored, "Lab", -60, rssi)); // second AP, stronger
    TEST_ASSERT_EQUAL_UINT8(0, WifiPolicy::mergeSeen(stored, "Lab", -70, rssi)); // weaker: kept
    TEST_ASSERT_EQUAL_INT(-60, rssi[0]);
    TEST_ASSERT_EQUAL_INT(WifiPolicy::NOT_SEEN, rssi[1]);
    // The same network stored in both slots (doubled SSID): both get the entry.
    const char* twice[2] = {"Lab", "Lab"};
    int r2[2] = {WifiPolicy::NOT_SEEN, WifiPolicy::NOT_SEEN};
    TEST_ASSERT_EQUAL_UINT8(3, WifiPolicy::mergeSeen(twice, "Lab", -50, r2));
    // Empty slots never match, not even an empty (hidden) SSID in the scan.
    const char* none[2] = {"", ""};
    int r3[2] = {WifiPolicy::NOT_SEEN, WifiPolicy::NOT_SEEN};
    TEST_ASSERT_EQUAL_UINT8(0, WifiPolicy::mergeSeen(none, "", -30, r3));
}

static void test_connect_timeout_at_the_limit() {
    Connector c;
    c.reset();
    c.attemptStarted(0, 1000);
    TEST_ASSERT_TRUE(c.attempting());
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::None, (int)c.poll(1000 + Connector::CONNECT_TIMEOUT_MS - 1));
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::TimedOut, (int)c.poll(1000 + Connector::CONNECT_TIMEOUT_MS));
    TEST_ASSERT_FALSE(c.attempting());
    TEST_ASSERT_EQUAL_INT(0, c.lastFailed());
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::None, (int)c.poll(1000 + 2 * Connector::CONNECT_TIMEOUT_MS));
}

static void test_connect_timeout_across_millis_wrap() {
    Connector c;
    c.reset();
    const uint32_t t0 = 0xFFFFF000u; // 4.1 s before the wrap
    c.attemptStarted(1, t0);
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::None, (int)c.poll(t0 + 5000)); // wrapped, 5 s
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::TimedOut, (int)c.poll(t0 + Connector::CONNECT_TIMEOUT_MS));
}

static const bool ONE_STORED[2] = {true, false};

static void test_scan_due_and_millis_wrap() {
    Connector c;
    c.reset();
    // The first scan at once, whatever millis() is (also 0).
    TEST_ASSERT_TRUE(c.scanDue(0, ONE_STORED, false));
    c.scanStarted(0xFFFFFF00u);
    TEST_ASSERT_FALSE(c.scanDue(0xFFFFFF00u + 1000, ONE_STORED, false));
    TEST_ASSERT_TRUE(c.scanDue(0xFFFFFF00u + Connector::SCAN_INTERVAL_MS, ONE_STORED, false));
    // A scan of the WIFI page counts as the last scan.
    c.scanEnded(50000);
    TEST_ASSERT_FALSE(c.scanDue(50000 + Connector::SCAN_INTERVAL_MS - 1, ONE_STORED, false));
}

// AF14, D32: no background scan without a stored network.
static void test_no_scan_without_stored_network() {
    Connector c;
    c.reset();
    const bool none[2] = {false, false};
    TEST_ASSERT_FALSE(c.scanDue(0, none, false));
    TEST_ASSERT_FALSE(c.scanDue(10 * 60000, none, false));
    TEST_ASSERT_TRUE(c.scanDue(10 * 60000, ONE_STORED, false));
}

// AF14, D32: 15 s, doubling while no stored network is seen, at most 5 min; a seen network
// (or a connection) starts again at 15 s.
static void test_scan_backoff() {
    Connector c;
    c.reset();
    const uint32_t expected[] = {15000, 30000, 60000, 120000, 240000, 300000, 300000};
    uint32_t t = 1000;
    c.scanStarted(t);
    for (uint32_t gap : expected) {
        TEST_ASSERT_EQUAL_UINT32(gap, c.scanIntervalMs(false));
        TEST_ASSERT_FALSE(c.scanDue(t + gap - 1, ONE_STORED, false));
        TEST_ASSERT_TRUE(c.scanDue(t + gap, ONE_STORED, false));
        c.scanResult(false); // nothing stored in the scan
        t += gap;
        c.scanStarted(t);
    }
    c.scanResult(true);
    TEST_ASSERT_EQUAL_UINT32(Connector::SCAN_INTERVAL_MS, c.scanIntervalMs(false));
    for (int i = 0; i < 4; ++i) c.scanResult(false);
    c.connected();
    TEST_ASSERT_EQUAL_UINT32(Connector::SCAN_INTERVAL_MS, c.scanIntervalMs(false));
    for (int i = 0; i < 4; ++i) c.scanResult(false);
    c.scanEnded(t); // a scan of the WIFI page
    TEST_ASSERT_EQUAL_UINT32(Connector::SCAN_INTERVAL_MS, c.scanIntervalMs(false));
    for (int i = 0; i < 4; ++i) c.scanResult(false);
    c.reset();
    TEST_ASSERT_EQUAL_UINT32(Connector::SCAN_INTERVAL_MS, c.scanIntervalMs(false));
}

// AF14, D32: with stations at the fallback AP at most every 2 min (the AP leaves its channel
// for every scan and attempt).
static void test_scan_with_ap_stations() {
    Connector c;
    c.reset();
    c.scanStarted(0);
    TEST_ASSERT_EQUAL_UINT32(Connector::SCAN_WITH_STATIONS_MS, c.scanIntervalMs(true));
    TEST_ASSERT_FALSE(c.scanDue(Connector::SCAN_WITH_STATIONS_MS - 1, ONE_STORED, true));
    TEST_ASSERT_TRUE(c.scanDue(Connector::SCAN_WITH_STATIONS_MS, ONE_STORED, true));
    for (int i = 0; i < 6; ++i) c.scanResult(false);
    TEST_ASSERT_EQUAL_UINT32(Connector::SCAN_BACKOFF_MAX_MS, c.scanIntervalMs(true));
}

// The stronger network fails (wrong password) twice in a row: the other one gets its turn after
// each failure, and a success clears the memory.
static void test_stronger_network_fails_twice() {
    Connector c;
    c.reset();
    const bool both[2] = {true, true};
    const int r[2] = {-75, -55};
    uint32_t t = 0;
    int slot = c.choose(both, r);
    TEST_ASSERT_EQUAL_INT(1, slot);
    c.attemptStarted(slot, t);
    t += Connector::CONNECT_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::TimedOut, (int)c.poll(t));
    slot = c.choose(both, r);
    TEST_ASSERT_EQUAL_INT(0, slot); // the weaker one
    c.attemptStarted(slot, t);
    t += Connector::CONNECT_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::TimedOut, (int)c.poll(t));
    slot = c.choose(both, r);
    TEST_ASSERT_EQUAL_INT(1, slot); // back to the stronger one (second try)
    c.attemptStarted(slot, t);
    t += Connector::CONNECT_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT((int)Connector::Step::TimedOut, (int)c.poll(t));
    TEST_ASSERT_EQUAL_INT(0, c.choose(both, r));
    c.attemptStarted(0, t);
    c.connected();
    TEST_ASSERT_FALSE(c.attempting());
    TEST_ASSERT_EQUAL_INT(-1, c.lastFailed());
    TEST_ASSERT_EQUAL_INT(1, c.choose(both, r)); // after a success the strongest again
}

// Both networks hidden (never in the scan): tried in turn.
static void test_both_hidden_round_robin() {
    Connector c;
    c.reset();
    const bool both[2] = {true, true};
    const int r[2] = {WifiPolicy::NOT_SEEN, WifiPolicy::NOT_SEEN};
    TEST_ASSERT_EQUAL_INT(0, c.choose(both, r));
    TEST_ASSERT_EQUAL_INT(1, c.choose(both, r));
    TEST_ASSERT_EQUAL_INT(0, c.choose(both, r));
    const bool none[2] = {false, false};
    TEST_ASSERT_EQUAL_INT(-1, c.choose(none, r));
}

static void test_open_ap_idle() {
    TEST_ASSERT_FALSE(WifiPolicy::openApIdleExpired(WifiPolicy::OPEN_AP_IDLE_OFF_MS - 1, 0));
    TEST_ASSERT_TRUE(WifiPolicy::openApIdleExpired(WifiPolicy::OPEN_AP_IDLE_OFF_MS, 0));
    TEST_ASSERT_FALSE(WifiPolicy::openApIdleExpired(1000, 0xFFFFF000u)); // across the wrap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_off_runs_nothing);
    RUN_TEST(test_access_point_mode);
    RUN_TEST(test_client_fallback_after_60_s);
    RUN_TEST(test_ap_never_with_client_address);
    RUN_TEST(test_open_fallback_needs_confirmation);
    RUN_TEST(test_choose_slot);
    RUN_TEST(test_classify_failure);
    RUN_TEST(test_status_poll_rate);
    RUN_TEST(test_merge_seen_strongest_per_ssid);
    RUN_TEST(test_connect_timeout_at_the_limit);
    RUN_TEST(test_connect_timeout_across_millis_wrap);
    RUN_TEST(test_scan_due_and_millis_wrap);
    RUN_TEST(test_no_scan_without_stored_network);
    RUN_TEST(test_scan_backoff);
    RUN_TEST(test_scan_with_ap_stations);
    RUN_TEST(test_stronger_network_fails_twice);
    RUN_TEST(test_both_hidden_round_robin);
    RUN_TEST(test_open_ap_idle);
    return UNITY_END();
}
