// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Contrast of the GUI palette (audit U-7): WCAG 2.x contrast ratios of the color pairs that
// the pages use. Text needs 4.2:1 or more (the channel colors cannot reach 4.5:1 with any
// text color), non-text parts (switch track, chip border, dialog button) 3:1 (WCAG 1.4.11).

#include <unity.h>
#include <palette.h>
#include <utils/contrast.h>

using namespace Palette;

void setUp() {}
void tearDown() {}

static void test_contrast_ratio_matches_wcag() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.0f, Contrast::ratio(0xFFFFFF, 0x000000));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, Contrast::ratio(0x777777, 0x777777));
    // Symmetric.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, Contrast::ratio(CH1, TEXT), Contrast::ratio(TEXT, CH1));
    // Known value: white on CH2 green 2.86:1 (audit U-7).
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 2.86f, Contrast::ratio(TEXT, CH2));
}

static void test_text_on_channel_colors_is_dark() {
    const uint32_t channels[] = {CH1, CH2, CH3, CONFIG};
    for (uint32_t bg : channels) {
        TEST_ASSERT_EQUAL_HEX32(TEXT_DARK, Contrast::textOn(bg));
        TEST_ASSERT_TRUE(Contrast::ratio(Contrast::textOn(bg), bg) >= 4.2f);
    }
}

static void test_text_on_dark_buttons_stays_light() {
    const uint32_t dark[] = {GRAY, CHIP_OFF, CARD_BG};
    for (uint32_t bg : dark) {
        TEST_ASSERT_EQUAL_HEX32(TEXT, Contrast::textOn(bg));
        TEST_ASSERT_TRUE(Contrast::ratio(TEXT, bg) >= 4.5f);
    }
}

static void test_state_labels_are_readable() {
    // Filled state labels (ON, WAIT, ERR, NO HW) with dark text.
    const uint32_t tones[] = {TONE_OK, TONE_WARN, TONE_ERROR, DIALOG_BUTTON};
    for (uint32_t bg : tones) {
        TEST_ASSERT_EQUAL_HEX32(TEXT_DARK, Contrast::textOn(bg));
        TEST_ASSERT_TRUE(Contrast::ratio(TEXT_DARK, bg) >= 4.5f);
    }
    // OFF and detail lines: muted on the card.
    TEST_ASSERT_TRUE(Contrast::ratio(MUTED, CARD_BG) >= 4.5f);
}

static void test_non_text_parts_reach_three_to_one() {
    const uint32_t backgrounds[] = {SCREEN_BG, HEADER_BG, CARD_BG};
    for (uint32_t bg : backgrounds) {
        TEST_ASSERT_TRUE(Contrast::ratio(OUTLINE, bg) >= 3.0f); // switch track off, chip border
        TEST_ASSERT_TRUE(Contrast::ratio(CH1, bg) >= 3.0f);     // switch track on
        TEST_ASSERT_TRUE(Contrast::ratio(CH2, bg) >= 3.0f);
        TEST_ASSERT_TRUE(Contrast::ratio(CH3, bg) >= 3.0f);
    }
    TEST_ASSERT_TRUE(Contrast::ratio(OUTLINE, KNOB) >= 3.0f);
    TEST_ASSERT_TRUE(Contrast::ratio(DIALOG_BUTTON, CARD_BG) >= 3.0f);
}

// MAIN status bar (S53): WiFi symbol / AP text in ok, warning or muted color on the bar,
// WiFi line of the info card in muted or warning color on the card.
static void test_status_bar_texts_are_readable() {
    const uint32_t colors[] = {TONE_OK, TONE_WARN, MUTED};
    for (uint32_t fg : colors) TEST_ASSERT_TRUE(Contrast::ratio(fg, HEADER_BG) >= 4.5f);
    TEST_ASSERT_TRUE(Contrast::ratio(TONE_WARN, CARD_BG) >= 4.5f);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_status_bar_texts_are_readable);
    RUN_TEST(test_contrast_ratio_matches_wcag);
    RUN_TEST(test_text_on_channel_colors_is_dark);
    RUN_TEST(test_text_on_dark_buttons_stays_light);
    RUN_TEST(test_state_labels_are_readable);
    RUN_TEST(test_non_text_parts_reach_three_to_one);
    return UNITY_END();
}
