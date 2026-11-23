// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// HeapWatch (FW-08, A-726): `Heap low` once below 40 KB, again only after recovering above 48 KB.

#include <unity.h>
#include <constants.h>
#include <utils/heap_watch.h>

void setUp() {}
void tearDown() {}

static const uint32_t LOW = HEAP_LOW_BYTES;
static const uint32_t RECOVER = HEAP_RECOVER_BYTES;

static void test_limits_are_40_and_48_kb() {
    TEST_ASSERT_EQUAL_UINT32(40u * 1024u, LOW);
    TEST_ASSERT_EQUAL_UINT32(48u * 1024u, RECOVER);
}

static void test_no_warning_above_the_limit() {
    HeapWatch w(LOW, RECOVER);
    TEST_ASSERT_FALSE(w.update(200u * 1024u));
    TEST_ASSERT_FALSE(w.update(LOW)); // exactly 40 KB is not below
    TEST_ASSERT_FALSE(w.low());
}

static void test_hysteresis_cycle() {
    HeapWatch w(LOW, RECOVER);
    TEST_ASSERT_FALSE(w.update(100u * 1024u));
    // Falls below: one warning.
    TEST_ASSERT_TRUE(w.update(LOW - 1));
    TEST_ASSERT_TRUE(w.low());
    // Stays below: no repetition.
    TEST_ASSERT_FALSE(w.update(LOW - 1));
    TEST_ASSERT_FALSE(w.update(10u * 1024u));
    // Recovers between 40 and 48 KB: still low, no new warning when it drops again.
    TEST_ASSERT_FALSE(w.update(44u * 1024u));
    TEST_ASSERT_FALSE(w.update(RECOVER)); // exactly 48 KB is not above
    TEST_ASSERT_TRUE(w.low());
    TEST_ASSERT_FALSE(w.update(LOW - 1));
    // Above 48 KB: recovered.
    TEST_ASSERT_FALSE(w.update(RECOVER + 1));
    TEST_ASSERT_FALSE(w.low());
    TEST_ASSERT_FALSE(w.update(60u * 1024u));
    // Falls again: a new warning.
    TEST_ASSERT_TRUE(w.update(30u * 1024u));
    TEST_ASSERT_TRUE(w.low());
}

static void test_first_measurement_below_warns() {
    HeapWatch w(LOW, RECOVER);
    TEST_ASSERT_TRUE(w.update(0));
    TEST_ASSERT_FALSE(w.update(0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_limits_are_40_and_48_kb);
    RUN_TEST(test_no_warning_above_the_limit);
    RUN_TEST(test_hysteresis_cycle);
    RUN_TEST(test_first_measurement_below_warns);
    return UNITY_END();
}
