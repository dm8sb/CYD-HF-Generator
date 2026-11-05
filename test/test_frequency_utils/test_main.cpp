#include <unity.h>
#include <constants.h>
#include <utils/frequency_utils.h>
#include <math.h>
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
    assertFormat(160000000ULL, "160.000 000 MHz");
    // Below 1 MHz in kHz with Hz resolution (range extension, E-16).
    assertFormat(8000ULL, "8.000 kHz");
    assertFormat(455000ULL, "455.000 kHz");
    assertFormat(999999ULL, "999.999 kHz");
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

static void test_format_mhz() {
    char buf[16];
    formatMHz(IQ_MIN_HZ, 3, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("4.730", buf);
    formatMHz(IQ_MIN_HZ, 2, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("4.73", buf);
    formatMHz(IQ_MAX_HZ, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100", buf);
    formatMHz(14074001ULL, 6, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("14.074001", buf);
    formatMHz(4730500ULL, 3, buf, sizeof(buf)); // rounded
    TEST_ASSERT_EQUAL_STRING("4.731", buf);
    formatMHz(999999ULL, 2, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1.00", buf);
}

static void test_spread_label() {
    TEST_ASSERT_EQUAL_STRING("Off", spreadLabel(Spread::Off));
    TEST_ASSERT_EQUAL_STRING("-0.5 %", spreadLabel(Spread::Down05));
    TEST_ASSERT_EQUAL_STRING("-1.5 %", spreadLabel(Spread::Down15));
    TEST_ASSERT_EQUAL_STRING("+/-0.5 %", spreadLabel(Spread::Center05));
}

static void test_deviation_rounds_to_tenth() {
    TEST_ASSERT_EQUAL_DOUBLE(0.3, deviation(10000000.3, 10000000ULL));
    TEST_ASSERT_EQUAL_DOUBLE(-1.4, deviation(9999998.62, 10000000ULL));
    TEST_ASSERT_EQUAL_DOUBLE(2272.7, deviation(7102272.73, 7100000ULL));
    // No "-0.0" for a tiny negative deviation.
    const double d = deviation(9999999.98, 10000000ULL);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, d);
    TEST_ASSERT_FALSE(signbit(d));
}

static void assertOutput(double actual, uint64_t requested, Spread spread, const char* expected, double expectedDev) {
    char buf[48];
    const double dev = formatOutput(actual, requested, spread, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING(expected, buf);
    TEST_ASSERT_EQUAL_DOUBLE(expectedDev, dev);
}

static void test_format_output() {
    assertOutput(10000000.3, 10000000ULL, Spread::Off, "Out 10.000 000 MHz (+0.3 Hz)", 0.3);
    assertOutput(10000000.0, 10000000ULL, Spread::Off, "Out 10.000 000 MHz (+0.0 Hz)", 0.0);
    assertOutput(9999999.98, 10000000ULL, Spread::Off, "Out 10.000 000 MHz (+0.0 Hz)", 0.0);
    // Rounded to 1 Hz; the deviation keeps 0.1 Hz.
    assertOutput(9999998.62, 10000000ULL, Spread::Off, "Out 9.999 999 MHz (-1.4 Hz)", -1.4);
    // Integer raster (concept example: 25 MHz x 25 / 88).
    assertOutput(7102272.73, 7100000ULL, Spread::Off, "Out 7.102 273 MHz (+2272.7 Hz)", 2272.7);
    assertOutput(100000000.0, 100000000ULL, Spread::Off, "Out 100.000 000 MHz (+0.0 Hz)", 0.0);
}

static void test_format_output_with_spread() {
    // Mean of a down spread = actual x (1 - amplitude / 2); center spread = actual.
    assertOutput(10000000.0, 10000000ULL, Spread::Down05, "Out 9.975 000 MHz mean, SSC -0.5 %", 0.0);
    assertOutput(10000000.0, 10000000ULL, Spread::Down15, "Out 9.925 000 MHz mean, SSC -1.5 %", 0.0);
    assertOutput(10000000.4, 10000000ULL, Spread::Center05, "Out 10.000 000 MHz mean, SSC +/-0.5 %", 0.0);
    assertOutput(7000000.0, 7000000ULL, Spread::Down05, "Out 6.982 500 MHz mean, SSC -0.5 %", 0.0);
}

static void test_format_output_without_actual() {
    assertOutput(0.0, 10000000ULL, Spread::Off, "Out -", 0.0);
    assertOutput(0.0, 10000000ULL, Spread::Down05, "Out -", 0.0);
    char buf[8];
    formatOutput(10000000.3, 10000000ULL, Spread::Off, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Out 10.", buf); // truncated, terminated
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
    TEST_ASSERT_EQUAL_UINT64(8000ULL, FREQ_MIN_HZ);
    TEST_ASSERT_EQUAL_UINT64(160000000ULL, FREQ_MAX_HZ);
    assertParse("8", "kHz", FREQ_MIN_HZ);
    assertParse("160", "MHz", FREQ_MAX_HZ);
    assertParse("0.5", "MHz", 500000ULL);
    assertParse("455", "kHz", 455000ULL);
    assertParse("150.000001", "MHz", 150000001ULL);
    assertRejected("7.999", "kHz");
    assertRejected("160.000001", "MHz");
    assertRejected("500", "MHz");
    assertRejected("0.005", "MHz");
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

// Audit C-4 / T-4: only digits and one decimal point; strtod alone also took NaN, infinity,
// exponents, hex, signs and leading blanks.
static void test_parse_accepts_only_digits_and_one_point() {
    assertRejected("nan", "Hz");
    assertRejected("NAN", "MHz");
    assertRejected("inf", "MHz");
    assertRejected("infinity", "Hz");
    assertRejected("1e7", "Hz");
    assertRejected("1E1", "MHz");
    assertRejected("0x989680", "Hz");
    assertRejected("1..2", "MHz");
    assertRejected("+10", "MHz");
    assertRejected("-10", "MHz");
    assertRejected(" 10", "MHz");
    assertRejected("10 ", "MHz");
    assertRejected("10,7", "MHz");
    assertRejected("10", "GHz");
    assertRejected("10", "");
    assertRejected("1234567890123456789012345", "Hz");
    assertParse("10.", "MHz", 10000000ULL);
    assertParse("1000.5", "kHz", 1000500ULL);
    assertParse("5000000", nullptr, 5000000ULL); // no unit = Hz
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

static void test_digit_exponent_on_khz_text() {
    const char* t = "455.000 kHz";
    TEST_ASSERT_EQUAL_INT(5, digitExponent(t, 0));
    TEST_ASSERT_EQUAL_INT(3, digitExponent(t, 2));
    TEST_ASSERT_EQUAL_INT(-1, digitExponent(t, 3));
    TEST_ASSERT_EQUAL_INT(0, digitExponent(t, 6));
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
    RUN_TEST(test_format_mhz);
    RUN_TEST(test_spread_label);
    RUN_TEST(test_deviation_rounds_to_tenth);
    RUN_TEST(test_format_output);
    RUN_TEST(test_format_output_with_spread);
    RUN_TEST(test_format_output_without_actual);
    RUN_TEST(test_parse_units);
    RUN_TEST(test_parse_limits);
    RUN_TEST(test_parse_rounds_to_hz);
    RUN_TEST(test_parse_rejects_malformed_input);
    RUN_TEST(test_parse_digit_sweep_matches_integer_math);
    RUN_TEST(test_parse_accepts_only_digits_and_one_point);
    RUN_TEST(test_clamp);
    RUN_TEST(test_step_index_and_snap);
    RUN_TEST(test_digit_exponent_on_formatted_text);
    RUN_TEST(test_digit_exponent_on_khz_text);
    RUN_TEST(test_digit_exponent_matches_step_digits);
    RUN_TEST(test_decimal_exponent_and_power_of_ten);
    return UNITY_END();
}
