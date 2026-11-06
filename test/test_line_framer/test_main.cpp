// LineFramer (W-S17, S-11) and the HTTP detection for TCP 5025 (W-S18, S-6).

#include <unity.h>
#include <string.h>
#include <string>
#include <remote/line_framer.h>

void setUp() {}
void tearDown() {}

// Feeds text; returns the events (L = line, T = too long, B = bad char, M = too many commands)
// and collects the lines separated by '|'.
static std::string feed(LineFramer& f, const std::string& text, std::string* lines = nullptr) {
    std::string events;
    for (char c : text) {
        const LineFramer::Event e = f.feed(c);
        switch (e) {
            case LineFramer::Event::None: break;
            case LineFramer::Event::Line:
                events += 'L';
                TEST_ASSERT_EQUAL_UINT8('\0', f.line()[f.length()]);
                if (lines) {
                    if (!lines->empty()) *lines += '|';
                    *lines += std::string(f.line(), f.length());
                }
                break;
            case LineFramer::Event::TooLong: events += 'T'; break;
            case LineFramer::Event::BadChar: events += 'B'; break;
            case LineFramer::Event::TooManyCommands: events += 'M'; break;
        }
    }
    return events;
}

static void test_lines_cr_and_blank_lines() {
    LineFramer f;
    std::string lines;
    TEST_ASSERT_EQUAL_STRING("LL", feed(f, "*IDN?\r\n\n  \r\nFREQ 7 MHZ\n", &lines).c_str());
    TEST_ASSERT_EQUAL_STRING("*IDN?|FREQ 7 MHZ", lines.c_str());
    // Split over several reads; a missing LF keeps the line pending.
    lines.clear();
    TEST_ASSERT_EQUAL_STRING("", feed(f, "OUTP1 ").c_str());
    TEST_ASSERT_EQUAL_STRING("L", feed(f, "ON\nOUTP2", &lines).c_str());
    TEST_ASSERT_EQUAL_STRING("OUTP1 ON", lines.c_str());
}

static void test_overlong_line_is_discarded_up_to_lf() {
    LineFramer f;
    std::string lines;
    const std::string max(LineFramer::MAX_LINE, 'A');
    TEST_ASSERT_EQUAL_STRING("L", feed(f, max + "\n", &lines).c_str());
    TEST_ASSERT_EQUAL_size_t(LineFramer::MAX_LINE, lines.size());
    lines.clear();
    // One byte more: discarded completely, also what follows up to the LF; the next line works.
    TEST_ASSERT_EQUAL_STRING("TL", feed(f, max + "B;*RST\n*IDN?\n", &lines).c_str());
    TEST_ASSERT_EQUAL_STRING("*IDN?", lines.c_str());
    TEST_ASSERT_EQUAL_INT16(-363, framerErrorCode(LineFramer::Event::TooLong));
}

static void test_control_and_non_ascii_bytes_discard_the_line() {
    LineFramer f;
    std::string lines;
    TEST_ASSERT_EQUAL_STRING("B", feed(f, std::string("OUTP1 ON\0;*RST\n", 15)).c_str());
    TEST_ASSERT_EQUAL_STRING("B", feed(f, "FREQ 7\xC3\xA4MHZ\n").c_str());
    TEST_ASSERT_EQUAL_STRING("B", feed(f, "\x1b[A\n").c_str());
    TEST_ASSERT_EQUAL_STRING("B", feed(f, "OUTP1 ON\x7F\n").c_str());
    // TAB is a blank.
    TEST_ASSERT_EQUAL_STRING("L", feed(f, "FREQ\t7MHZ\n", &lines).c_str());
    TEST_ASSERT_EQUAL_INT16(-101, framerErrorCode(LineFramer::Event::BadChar));
    TEST_ASSERT_EQUAL_INT16(0, framerErrorCode(LineFramer::Event::Line));
}

