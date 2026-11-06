// Sanitize::text (W-S9, S-7, S-14): foreign strings become valid UTF-8 without control
// characters, at most 32 bytes, before LVGL and JSON see them.

#include <unity.h>
#include <string.h>
#include <string>
#include <utils/sanitize.h>

void setUp() {}
void tearDown() {}

static std::string clean(const std::string& in, size_t maxBytes = 32) {
    char out[64];
    const size_t n = Sanitize::text(in.data(), in.size(), out, sizeof(out), maxBytes);
    TEST_ASSERT_EQUAL_size_t(strlen(out), n);
    return out;
}

static void test_plain_and_utf8_pass() {
    TEST_ASSERT_EQUAL_STRING("Lab WiFi 2.4", clean("Lab WiFi 2.4").c_str());
    TEST_ASSERT_EQUAL_STRING("B\xC3\xBCro \xE2\x82\xAC \xF0\x9F\x93\xA1", clean("B\xC3\xBCro \xE2\x82\xAC \xF0\x9F\x93\xA1").c_str());
    // Markup stays text (LVGL recolor is never enabled; the web shows it with textContent).
    TEST_ASSERT_EQUAL_STRING("<svg onload=alert(1)>", clean("<svg onload=alert(1)>").c_str());
    TEST_ASSERT_EQUAL_STRING("a\"b\\c #ff0000 x#", clean("a\"b\\c #ff0000 x#").c_str());
}

static void test_invalid_sequences_become_question_marks() {
    TEST_ASSERT_EQUAL_STRING("?A", clean("\xF0" "A").c_str());          // lead byte without continuation
    TEST_ASSERT_EQUAL_STRING("x?", clean("x\xC3").c_str());              // truncated at the end
    TEST_ASSERT_EQUAL_STRING("??", clean("\x80\xBF").c_str());           // lone continuation bytes
    TEST_ASSERT_EQUAL_STRING("??", clean("\xC0\xAF").c_str());           // overlong '/'
    TEST_ASSERT_EQUAL_STRING("???", clean("\xED\xA0\x80").c_str());      // surrogate U+D800
    TEST_ASSERT_EQUAL_STRING("????", clean("\xF4\x90\x80\x80").c_str()); // above U+10FFFF
    TEST_ASSERT_EQUAL_STRING("??", clean("\xFE\xFF").c_str());
}

static void test_control_characters() {
    TEST_ASSERT_EQUAL_STRING("x?y", clean("x\ny").c_str());
    TEST_ASSERT_EQUAL_STRING("a?b?c", clean(std::string("a\0b\tc", 5)).c_str());
    TEST_ASSERT_EQUAL_STRING("?[31m", clean("\x1b[31m").c_str());
    TEST_ASSERT_EQUAL_STRING("x?", clean("x\x7F").c_str());
    TEST_ASSERT_EQUAL_STRING("?", clean("\xC2\x85").c_str()); // C1 NEL
}

static void test_length_limit_on_character_boundary() {
    TEST_ASSERT_EQUAL_STRING(std::string(32, 'a').c_str(), clean(std::string(40, 'a')).c_str());
    // 31 bytes + a 2-byte character: the character does not fit and is left out.
    const std::string in = std::string(31, 'a') + "\xC3\xBC" + "b";
    TEST_ASSERT_EQUAL_STRING(std::string(31, 'a').c_str(), clean(in).c_str());
    TEST_ASSERT_EQUAL_STRING("ab", clean("abc", 2).c_str());
    char small[4];
    TEST_ASSERT_EQUAL_size_t(3, Sanitize::text("abcdef", 6, small, sizeof(small)));
    TEST_ASSERT_EQUAL_STRING("abc", small);
    TEST_ASSERT_EQUAL_size_t(0, Sanitize::text(nullptr, 5, small, sizeof(small)));
    TEST_ASSERT_EQUAL_STRING("", small);
}

// Random bytes with a fixed seed: the output is always valid UTF-8 without controls.
static void test_random_input_gives_valid_utf8() {
    uint32_t x = 4711;
    for (int round = 0; round < 20000; ++round) {
        unsigned char in[48];
        const size_t n = (x >> 3) % sizeof(in);
        for (size_t i = 0; i < n; ++i) {
            x = x * 1103515245u + 12345u;
            in[i] = (unsigned char)(x >> 16);
        }
        x = x * 1103515245u + 12345u;
        char out[40];
        const size_t len = Sanitize::text((const char*)in, n, out, sizeof(out));
        TEST_ASSERT_TRUE(len <= 32);
        char again[40];
        // Valid output passes unchanged (idempotent).
        Sanitize::text(out, len, again, sizeof(again));
        TEST_ASSERT_EQUAL_STRING(out, again);
        for (size_t i = 0; i < len; ++i) TEST_ASSERT_TRUE((unsigned char)out[i] >= 0x20 && out[i] != 0x7F);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plain_and_utf8_pass);
    RUN_TEST(test_invalid_sequences_become_question_marks);
    RUN_TEST(test_control_characters);
    RUN_TEST(test_length_limit_on_character_boundary);
    RUN_TEST(test_random_input_gives_valid_utf8);
    return UNITY_END();
}
