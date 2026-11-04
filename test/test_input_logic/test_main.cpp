// GUI input logic without LVGL (audit T-10, T-11): numpad editing and confirmation, tap on a
// digit of the frequency label, colored step digit.

#include <unity.h>
#include <constants.h>
#include <utils/frequency_utils.h>
#include <utils/input_logic.h>
#include <utils/message_queue.h>
#include <utils/status_text.h>
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
    // The 10th character is not ignored silently (audit U-9).
    TEST_ASSERT_EQUAL(NumpadEvent::Rejected, numpadKey(n, "1", nullptr));
    TEST_ASSERT_EQUAL(NumpadError::TooLong, n.error);
    TEST_ASSERT_EQUAL_STRING("123456789", n.text);
    // Backspace still works and clears the error.
    TEST_ASSERT_EQUAL(NumpadEvent::Edited, numpadKey(n, KEY_BACKSPACE, nullptr));
    TEST_ASSERT_EQUAL(NumpadError::None, n.error);
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

// MESSPLAN B3: "500" MHz and "0.5" MHz are rejected and marked; the input stays, so it can be
// corrected (audit U-9).
static void test_numpad_rejects_out_of_range_and_empty() {
    struct Case {
        const char* text;
        NumpadError error;
    };
    const Case bad[] = {{"500", NumpadError::OutOfRange}, {"0.5", NumpadError::OutOfRange},
                        {"", NumpadError::Empty}, {".", NumpadError::Empty}};
    for (const Case& c : bad) {
        Numpad n = typed(c.text);
        uint64_t hz = 42;
        TEST_ASSERT_EQUAL_MESSAGE(NumpadEvent::Rejected, numpadKey(n, "MHz", &hz), c.text);
        TEST_ASSERT_EQUAL_UINT64(42, hz);
        TEST_ASSERT_EQUAL_MESSAGE(c.error, n.error, c.text);
        TEST_ASSERT_EQUAL_STRING(c.text, n.text);
        // The next key clears the error mark.
        numpadKey(n, KEY_BACKSPACE, nullptr);
        TEST_ASSERT_EQUAL(NumpadError::None, n.error);
    }
    // Corrected after the error: "500" -> "50" MHz.
    Numpad n = typed("500");
    numpadKey(n, "MHz", nullptr);
    numpadKey(n, KEY_BACKSPACE, nullptr);
    uint64_t hz = 0;
    TEST_ASSERT_EQUAL(NumpadEvent::Confirmed, numpadKey(n, "MHz", &hz));
    TEST_ASSERT_EQUAL_UINT64(50000000ULL, hz);
}

