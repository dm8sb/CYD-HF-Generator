// Logger with switchable output (V-5, C-16): Serial, ring buffer (while SCPI uses the USB
// serial port) or off; the buffer keeps the newest lines and counts dropped ones.

#include <unity.h>
#include <Arduino.h>
#include <string.h>
#include <utils/logger.h>

void setUp() {
    Fake::serialLog().clear();
    Logger::setOutput(Logger::Output::ToSerial);
    char drain[2048];
    Logger::readBuffered(drain, sizeof(drain));
}
void tearDown() { Logger::setOutput(Logger::Output::ToSerial); }

static void test_serial_output_by_default() {
    Logger::info("APP", "hello");
    Logger::error("SI5351", "lost");
    TEST_ASSERT_TRUE(Fake::serialLog().contains("[APP] hello\n"));
    TEST_ASSERT_TRUE(Fake::serialLog().contains("[ERROR][SI5351] lost\n"));
    TEST_ASSERT_EQUAL_UINT16(0, Logger::buffered());
}

static void test_buffer_keeps_serial_clean() {
    Logger::setOutput(Logger::Output::ToBuffer);
    Logger::info("APP", "one");
    Logger::value("APP", "two", 2);
    TEST_ASSERT_EQUAL_UINT(0, Fake::serialLog().lines); // nothing reaches the SCPI port
    TEST_ASSERT_EQUAL_UINT16(2, Logger::buffered());
    char out[256];
    const uint16_t n = Logger::readBuffered(out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT16(2, n);
    TEST_ASSERT_EQUAL_STRING("[APP] one\n[APP] two: 2\n", out);
    TEST_ASSERT_EQUAL_UINT16(0, Logger::buffered());
}

static void test_buffer_drops_the_oldest_lines() {
    Logger::setOutput(Logger::Output::ToBuffer);
    char line[16];
    for (int i = 0; i < Logger::BUFFER_LINES + 5; ++i) {
        snprintf(line, sizeof(line), "%d", i);
        Logger::info("T", line);
    }
    TEST_ASSERT_EQUAL_UINT16(Logger::BUFFER_LINES, Logger::buffered());
    TEST_ASSERT_EQUAL_UINT32(5, Logger::dropped());
    char out[4096];
    Logger::readBuffered(out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING_LEN("[T] 5\n", out, 6); // oldest kept line first
}

static void test_read_stops_at_the_buffer_size() {
    Logger::setOutput(Logger::Output::ToBuffer);
    Logger::info("APP", "a fairly long line number one");
    Logger::info("APP", "second");
    char out[20];
    TEST_ASSERT_EQUAL_UINT16(0, Logger::readBuffered(out, sizeof(out))); // first line does not fit
    TEST_ASSERT_EQUAL_UINT16(2, Logger::buffered());                     // and stays
    char big[256];
    TEST_ASSERT_EQUAL_UINT16(2, Logger::readBuffered(big, sizeof(big)));
}

static void test_off_drops_everything() {
    Logger::setOutput(Logger::Output::Off);
    Logger::error("APP", "silent");
    TEST_ASSERT_EQUAL_UINT(0, Fake::serialLog().lines);
    TEST_ASSERT_EQUAL_UINT16(0, Logger::buffered());
    Logger::setOutput(Logger::Output::ToSerial);
    Logger::info("APP", "back");
    TEST_ASSERT_TRUE(Fake::serialLog().contains("[APP] back"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_serial_output_by_default);
    RUN_TEST(test_buffer_keeps_serial_clean);
    RUN_TEST(test_buffer_drops_the_oldest_lines);
    RUN_TEST(test_read_stops_at_the_buffer_size);
    RUN_TEST(test_off_drops_everything);
    return UNITY_END();
}
