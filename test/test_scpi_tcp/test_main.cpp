// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// SCPI over TCP 5025 (W-E13, W-S18, W-S20): ScpiTcpLink with a fake socket against the real
// controller and the SI5351 register model: query/answer, one connection, HTTP lines close
// without execution, idle timeout, framing errors, allowed/not allowed, partial reads and writes.

#include <unity.h>
#include <Preferences.h>
#include <Wire.h>
#include <string.h>
#include <string>
#include <config.h>
#include <constants.h>
#include <core/app_controller.h>
#include <hal/i2c_bus.h>
#include <remote/scpi.h>
#include <remote/scpi_tcp.h>
#include <remote/scpi_usb.h>
#include <utils/logger.h>

static Fake::Si5351Chip& chip = Fake::si5351();

static const uint32_t IP_A = 0xC0A80114; // 192.168.1.20
static const uint32_t IP_B = 0xC0A80115; // 192.168.1.21

// Takes at most `room` bytes (-1: all), or fails.
struct FakeSocket : ScpiTcpSocket {
    std::string sent;
    int32_t room = -1;
    bool fail = false;
    int closes = 0;
    int sends = 0;
    int32_t trySend(const char* data, size_t len) override {
        ++sends;
        if (fail) return -1;
        size_t take = len;
        if (room >= 0 && take > (size_t)room) take = (size_t)room;
        if (room >= 0) room -= (int32_t)take;
        sent.append(data, take);
        return (int32_t)take;
    }
    void closeConnection() override { ++closes; }
};

// Counts the commands that reach the controller.
struct CountingBackend : ScpiBackend {
    ScpiBackend& inner;
    int submits = 0;
    explicit CountingBackend(ScpiBackend& b) : inner(b) {}
    Outcome submit(const Command& c) override {
        ++submits;
        return inner.submit(c);
    }
    bool snapshot(Snapshot* out) override { return inner.snapshot(out); }
    void heap(uint32_t* f, uint32_t* m, uint32_t* l) override { inner.heap(f, m, l); }
};

struct Device {
    SettingsService settings;
    SI5351Manager si;
    BacklightDriver backlight;
    AppController app;
    CommandQueue queue;
    StateSnapshot snap;
    LoopScpiBackend loopBackend;
    CountingBackend backend;
    FakeSocket socket;
    ScpiTcpLink link;
    Device()
        : app(settings, si, backlight), loopBackend(queue, app, snap), backend(loopBackend),
          link(backend, socket, "0123456789AB") {
        I2CBus::begin();
        settings.begin();
        PersistentState loaded;
        settings.load(loaded);
        app.begin(loaded);
        backlight.begin();
        app.connectHardware();
        app.applyStoredChannelSettings();
    }
    // Allowed and one client connected at t = 0.
    void connect(uint32_t ip = IP_A) {
        link.setAllowed(true);
        TEST_ASSERT_EQUAL(ScpiTcpLink::Accept::Accepted, link.offer(ip, 0));
    }
    // Bytes in; what was sent for them.
    std::string rx(const char* s, uint32_t now = 0) {
        socket.sent.clear();
        link.receive(s, strlen(s), now);
        return socket.sent;
    }
};

static std::string logText() {
    std::string all;
    char buf[512];
    while (Logger::readBuffered(buf, sizeof(buf)) > 0) all += buf;
    return all;
}

static bool contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

void setUp() {
    chip.reset();
    Fake::nvs().reset();
    Fake::clockMs() = 0;
    Logger::setOutput(Logger::Output::ToBuffer);
    logText();
}
void tearDown() { Logger::setOutput(Logger::Output::ToSerial); }

static void test_allowed_only_with_wifi_and_not_on_open_ap() {
    TEST_ASSERT_FALSE(ScpiTcpLink::allowedFor(false, false, false));
    TEST_ASSERT_TRUE(ScpiTcpLink::allowedFor(true, false, false));
    TEST_ASSERT_TRUE(ScpiTcpLink::allowedFor(false, true, false));
    TEST_ASSERT_FALSE(ScpiTcpLink::allowedFor(false, true, true));
    TEST_ASSERT_FALSE(ScpiTcpLink::allowedFor(true, true, true));
}

