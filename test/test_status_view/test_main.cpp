// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// AF12: the GUI redraws a page only when the fields it shows change.

#include <unity.h>
#include <string.h>
#include <utils/status_view.h>

void setUp() {}
void tearDown() {}

static StatusView zero() {
    StatusView v;
    memset(&v, 0, sizeof(v));
    return v;
}

static void test_equal_views() {
    StatusView a = zero(), b = zero();
    TEST_ASSERT_TRUE(a == b);
    a.staIp = 0xC0A80132;
    b.staIp = 0xC0A80132;
    a.rssi = b.rssi = -60;
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a != b);
}

// Every field takes part in the comparison (no hash that two states could share).
static void test_every_field_counts() {
    const StatusView base = zero();
    StatusView v;
    v = base; v.remote = 2; TEST_ASSERT_TRUE(v != base);
    v = base; v.wifiMode = 1; TEST_ASSERT_TRUE(v != base);
    v = base; v.staConnected = true; TEST_ASSERT_TRUE(v != base);
    v = base; v.apActive = true; TEST_ASSERT_TRUE(v != base);
    v = base; v.apOpen = true; TEST_ASSERT_TRUE(v != base);
    v = base; v.fallback = true; TEST_ASSERT_TRUE(v != base);
    v = base; v.needsOpenConfirmation = true; TEST_ASSERT_TRUE(v != base);
    v = base; v.connecting = true; TEST_ASSERT_TRUE(v != base);
    v = base; v.apStations = 1; TEST_ASSERT_TRUE(v != base);
    v = base; v.lastFailure = 2; TEST_ASSERT_TRUE(v != base);
    v = base; v.scanState = 1; TEST_ASSERT_TRUE(v != base);
    v = base; v.scanCount = 3; TEST_ASSERT_TRUE(v != base);
    v = base; v.rssi = -70; TEST_ASSERT_TRUE(v != base);
    v = base; v.staIp = 1; TEST_ASSERT_TRUE(v != base);
    v = base; v.heapStep = 1; TEST_ASSERT_TRUE(v != base);
    v = base; v.webClients = 1; TEST_ASSERT_TRUE(v != base);
    v = base; v.scpiClient = 1; TEST_ASSERT_TRUE(v != base);
    v = base; v.netPendingS = 1; TEST_ASSERT_TRUE(v != base);
}

// The old XOR hash: staIp * 2654435761 ^ (rssi/5 << 9) could cancel out; field-wise it cannot.
static void test_no_collision_of_ip_and_rssi() {
    StatusView a = zero(), b = zero();
    a.staIp = 1;
    b.rssi = -5;
    TEST_ASSERT_TRUE(a != b);
}

static void test_rssi_hysteresis() {
    using StatusViewRules::rssiShown;
    TEST_ASSERT_EQUAL_INT(-60, rssiShown(0, -60));   // first value at once
    TEST_ASSERT_EQUAL_INT(-60, rssiShown(-60, -62)); // 2 dB: kept
    TEST_ASSERT_EQUAL_INT(-60, rssiShown(-60, -58));
    TEST_ASSERT_EQUAL_INT(-63, rssiShown(-60, -63)); // 3 dB: follows
    TEST_ASSERT_EQUAL_INT(-57, rssiShown(-60, -57));
    TEST_ASSERT_EQUAL_INT(0, rssiShown(-60, 0));     // disconnected at once
}

static void test_heap_steps_and_rate() {
    using namespace StatusViewRules;
    TEST_ASSERT_EQUAL_UINT32(heapStep(100000), heapStep(100000 + 1000)); // 1 KB: same step
    TEST_ASSERT_EQUAL_UINT32(24, heapStep(24 * 4096 + 4095));
    TEST_ASSERT_EQUAL_UINT32(25, heapStep(25 * 4096));
    TEST_ASSERT_TRUE(heapDue(0, 0, false));
    TEST_ASSERT_FALSE(heapDue(4999, 0, true));
    TEST_ASSERT_TRUE(heapDue(5000, 0, true));
    TEST_ASSERT_FALSE(heapDue(100, 0xFFFFF000u, true)); // across the millis() wrap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_equal_views);
    RUN_TEST(test_every_field_counts);
    RUN_TEST(test_no_collision_of_ip_and_rssi);
    RUN_TEST(test_rssi_hysteresis);
    RUN_TEST(test_heap_steps_and_rate);
    return UNITY_END();
}
