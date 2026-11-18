// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Ipv4::parse/format (CQ-10): the one dotted-quad parser and formatter of the firmware.

#include <unity.h>
#include <string.h>
#include <utils/ipv4.h>

void setUp() {}
void tearDown() {}

static void test_parse_canonical_quads() {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(Ipv4::parse("192.168.4.1", &v));
    TEST_ASSERT_EQUAL_HEX32(0xC0A80401u, v);
    TEST_ASSERT_TRUE(Ipv4::parse("0.0.0.0", &v));
    TEST_ASSERT_EQUAL_HEX32(0u, v);
    TEST_ASSERT_TRUE(Ipv4::parse("255.255.255.255", &v));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFFu, v);
    // Length-bounded: only the first n bytes count (Host header with a port).
    TEST_ASSERT_TRUE(Ipv4::parse("10.0.0.7:80", 8, &v));
    TEST_ASSERT_EQUAL_HEX32(0x0A000007u, v);
}

static void test_parse_rejects_everything_else() {
    static const char* const BAD[] = {"", "1.2.3", "1.2.3.4.", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "01.2.3.4",
                                      "1.2.3.04", "1.2.3.00", "1..3.4", ".1.2.3", "1.2.3.4 ", " 1.2.3.4", "+1.2.3.4",
                                      "1.2.3.-4", "1.2.3.a", "1000.1.1.1", "0x1.2.3.4", "1.2.3.4/24"};
    for (const char* s : BAD) {
        uint32_t v = 0xDEADBEEF;
        TEST_ASSERT_FALSE_MESSAGE(Ipv4::parse(s, &v), s);
        TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, v); // untouched on failure
    }
    uint32_t v = 0;
    TEST_ASSERT_FALSE(Ipv4::parse(nullptr, &v));
    TEST_ASSERT_FALSE(Ipv4::parse("1.2.3.4", nullptr));
    TEST_ASSERT_FALSE(Ipv4::parse("1.2.3.4", 6, &v)); // cut inside the last part: "1.2.3."
}

static void test_format_and_round_trip() {
    char t[Ipv4::TEXT_SIZE];
    TEST_ASSERT_TRUE(Ipv4::format(0xC0A80401u, t, sizeof(t)));
    TEST_ASSERT_EQUAL_STRING("192.168.4.1", t);
    TEST_ASSERT_TRUE(Ipv4::format(0xFFFFFFFFu, t, sizeof(t)));
    TEST_ASSERT_EQUAL_STRING("255.255.255.255", t);
    const uint32_t samples[] = {0u, 1u, 0x7F000001u, 0x0A0B0C0Du, 0x80000000u, 0xFFFFFFFEu};
    for (uint32_t x : samples) {
        uint32_t back = 0;
        TEST_ASSERT_TRUE(Ipv4::format(x, t, sizeof(t)));
        TEST_ASSERT_TRUE(Ipv4::parse(t, &back));
        TEST_ASSERT_EQUAL_HEX32(x, back);
    }
}

static void test_format_truncates_safely() {
    char t[8];
    memset(t, 'x', sizeof(t));
    TEST_ASSERT_FALSE(Ipv4::format(0xC0A80401u, t, sizeof(t)));
    TEST_ASSERT_EQUAL_STRING("192.168", t);
    TEST_ASSERT_FALSE(Ipv4::format(1u, t, 0));
    TEST_ASSERT_FALSE(Ipv4::format(1u, nullptr, 16));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_canonical_quads);
    RUN_TEST(test_parse_rejects_everything_else);
    RUN_TEST(test_format_and_round_trip);
    RUN_TEST(test_format_truncates_safely);
    return UNITY_END();
}