static void test_query_and_answer() {
    Device d;
    TEST_ASSERT_FALSE(d.link.active());
    d.connect();
    TEST_ASSERT_TRUE(d.link.active());
    TEST_ASSERT_EQUAL_HEX32(IP_A, d.link.clientIp());
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] TCP client 192.168.1.20 connected\n"));
    TEST_ASSERT_EQUAL_STRING("CYD,HF-Generator,0123456789AB," APP_VERSION "\n", d.rx("*IDN?\n").c_str());
    // A query later in the same line sees the change; the source is SCPI.
    TEST_ASSERT_EQUAL_STRING("7100000\n", d.rx("FREQ 7.1MHZ;:FREQ?\r\n").c_str());
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL(Source::Scpi, d.app.lastRemote());
    // Command contents never reach the log.
    TEST_ASSERT_FALSE(contains(logText(), "FREQ"));
    // The answers of a line leave in one piece.
    d.socket.sends = 0;
    TEST_ASSERT_EQUAL_STRING("7100000;1;0\n", d.rx("FREQ?;*OPC?;:OUTP1?\n").c_str());
    TEST_ASSERT_EQUAL(1, d.socket.sends);
}

// Answers of one line longer than the buffer: sent in the middle of the line, nothing lost.
// SYST:LOG? gives the longest answers; over TCP it reads nothing (SEC-6), so the test hands the
// log buffer to this session.
static void test_long_compound_answer() {
    Device d;
    d.connect();
    logText();
    TEST_ASSERT_FALSE(d.link.session().readsLog());
    d.link.session().setReadsLog(true);
    const std::string msg(80, 'x');
    for (uint8_t i = 0; i < LineFramer::MAX_COMMANDS; ++i) Logger::info("TEST", msg.c_str());
    std::string line;
    for (uint8_t i = 0; i < LineFramer::MAX_COMMANDS; ++i) line += i ? ";:SYST:LOG?" : "SYST:LOG?";
    line += "\n";
    d.socket.sends = 0;
    const std::string answer = d.rx(line.c_str());
    TEST_ASSERT_TRUE(answer.size() > ScpiTcpLink::OUT_BUFFER);
    TEST_ASSERT_TRUE(d.link.active());
    TEST_ASSERT_EQUAL(2, d.socket.sends);
    std::string one = "\"[TEST] " + msg + "\"";
    std::string expected = one;
    for (uint8_t i = 1; i < LineFramer::MAX_COMMANDS; ++i) expected += ";" + one;
    expected += "\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), answer.c_str());
}

static void test_partial_reads_across_line_boundaries() {
    Device d;
    d.connect();
    TEST_ASSERT_EQUAL_STRING("", d.rx("FREQ 7").c_str());
    TEST_ASSERT_EQUAL_STRING("", d.rx(".1MH").c_str());
    TEST_ASSERT_EQUAL_STRING("", d.rx("Z\nFRE").c_str());
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL_STRING("7100000\n", d.rx("Q?\n*OPC?").c_str());
    TEST_ASSERT_EQUAL_STRING("1\n", d.rx("\n").c_str());
    // Byte by byte.
    const char* line = "SOUR2:FREQ 455KHZ;:SOUR2:FREQ?\n";
    std::string answer;
    for (const char* p = line; *p; ++p) {
        const char one[2] = {*p, 0};
        answer += d.rx(one);
    }
    TEST_ASSERT_EQUAL_STRING("455000\n", answer.c_str());
}

static void test_second_client_refused() {
    Device d;
    d.connect(IP_A);
    logText();
    TEST_ASSERT_EQUAL(ScpiTcpLink::Accept::Busy, d.link.offer(IP_B, 10));
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] TCP client 192.168.1.21 refused (busy)\n"));
    // The first connection is untouched (the glue closes the refused socket, not this one).
    TEST_ASSERT_EQUAL(0, d.socket.closes);
    TEST_ASSERT_TRUE(d.link.active());
    TEST_ASSERT_EQUAL_HEX32(IP_A, d.link.clientIp());
    TEST_ASSERT_EQUAL_STRING("1\n", d.rx("*OPC?\n").c_str());
    // When the first client leaves, the next one gets in.
    d.link.peerClosed();
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(1, d.socket.closes);
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] TCP client 192.168.1.20 closed\n"));
    TEST_ASSERT_EQUAL(ScpiTcpLink::Accept::Accepted, d.link.offer(IP_B, 20));
    TEST_ASSERT_EQUAL_HEX32(IP_B, d.link.clientIp());
}

