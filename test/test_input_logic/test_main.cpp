// GUI input logic without LVGL (audit T-10, T-11): numpad editing and confirmation, tap on a
// digit of the frequency label, colored step digit.

#include <unity.h>
#include <constants.h>
#include <utils/frequency_utils.h>
#include <utils/input_logic.h>
#include <string.h>

using namespace InputLogic;

void setUp() {}
void tearDown() {}

static Numpad typed(const char* keys) {
    Numpad n;
    char k[2] = {0, 0};
    for (const char* p = keys; *p; ++p) {
        k[0] = *p;
        numpadKey(n, k, nullptr);
    }
    return n;
}

static void test_numpad_digits_and_one_point() {
    Numpad n = typed("10.7.5");
    TEST_ASSERT_EQUAL_STRING("10.75", n.text);
    TEST_ASSERT_EQUAL(NumpadEvent::Ignored, numpadKey(n, ".", nullptr));
    TEST_ASSERT_EQUAL(NumpadEvent::Ignored, numpadKey(n, "x", nullptr));
    TEST_ASSERT_EQUAL(NumpadEvent::Ignored, numpadKey(n, nullptr, nullptr));
}

static void test_numpad_length_limit() {
    Numpad n = typed("1234567890");
    TEST_ASSERT_EQUAL_STRING("123456789", n.text);
    TEST_ASSERT_EQUAL_UINT(NUMPAD_MAX_LEN, n.len);
    TEST_ASSERT_EQUAL(NumpadEvent::Ignored, numpadKey(n, "1", nullptr));
}

static void test_numpad_backspace_and_clear() {
    Numpad n = typed("14.2");
    TEST_ASSERT_EQUAL(NumpadEvent::Edited, numpadKey(n, KEY_BACKSPACE, nullptr));
    TEST_ASSERT_EQUAL_STRING("14.", n.text);
    TEST_ASSERT_EQUAL(NumpadEvent::Edited, numpadKey(n, "C", nullptr));
    TEST_ASSERT_EQUAL_STRING("", n.text);
    TEST_ASSERT_EQUAL(NumpadEvent::Ignored, numpadKey(n, KEY_BACKSPACE, nullptr));
    // After backspace over the point a new point is allowed again.
    n = typed("1.");
    numpadKey(n, KEY_BACKSPACE, nullptr);
    TEST_ASSERT_EQUAL(NumpadEvent::Edited, numpadKey(n, ".", nullptr));
}

static void test_numpad_units_confirm() {
    uint64_t hz = 0;
    Numpad n = typed("10.7");
    TEST_ASSERT_EQUAL(NumpadEvent::Confirmed, numpadKey(n, "MHz", &hz));
    TEST_ASSERT_EQUAL_UINT64(10700000ULL, hz);
    n = typed("1000");
    TEST_ASSERT_EQUAL(NumpadEvent::Confirmed, numpadKey(n, "kHz", &hz));
    TEST_ASSERT_EQUAL_UINT64(1000000ULL, hz);
    n = typed("5000000");
    TEST_ASSERT_EQUAL(NumpadEvent::Confirmed, numpadKey(n, "Hz", &hz));
    TEST_ASSERT_EQUAL_UINT64(5000000ULL, hz);
}

// MESSPLAN B3: "500" MHz and "0.5" MHz are rejected, the field is cleared and marked.
static void test_numpad_rejects_out_of_range_and_empty() {
    const char* bad[] = {"500", "0.5", "", "."};
    for (const char* t : bad) {
        Numpad n = typed(t);
        uint64_t hz = 42;
        TEST_ASSERT_EQUAL_MESSAGE(NumpadEvent::Rejected, numpadKey(n, "MHz", &hz), t);
        TEST_ASSERT_EQUAL_UINT64(42, hz);
        TEST_ASSERT_TRUE(n.error);
        TEST_ASSERT_EQUAL_STRING("", n.text);
        // The next key clears the error mark.
        numpadKey(n, "1", nullptr);
        TEST_ASSERT_FALSE(n.error);
    }
}

// Monospaced test font: every character 10 px.
static int width10(char, char, void*) { return 10; }

static void test_char_index_at() {
    const char* text = "10.700 000 MHz";
    TEST_ASSERT_EQUAL_UINT(0, charIndexAt(text, 0, width10, nullptr));
    TEST_ASSERT_EQUAL_UINT(0, charIndexAt(text, 9, width10, nullptr));
    TEST_ASSERT_EQUAL_UINT(1, charIndexAt(text, 10, width10, nullptr));
    TEST_ASSERT_EQUAL_UINT(13, charIndexAt(text, 139, width10, nullptr));
    TEST_ASSERT_EQUAL_UINT(13, charIndexAt(text, 500, width10, nullptr)); // right of the text
    TEST_ASSERT_EQUAL_UINT(0, charIndexAt(text, -5, width10, nullptr));
}

static void test_tap_target() {
    const char* text = "10.700 000 MHz";
    uint32_t step = 0;
    TEST_ASSERT_EQUAL(TapTarget::Step, tapTarget(text, 5, &step)); // 1 kHz digit
    TEST_ASSERT_EQUAL_UINT32(1000, step);
    TEST_ASSERT_EQUAL(TapTarget::Step, tapTarget(text, 9, &step)); // 1 Hz digit
    TEST_ASSERT_EQUAL_UINT32(1, step);
    TEST_ASSERT_EQUAL(TapTarget::Step, tapTarget(text, 1, &step)); // 1 MHz digit
    TEST_ASSERT_EQUAL_UINT32(1000000, step);
    TEST_ASSERT_EQUAL(TapTarget::Step, tapTarget(text, 0, &step)); // 10 MHz -> largest step
    TEST_ASSERT_EQUAL_UINT32(DELTA_STEPS[DELTA_STEP_COUNT - 1], step);
    TEST_ASSERT_EQUAL(TapTarget::None, tapTarget(text, 2, &step)); // point
    TEST_ASSERT_EQUAL(TapTarget::None, tapTarget(text, 6, &step)); // space
    TEST_ASSERT_EQUAL(TapTarget::Numpad, tapTarget(text, 11, &step));
    TEST_ASSERT_EQUAL(TapTarget::Numpad, tapTarget(text, 13, &step));
    TEST_ASSERT_EQUAL(TapTarget::None, tapTarget(text, 99, &step));
}

static void test_step_markup() {
    char out[64];
    stepMarkup("10.700 000 MHz", 1000, 0xFF0000, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("10.70#FF0000 0# 000 MHz", out);
    stepMarkup("100.000 000 MHz", 1000000, 0x00FF00, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("10#00FF00 0#.000 000 MHz", out);
    char small[16];
    stepMarkup("10.700 000 MHz", 1, 0x123456, small, sizeof(small));
    TEST_ASSERT_TRUE(strlen(small) < sizeof(small));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_numpad_digits_and_one_point);
    RUN_TEST(test_numpad_length_limit);
    RUN_TEST(test_numpad_backspace_and_clear);
    RUN_TEST(test_numpad_units_confirm);
    RUN_TEST(test_numpad_rejects_out_of_range_and_empty);
    RUN_TEST(test_char_index_at);
    RUN_TEST(test_tap_target);
    RUN_TEST(test_step_markup);
    return UNITY_END();
}