static void test_numpad_error_texts() {
    TEST_ASSERT_EQUAL_STRING("", numpadErrorText(NumpadError::None));
    TEST_ASSERT_EQUAL_STRING("Enter a number", numpadErrorText(NumpadError::Empty));
    TEST_ASSERT_EQUAL_STRING("Allowed 1 - 100 MHz", numpadErrorText(NumpadError::OutOfRange));
    TEST_ASSERT_EQUAL_STRING("Max. 9 characters", numpadErrorText(NumpadError::TooLong));
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

// A tap on a point or space selects the nearer neighbor (audit U-15): the targets are only
// 1.8-3.4 mm wide.
static void test_tap_index_assigns_separators_to_neighbors() {
    const char* text = "10.700 000 MHz"; // 10 px per character
    TEST_ASSERT_EQUAL_UINT(1, tapIndexAt(text, 22, width10, nullptr)); // left half of '.'
    TEST_ASSERT_EQUAL_UINT(3, tapIndexAt(text, 27, width10, nullptr)); // right half of '.'
    TEST_ASSERT_EQUAL_UINT(5, tapIndexAt(text, 61, width10, nullptr)); // left half of ' '
    TEST_ASSERT_EQUAL_UINT(7, tapIndexAt(text, 66, width10, nullptr)); // right half of ' '
    TEST_ASSERT_EQUAL_UINT(9, tapIndexAt(text, 101, width10, nullptr)); // ' ' before the unit
    TEST_ASSERT_EQUAL_UINT(11, tapIndexAt(text, 107, width10, nullptr)); // unit side
    TEST_ASSERT_EQUAL_UINT(4, tapIndexAt(text, 45, width10, nullptr)); // digits unchanged
    TEST_ASSERT_EQUAL_UINT(13, tapIndexAt(text, 500, width10, nullptr));
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

// ---------------------------------------------------------------------------
// Message queue (audit T-12, A-38)

static bool isLink(const char* text) { return strncmp(text, "Link", 4) == 0; }

static void test_queue_one_open_rest_waits_in_order() {
    MessageQueue q;
    TEST_ASSERT_EQUAL(MessageQueue::Post::Open, q.post("SI5351", "Link lost"));
    TEST_ASSERT_EQUAL(MessageQueue::Post::Queued, q.post("SI5351", "Frequency set failed"));
    TEST_ASSERT_EQUAL(MessageQueue::Post::Queued, q.post("Settings", "Save failed"));
    TEST_ASSERT_EQUAL_STRING("Link lost", q.openText());
    const MessageQueue::Message* m = q.close();
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_STRING("Frequency set failed", q.openText());
    m = q.close();
    TEST_ASSERT_EQUAL_STRING("Settings", m->title);
    TEST_ASSERT_NULL(q.close());
    TEST_ASSERT_FALSE(q.isOpen());
}

static void test_queue_drops_duplicates_and_overflow() {
    MessageQueue q;
    q.post("A", "one");
    TEST_ASSERT_EQUAL(MessageQueue::Post::Duplicate, q.post("A", "one"));
    q.post("A", "two");
    TEST_ASSERT_EQUAL(MessageQueue::Post::Duplicate, q.post("B", "two"));
    q.post("A", "three");
    q.post("A", "four");
    TEST_ASSERT_EQUAL_UINT8(MessageQueue::CAPACITY, q.queued());
    TEST_ASSERT_EQUAL(MessageQueue::Post::Full, q.post("A", "five"));
    // A long text is compared as stored (truncated).
    char longText[300];
    memset(longText, 'x', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = 0;
    MessageQueue r;
    r.post("A", longText);
    TEST_ASSERT_EQUAL(MessageQueue::Post::Duplicate, r.post("A", longText));
}

static void test_queue_drops_link_messages_after_reconnect() {
    MessageQueue q;
    q.post("SI5351", "Link lost");
    q.post("SI5351", "Frequency set failed");
    q.post("SI5351", "Link stays low");
    TEST_ASSERT_TRUE(q.dropMatching(isLink)); // the open one is a link message
    TEST_ASSERT_EQUAL_UINT8(1, q.queued());
    q.close();
    TEST_ASSERT_EQUAL_STRING("Frequency set failed", q.openText());
    TEST_ASSERT_FALSE(q.dropMatching(isLink));
}

// ---------------------------------------------------------------------------
// IQ limit text (audit T-13, C-11)

static void test_iq_limit_is_rounded_towards_the_inside() {
    char buf[16];
    FrequencyUtils::formatIqLimit(4730000, true, 3, false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("4.730", buf);
    FrequencyUtils::formatIqLimit(4748151, true, 3, false, buf, sizeof(buf)); // -0.5 % spread
    TEST_ASSERT_EQUAL_STRING("4.749", buf);
    FrequencyUtils::formatIqLimit(4748151, true, 2, false, buf, sizeof(buf)); // MAIN
    TEST_ASSERT_EQUAL_STRING("4.75", buf);
    FrequencyUtils::formatIqLimit(99502487, false, 3, false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("99.502", buf);
    FrequencyUtils::formatIqLimit(99502487, false, 2, false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("99.50", buf);
    FrequencyUtils::formatIqLimit(100000000, false, 3, true, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100", buf);
    FrequencyUtils::formatIqLimit(100000000, false, 3, false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100.000", buf);
}

// ---------------------------------------------------------------------------
// State texts of MAIN and the headers (audit U-1)

static void test_status_texts_show_the_actual_state() {
    using namespace StatusText;
    TEST_ASSERT_EQUAL_STRING("ON", channel(ChannelStatus::On).text);
    TEST_ASSERT_EQUAL(Tone::Ok, channel(ChannelStatus::On).tone);
    TEST_ASSERT_EQUAL_STRING("OFF", channel(ChannelStatus::Off).text);
    TEST_ASSERT_EQUAL(Tone::Muted, channel(ChannelStatus::Off).tone);
    TEST_ASSERT_EQUAL_STRING("! ERR", channel(ChannelStatus::Error).text);
    TEST_ASSERT_EQUAL_STRING("! NO HW", channel(ChannelStatus::NoDevice).text);
    TEST_ASSERT_EQUAL(Tone::Error, channel(ChannelStatus::NoDevice).tone);
    // CH3 switched on but waiting is not "ON" (before, the header showed the request).
    TEST_ASSERT_EQUAL_STRING("WAIT", ch3(Ch3Status::MasterOff).text);
    TEST_ASSERT_EQUAL_STRING("WAIT", ch3(Ch3Status::FreqTooLow).text);
    TEST_ASSERT_EQUAL_STRING("WAIT", ch3(Ch3Status::FreqTooHigh).text);
    TEST_ASSERT_EQUAL(Tone::Warn, ch3(Ch3Status::FreqTooHigh).tone);
    TEST_ASSERT_EQUAL_STRING("ON", ch3(Ch3Status::Active).text);
    TEST_ASSERT_EQUAL_STRING("! ERR", ch3(Ch3Status::Error).text);
    TEST_ASSERT_EQUAL_STRING("! NO HW", ch3(Ch3Status::NoDevice).text);
    TEST_ASSERT_EQUAL_STRING("OFF", ch3(Ch3Status::Off).text);
}

// Special modes line (audit U-2)
static void test_mode_summary_lists_only_deviations() {
    using namespace StatusText;
    char buf[64];
    TEST_ASSERT_EQUAL(Tone::Muted, modeSummary(TuningMode::Fractional, Spread::Off, DEFAULT_DRIVE_MA, OffState::Low, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_EQUAL(Tone::Warn, modeSummary(TuningMode::LowNoise, Spread::Down05, 8, OffState::High, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("OFF = High | SSC -0.5 % | Low noise | 8 mA", buf);
    TEST_ASSERT_EQUAL(Tone::Muted, modeSummary(TuningMode::Integer, Spread::Off, 2, OffState::HighZ, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("OFF = High-Z | Integer | 2 mA", buf);
    modeSummary(TuningMode::Fractional, Spread::Center05, DEFAULT_DRIVE_MA, OffState::Low, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("SSC +/-0.5 %", buf);
    char small[12];
    modeSummary(TuningMode::LowNoise, Spread::Down15, 8, OffState::High, small, sizeof(small));
    TEST_ASSERT_TRUE(strlen(small) < sizeof(small));
}

// Buttons at the screen edge take taps up to the edge (audit U-5)
static void test_edge_area_reaches_only_near_edges() {
    using namespace InputLogic;
    // MAIN in the header (254..315 x 1..36): to the right and top edge, not left or down.
    Area a = edgeArea({254, 1, 315, 36}, EDGE_REACH, 320, 240);
    TEST_ASSERT_EQUAL(254, a.x1);
    TEST_ASSERT_EQUAL(0, a.y1);
    TEST_ASSERT_EQUAL(319, a.x2);
    TEST_ASSERT_EQUAL(36, a.y2);
    TEST_ASSERT_TRUE(contains(a, 319, 0)); // clamped corner tap
    TEST_ASSERT_FALSE(contains(a, 253, 10));
    TEST_ASSERT_FALSE(contains(a, 300, 37));
    // CH1 at the bottom left (8..79 x 194..235): left and bottom edge.
    a = edgeArea({8, 194, 79, 235}, EDGE_REACH, 320, 240);
    TEST_ASSERT_TRUE(contains(a, 0, 239));
    TEST_ASSERT_FALSE(contains(a, 80, 239)); // the gap to CH2 stays free
    TEST_ASSERT_FALSE(contains(a, 40, 193));
    // A button in the middle keeps its area.
    a = edgeArea({100, 100, 150, 140}, EDGE_REACH, 320, 240);
    TEST_ASSERT_EQUAL(100, a.x1);
    TEST_ASSERT_EQUAL(100, a.y1);
    TEST_ASSERT_EQUAL(150, a.x2);
    TEST_ASSERT_EQUAL(140, a.y2);
    // 9 px from the edge is too far.
    a = edgeArea({9, 9, 50, 50}, EDGE_REACH, 320, 240);
    TEST_ASSERT_EQUAL(9, a.x1);
    TEST_ASSERT_EQUAL(9, a.y1);
}

// Output error dialogs name the channel and what happens next (audit U-21).
static void test_output_error_texts_name_channel_and_retry() {
    using namespace StatusText;
    char buf[160];
    outputErrorText(2, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("CH2: frequency setup failed. Retrying every 2 s.", buf);
    outputErrorText(3, 1, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("CH3: IQ setup failed, CLK2 stays off. CH1 keeps running. Retrying every 2 s.", buf);
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
    RUN_TEST(test_queue_one_open_rest_waits_in_order);
    RUN_TEST(test_queue_drops_duplicates_and_overflow);
    RUN_TEST(test_queue_drops_link_messages_after_reconnect);
    RUN_TEST(test_iq_limit_is_rounded_towards_the_inside);
    RUN_TEST(test_status_texts_show_the_actual_state);
    RUN_TEST(test_mode_summary_lists_only_deviations);
    RUN_TEST(test_edge_area_reaches_only_near_edges);
    RUN_TEST(test_numpad_error_texts);
    RUN_TEST(test_tap_index_assigns_separators_to_neighbors);
    RUN_TEST(test_output_error_texts_name_channel_and_retry);
    return UNITY_END();
}