// W-S18: a browser (or a cross-site request) sends HTTP; nothing of it is executed.
static void test_http_request_line_closes_without_execution() {
    Device d;
    d.connect();
    logText();
    const std::string sent = d.rx("POST /x HTTP/1.1\r\nHost: 192.168.1.5\r\n\r\n*RST\nOUTP1 ON\n");
    TEST_ASSERT_EQUAL_STRING("", sent.c_str());
    TEST_ASSERT_EQUAL(0, d.backend.submits);
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(1, d.socket.closes);
    TEST_ASSERT_EQUAL(ScpiTcpLink::Close::Http, d.link.lastClose());
    const std::string log = logText();
    TEST_ASSERT_TRUE(contains(log, "[SCPI] TCP client 192.168.1.20 closed (HTTP request)\n"));
    TEST_ASSERT_FALSE(contains(log, "POST"));
    // GET in pieces, also after a too long request line.
    d.connect();
    const std::string longTarget = "GET /" + std::string(LineFramer::MAX_LINE, 'a') + " HTTP/1.1\n";
    d.rx(longTarget.c_str());
    TEST_ASSERT_TRUE(d.link.active()); // discarded as too long (-363), not executed
    d.rx("Host: ");
    d.rx("x\nOUTP1 ON\n");
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(0, d.backend.submits);
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
}

static void test_header_line_closes_without_execution() {
    Device d;
    d.connect();
    d.rx("FREQ 7MHZ\n");
    TEST_ASSERT_EQUAL(1, d.backend.submits);
    TEST_ASSERT_EQUAL_STRING("", d.rx("Origin: http://evil.example\nOUTP1 ON\n").c_str());
    TEST_ASSERT_EQUAL(1, d.backend.submits);
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(ScpiTcpLink::Close::Http, d.link.lastClose());
    // Bytes after the close are ignored.
    d.link.receive("OUTP1 ON\n", 9, 0);
    TEST_ASSERT_EQUAL(1, d.backend.submits);
}

static void test_idle_timeout() {
    Device d;
    d.connect();
    d.link.update(SCPI_IDLE_TIMEOUT_MS - 1);
    TEST_ASSERT_TRUE(d.link.active());
    // A line restarts the time.
    d.rx("*OPC?\n", SCPI_IDLE_TIMEOUT_MS - 1);
    d.link.update(2 * SCPI_IDLE_TIMEOUT_MS - 2);
    TEST_ASSERT_TRUE(d.link.active());
    logText();
    d.link.update(2 * SCPI_IDLE_TIMEOUT_MS - 1);
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(1, d.socket.closes);
    TEST_ASSERT_EQUAL(ScpiTcpLink::Close::Idle, d.link.lastClose());
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] TCP client 192.168.1.20 closed (idle)\n"));
    TEST_ASSERT_EQUAL_HEX32(0, d.link.clientIp());
}

static void test_framing_errors_in_error_queue() {
    Device d;
    d.connect();
    const std::string longLine = std::string(LineFramer::MAX_LINE + 1, 'A') + "\n";
    d.rx(longLine.c_str());
    d.rx("FREQ\x01 7MHZ\n");
    std::string many;
    for (uint8_t i = 0; i < LineFramer::MAX_COMMANDS + 1; ++i) many += "*OPC;";
    many += "\n";
    d.rx(many.c_str());
    TEST_ASSERT_EQUAL(0, d.backend.submits);
    TEST_ASSERT_EQUAL_STRING("-363,\"Input buffer overrun\"\n", d.rx("SYST:ERR?\n").c_str());
    TEST_ASSERT_EQUAL_STRING("-101,\"Invalid character\"\n", d.rx("SYST:ERR?\n").c_str());
    TEST_ASSERT_EQUAL_STRING("-223,\"Too much data\"\n", d.rx("SYST:ERR?\n").c_str());
    TEST_ASSERT_EQUAL_STRING("0,\"No error\"\n", d.rx("SYST:ERR?\n").c_str());
    // A new connection starts with an empty error queue and cleared enable registers.
    d.rx("FOO\n*ESE 255\n");
    d.link.peerClosed();
    TEST_ASSERT_EQUAL(ScpiTcpLink::Accept::Accepted, d.link.offer(IP_B, 0));
    TEST_ASSERT_EQUAL_STRING("0,\"No error\"\n", d.rx("SYST:ERR?\n").c_str());
    TEST_ASSERT_EQUAL_STRING("0\n", d.rx("*ESE?\n").c_str());
}