static void test_command_count_outside_quotes() {
    LineFramer f;
    std::string sixteen = "*OPC";
    for (int i = 1; i < LineFramer::MAX_COMMANDS; ++i) sixteen += ";*OPC";
    TEST_ASSERT_EQUAL_STRING("L", feed(f, sixteen + "\n").c_str());
    TEST_ASSERT_EQUAL_STRING("M", feed(f, sixteen + ";*OPC\n").c_str());
    TEST_ASSERT_EQUAL_STRING("M", feed(f, std::string(40, ';') + "\n").c_str());
    // ';' inside strings does not count.
    TEST_ASSERT_EQUAL_STRING("L", feed(f, "X \"" + std::string(20, ';') + "\";'" + std::string(20, ';') + "'\n").c_str());
    TEST_ASSERT_EQUAL_INT16(-223, framerErrorCode(LineFramer::Event::TooManyCommands));
}

static void test_reset_drops_a_partial_line() {
    LineFramer f;
    std::string lines;
    feed(f, "OUTP1 O");
    f.reset();
    TEST_ASSERT_EQUAL_STRING("L", feed(f, "*IDN?\n", &lines).c_str());
    TEST_ASSERT_EQUAL_STRING("*IDN?", lines.c_str());
}

static bool http(const char* s) { return looksLikeHttp(s, strlen(s)); }

static void test_http_detection() {
    TEST_ASSERT_TRUE(http("GET / HTTP/1.1"));
    TEST_ASSERT_TRUE(http("POST /api/channel/1 HTTP/1.0"));
    TEST_ASSERT_TRUE(http("Host: 192.168.1.50:5025"));
    TEST_ASSERT_TRUE(http("Content-Type: text/plain"));
    TEST_ASSERT_TRUE(http("sec-fetch-site:\tcross-site"));
    TEST_ASSERT_FALSE(http("*IDN?"));
    TEST_ASSERT_FALSE(http("SYST:ERR?"));
    TEST_ASSERT_FALSE(http("OUTP1:STAT ON"));
    TEST_ASSERT_FALSE(http("FREQ 7 MHZ"));
    TEST_ASSERT_FALSE(http("GET / HTTP/2"));
    TEST_ASSERT_FALSE(http("GET  HTTP/1.1"));
    TEST_ASSERT_FALSE(http(": x"));
    TEST_ASSERT_FALSE(looksLikeHttp(nullptr, 0));
}

// Random bytes with a fixed seed: every line stays within the limits and holds only
// printable ASCII or TAB (fuzzing proper follows in S57).
static void test_random_input_keeps_the_invariants() {
    LineFramer f;
    uint32_t x = 12345;
    unsigned lines = 0;
    for (int i = 0; i < 200000; ++i) {
        x = x * 1103515245u + 12345u;
        uint8_t b = (uint8_t)(x >> 16);
        if ((x >> 8) % 7 == 0) b = '\n';
        else if ((x >> 8) % 5 == 0) b = (uint8_t)(' ' + b % 95);
        if (f.feed((char)b) != LineFramer::Event::Line) continue;
        ++lines;
        TEST_ASSERT_TRUE(f.length() <= LineFramer::MAX_LINE);
        for (size_t k = 0; k < f.length(); ++k) {
            const unsigned char c = (unsigned char)f.line()[k];
            TEST_ASSERT_TRUE(c == '\t' || (c >= 0x20 && c < 0x7F));
        }
    }
    TEST_ASSERT_TRUE(lines > 100);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_lines_cr_and_blank_lines);
    RUN_TEST(test_overlong_line_is_discarded_up_to_lf);
    RUN_TEST(test_control_and_non_ascii_bytes_discard_the_line);
    RUN_TEST(test_command_count_outside_quotes);
    RUN_TEST(test_reset_drops_a_partial_line);
    RUN_TEST(test_http_detection);
    RUN_TEST(test_random_input_keeps_the_invariants);
    return UNITY_END();
}
