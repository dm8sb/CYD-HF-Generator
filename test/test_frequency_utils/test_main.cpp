#include <unity.h>
#include <constants.h>
#include <utils/frequency_utils.h>
#include <stdio.h>
#include <string.h>

using namespace FrequencyUtils;

void setUp() {}
void tearDown() {}

static void assertFormat(uint64_t hz, const char* expected) {
    char buf[32];
    format(hz, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_format_groups_digits() {
    assertFormat(1000000ULL, "1.000 000 MHz");
    assertFormat(10700000ULL, "10.700 000 MHz");
    assertFormat(14074001ULL, "14.074 001 MHz");
    assertFormat(100000000ULL, "100.000 000 MHz");
}

static void test_format_truncates_to_buffer() {
    char buf[6];
    format(10700000ULL, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("10.70", buf);
    format(10700000ULL, nullptr, 8); // must not crash
}

static void test_format_step() {
    char buf[16];
    formatStep(1, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1 Hz", buf);
    formatStep(100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100 Hz", buf);
    formatStep(10000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("10 kHz", buf);
    formatStep(1000000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1 MHz", buf);
    formatStep(1500, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1500 Hz", buf);
}

static void test_format_ppm() {
    char buf[16];
    formatPpm(970, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("+9.70 ppm", buf);
    formatPpm(-5, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-0.05 ppm", buf);
    formatPpm(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("+0.00 ppm", buf);
    formatPpm(-CORRECTION_MAX, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-50.00 ppm", buf);
}

static void assertParse(const char* text, const char* unit, uint64_t expected) {
    uint64_t hz = 0;
    TEST_ASSERT_TRUE_MESSAGE(parseFrequency(text, unit, &hz), text);
    TEST_ASSERT_EQUAL_UINT64(expected, hz);
}

static void assertRejected(const char* text, const char* unit) {
    uint64_t hz = 12345;
    TEST_ASSERT_FALSE_MESSAGE(parseFrequency(text, unit, &hz), text);
    TEST_ASSERT_EQUAL_UINT64(12345, hz);
}

static void test_parse_units() {
    assertParse("10.7", "MHz", 10700000ULL);
    assertParse("1000", "kHz", 1000000ULL);
    assertParse("5000000", "Hz", 5000000ULL);
    assertParse("14.074001", "MHz", 14074001ULL);
    assertParse("7.0745", "MHz", 7074500ULL);
}

static void test_parse_limits() {
    assertParse("1", "MHz", FREQ_MIN_HZ);
    assertParse("100", "MHz", FREQ_MAX_HZ);
    assertRejected("0.999999", "MHz");
    assertRejected("100.000001", "MHz");
    assertRejected("500", "MHz");
    assertRejected("0.5", "MHz");
    assertRejected("500", "Hz");
}

static void test_parse_rounds_to_hz() {
    assertParse("1000000.4", "Hz", 1000000ULL);
    assertParse("1000000.5", "Hz", 1000001ULL);
    // Binary floating point must not lose the last Hz digit.
    assertParse("99.999999", "MHz", 99999999ULL);
    assertParse("12.345678", "MHz", 12345678ULL);
}

static void test_parse_rejects_malformed_input() {
    assertRejected("", "MHz");
    assertRejected(".", "MHz");
    assertRejected("1.2.3", "MHz");
    assertRejected("10x", "MHz");
    assertRejected("0", "MHz");
    TEST_ASSERT_FALSE(parseFrequency(nullptr, "MHz", nullptr));
}

static void test_parse_digit_sweep_matches_integer_math() {
    // Every kHz-aligned value from the numpad must parse exactly.
    char text[16];
    for (uint32_t khz = 1000; khz <= 100000; khz += 7) {
        snprintf(text, sizeof(text), "%lu.%03lu", (unsigned long)(khz / 1000), (unsigned long)(khz % 1000));
        uint64_t hz = 0;
        TEST_ASSERT_TRUE_MESSAGE(parseFrequency(text, "MHz", &hz), text);
        TEST_ASSERT_EQUAL_UINT64_MESSAGE((uint64_t)khz * 1000ULL, hz, text);
    }
}

static void test_clamp() {
    TEST_ASSERT_EQUAL_UINT64(FREQ_MIN_HZ, clamp(0));
    TEST_ASSERT_EQUAL_UINT64(FREQ_MIN_HZ, clamp(FREQ_MIN_HZ - 1));
    TEST_ASSERT_EQUAL_UINT64(12345678ULL, clamp(12345678ULL));
    TEST_ASSERT_EQUAL_UINT64(FREQ_MAX_HZ, clamp(FREQ_MAX_HZ + 1));
}

static void test_step_index_and_snap() {
    TEST_ASSERT_EQUAL_UINT8(0, stepIndex(0));
    TEST_ASSERT_EQUAL_UINT8(0, stepIndex(1));
    TEST_ASSERT_EQUAL_UINT8(2, stepIndex(999));
    TEST_ASSERT_EQUAL_UINT8(3, stepIndex(1000));
    TEST_ASSERT_EQUAL_UINT8(DELTA_STEP_COUNT - 1, stepIndex(50000000));
    TEST_ASSERT_EQUAL_UINT32(1, snapStep(0));
    TEST_ASSERT_EQUAL_UINT32(1000, snapStep(5000));
    TEST_ASSERT_EQUAL_UINT32(1000000, snapStep(2000000));
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) TEST_ASSERT_EQUAL_UINT32(DELTA_STEPS[i], snapStep(DELTA_STEPS[i]));
}

static void test_digit_exponent_on_formatted_text() {
    const char* text = "10.700 000 MHz";
    TEST_ASSERT_EQUAL_INT(7, digitExponent(text, 0));
    TEST_ASSERT_EQUAL_INT(6, digitExponent(text, 1));
    TEST_ASSERT_EQUAL_INT(-1, digitExponent(text, 2));  // '.'
    TEST_ASSERT_EQUAL_INT(5, digitExponent(text, 3));
    TEST_ASSERT_EQUAL_INT(-1, digitExponent(text, 6));  // ' '
    TEST_ASSERT_EQUAL_INT(0, digitExponent(text, 9));
    TEST_ASSERT_EQUAL_INT(-1, digitExponent(text, 11)); // 'M'
    TEST_ASSERT_EQUAL_INT(-1, digitExponent(text, 99));
    TEST_ASSERT_EQUAL_INT(-1, digitExponent(nullptr, 0));
}

static void test_digit_exponent_matches_step_digits() {
    // The digit highlighted for a step must be the one that +/- changes.
    char text[32];
    format(12345678ULL, text, sizeof(text));
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) {
        const uint8_t exp = decimalExponent(DELTA_STEPS[i]);
        int found = -1;
        for (size_t k = 0; text[k]; ++k) {
            if (digitExponent(text, k) == exp) found = (int)k;
        }
        TEST_ASSERT_TRUE(found >= 0);
        TEST_ASSERT_EQUAL_CHAR("12345678"[7 - exp], text[found]);
    }
}

static void test_decimal_exponent_and_power_of_ten() {
    TEST_ASSERT_EQUAL_UINT8(0, decimalExponent(0));
    TEST_ASSERT_EQUAL_UINT8(0, decimalExponent(9));
    TEST_ASSERT_EQUAL_UINT8(1, decimalExponent(10));
    TEST_ASSERT_EQUAL_UINT8(6, decimalExponent(1000000));
    TEST_ASSERT_EQUAL_UINT8(9, decimalExponent(4294967295UL));
    for (uint8_t e = 0; e <= 9; ++e) TEST_ASSERT_EQUAL_UINT8(e, decimalExponent(powerOfTen(e)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_format_groups_digits);
    RUN_TEST(test_format_truncates_to_buffer);
    RUN_TEST(test_format_step);
    RUN_TEST(test_format_ppm);
    RUN_TEST(test_parse_units);
    RUN_TEST(test_parse_limits);
    RUN_TEST(test_parse_rounds_to_hz);
    RUN_TEST(test_parse_rejects_malformed_input);
    RUN_TEST(test_parse_digit_sweep_matches_integer_math);
    RUN_TEST(test_clamp);
    RUN_TEST(test_step_index_and_snap);
    RUN_TEST(test_digit_exponent_on_formatted_text);
    RUN_TEST(test_digit_exponent_matches_step_digits);
    RUN_TEST(test_decimal_exponent_and_power_of_ten);
    return UNITY_END();
}