static void test_not_allowed_closes_and_refuses() {
    Device d;
    TEST_ASSERT_FALSE(d.link.allowed());
    TEST_ASSERT_EQUAL(ScpiTcpLink::Accept::NotAllowed, d.link.offer(IP_A, 0));
    TEST_ASSERT_FALSE(d.link.active());
    d.connect();
    logText();
    d.link.setAllowed(false); // WiFi lost or the access point became open
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(1, d.socket.closes);
    TEST_ASSERT_EQUAL(ScpiTcpLink::Close::NotAllowed, d.link.lastClose());
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] TCP client 192.168.1.20 closed (WiFi)\n"));
    TEST_ASSERT_EQUAL(ScpiTcpLink::Accept::NotAllowed, d.link.offer(IP_B, 0));
    d.link.receive("OUTP1 ON\n", 9, 0);
    TEST_ASSERT_EQUAL(0, d.backend.submits);
    // Closing twice does nothing more.
    d.link.setAllowed(false);
    d.link.disconnect();
    TEST_ASSERT_EQUAL(1, d.socket.closes);
}

static void test_disconnect_at_the_device() {
    Device d;
    d.connect();
    logText();
    d.link.disconnect();
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(1, d.socket.closes);
    TEST_ASSERT_EQUAL(ScpiTcpLink::Close::Local, d.link.lastClose());
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] TCP client 192.168.1.20 closed (disconnect)\n"));
    TEST_ASSERT_TRUE(d.link.allowed());
    TEST_ASSERT_EQUAL(ScpiTcpLink::Accept::Accepted, d.link.offer(IP_B, 0));
}

static void test_partial_writes() {
    Device d;
    d.connect();
    const std::string idn = "CYD,HF-Generator,0123456789AB," APP_VERSION "\n";
    // The stack takes 5 bytes; the rest waits and no input is read meanwhile.
    d.socket.room = 5;
    TEST_ASSERT_EQUAL_STRING(idn.substr(0, 5).c_str(), d.rx("*IDN?\n").c_str());
    TEST_ASSERT_TRUE(d.link.active());
    TEST_ASSERT_FALSE(d.link.wantsInput());
    d.socket.room = 0;
    d.link.update(1);
    TEST_ASSERT_FALSE(d.link.wantsInput());
    d.socket.room = 10;
    d.link.update(2);
    d.socket.room = -1;
    d.link.update(3);
    TEST_ASSERT_EQUAL_STRING(idn.c_str(), d.socket.sent.c_str());
    TEST_ASSERT_TRUE(d.link.wantsInput());
    // A client that never reads: the waiting answers overflow and the connection closes.
    d.socket.room = 0;
    std::string lines;
    for (size_t n = 0; n < ScpiTcpLink::OUT_BUFFER / idn.size() + 2; ++n) lines += "*IDN?\n";
    d.link.receive(lines.c_str(), lines.size(), 4);
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(ScpiTcpLink::Close::Overflow, d.link.lastClose());
    // A failed send closes once the line is done.
    d.socket.room = -1;
    d.connect();
    d.socket.fail = true;
    d.rx("*IDN?;:FREQ 7MHZ\n*OPC?\n");
    TEST_ASSERT_FALSE(d.link.active());
    TEST_ASSERT_EQUAL(ScpiTcpLink::Close::SendFailed, d.link.lastClose());
    TEST_ASSERT_EQUAL_UINT64(7000000, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL(2, d.socket.closes);
}

// TCP and USB have separate sessions: errors of one do not show up in the other.
static void test_tcp_and_usb_sessions_are_separate() {
    Device d;
    ScpiSession usb(d.loopBackend, Source::Usb, "0123456789AB");
    d.connect();
    d.rx("FOO\n");
    TEST_ASSERT_EQUAL(0, usb.errorCount());
    TEST_ASSERT_EQUAL(1, d.link.session().errorCount());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_allowed_only_with_wifi_and_not_on_open_ap);
    RUN_TEST(test_query_and_answer);
    RUN_TEST(test_long_compound_answer);
    RUN_TEST(test_partial_reads_across_line_boundaries);
    RUN_TEST(test_second_client_refused);
    RUN_TEST(test_http_request_line_closes_without_execution);
    RUN_TEST(test_header_line_closes_without_execution);
    RUN_TEST(test_idle_timeout);
    RUN_TEST(test_framing_errors_in_error_queue);
    RUN_TEST(test_not_allowed_closes_and_refuses);
    RUN_TEST(test_disconnect_at_the_device);
    RUN_TEST(test_partial_writes);
    RUN_TEST(test_tcp_and_usb_sessions_are_separate);
    return UNITY_END();
}
