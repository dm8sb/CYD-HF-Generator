// SCPI command tree (ARBEITSPLAN 3.5) through a session with the loop backend, against the
// real controller and the SI5351 register model: short/long forms, units, MIN/MAX/DEF,
// rejected numbers (W-S19), error queue, IEEE 488.2, W-E25, *RST, USB session and log.

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
#include <remote/scpi_usb.h>
#include <utils/logger.h>

static Fake::Si5351Chip& chip = Fake::si5351();
static std::string out;
static void capture(const char* data, size_t len, void*) { out.append(data, len); }
static bool debugOn = true;
static void debugOutput(bool on) { debugOn = on; }

struct Device {
    SettingsService settings;
    SI5351Manager si;
    BacklightDriver backlight;
    AppController app;
    CommandQueue queue;
    StateSnapshot snap;
    LoopScpiBackend backend;
    ScpiSession scpi;
    Device() : app(settings, si, backlight), backend(queue, app, snap), scpi(backend, Source::Usb, "0123456789AB") {
        I2CBus::begin();
        settings.begin();
        PersistentState loaded;
        settings.load(loaded);
        app.begin(loaded);
        backlight.begin();
        app.connectHardware();
        app.applyStoredChannelSettings();
        scpi.setWriter(capture, nullptr);
    }
    // One line; the answer without its LF ("" for none).
    std::string q(const char* line) {
        out.clear();
        scpi.execute(line, strlen(line));
        std::string r = out;
        if (!r.empty() && r[r.size() - 1] == '\n') r.erase(r.size() - 1);
        return r;
    }
    std::string err() { return q("SYST:ERR?"); }
};

static void drainLog() {
    char buf[256];
    while (Logger::readBuffered(buf, sizeof(buf)) > 0) {}
}

void setUp() {
    chip.reset();
    Fake::nvs().reset();
    Fake::clockMs() = 0;
    Logger::setOutput(Logger::Output::ToSerial);
    drainLog();
    debugOn = true;
}
void tearDown() { Logger::setOutput(Logger::Output::ToSerial); }

#define ASSERT_ANSWER(expected, actual) TEST_ASSERT_EQUAL_STRING(expected, (actual).c_str())

static void test_idn_version_and_empty_error_queue() {
    Device d;
    ASSERT_ANSWER("CYD,HF-Generator,0123456789AB," APP_VERSION, d.q("*IDN?"));
    ASSERT_ANSWER("1999.0", d.q("SYST:VERS?"));
    ASSERT_ANSWER("0,\"No error\"", d.err());
    ASSERT_ANSWER("0", d.q("*TST?"));
    ASSERT_ANSWER("1", d.q("*OPC?"));
}

static void test_short_and_long_forms_and_suffixes() {
    Device d;
    ASSERT_ANSWER("", d.q("FREQ 7.1 MHZ"));
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    ASSERT_ANSWER("", d.q("source2:frequency:cw 455khz"));
    TEST_ASSERT_EQUAL_UINT64(455000, d.app.channel(2).frequency);
    ASSERT_ANSWER("", d.q("SOUR1:FREQ:CW 10000000"));
    ASSERT_ANSWER("10000000", d.q("FREQ?"));
    ASSERT_ANSWER("455000", d.q("SOURce2:FREQuency?"));
    // CH3 answers with its master frequency and takes none itself.
    ASSERT_ANSWER("10000000", d.q("SOUR3:FREQ?"));
    ASSERT_ANSWER("", d.q("SOUR3:FREQ 7MHZ"));
    ASSERT_ANSWER("-221,\"Settings conflict;CH3 follows its master\"", d.err());
    ASSERT_ANSWER("0,\"No error\"", d.err());
}

static void test_numbers_are_exact_integers() {
    Device d;
    d.q("FREQ 7.1E6");
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    d.q("FREQ 10.0000005MHZ"); // 10 000 000.5 Hz: half up
    TEST_ASSERT_EQUAL_UINT64(10000001, d.app.channel(1).frequency);
    d.q("FREQ 7100000.4");
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    d.q("FREQ 0.0455 MHz");
    TEST_ASSERT_EQUAL_UINT64(45500, d.app.channel(1).frequency);
    d.q("FREQ MIN");
    TEST_ASSERT_EQUAL_UINT64(FREQ_MIN_HZ, d.app.channel(1).frequency);
    d.q("FREQ maximum");
    TEST_ASSERT_EQUAL_UINT64(FREQ_MAX_HZ, d.app.channel(1).frequency);
    d.q("FREQ DEF");
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1, d.app.channel(1).frequency);
    ASSERT_ANSWER("0,\"No error\"", d.err());
}

static void test_rejected_numbers() {
    Device d;
    struct Case {
        const char* line;
        const char* error;
    };
    const Case cases[] = {
        {"FREQ 200MHZ", "-222,\"Data out of range\""},
        {"FREQ 7999", "-222,\"Data out of range\""},
        {"FREQ -7MHZ", "-222,\"Data out of range\""},
        {"FREQ 1E30", "-222,\"Data out of range\""},
        {"FREQ NAN", "-224,\"Illegal parameter value\""},
        {"FREQ INF", "-224,\"Illegal parameter value\""},
        {"FREQ #H6C5C40", "-224,\"Illegal parameter value\""},
        {"FREQ 7 XHZ", "-224,\"Illegal parameter value\""},
        {"FREQ 1234567890123456789", "-224,\"Illegal parameter value\""},
        {"FREQ \"7MHZ\"", "-104,\"Data type error\""},
        {"FREQ", "-109,\"Missing parameter\""},
        {"FREQ 7MHZ,1", "-108,\"Parameter not allowed\""},
        {"OUTP4 ON", "-114,\"Header suffix out of range\""},
        {"SOUR2:PHAS 90", "-114,\"Header suffix out of range\""},
        {"OUTP1 MAYBE", "-224,\"Illegal parameter value\""},
        {"SYST:STEP 5", "-224,\"Illegal parameter value\""},
        {"OUTP:DRIV 4.5", "-224,\"Illegal parameter value\""},
        {"SYST:CORR 51", "-222,\"Data out of range\""},
        {"SYST:BACK 5", "-222,\"Data out of range\""},
        {"SOUR3:PHAS 45", "-224,\"Illegal parameter value\""},
        {"SYST:TUN FAST", "-224,\"Illegal parameter value\""},
    };
    for (const Case& c : cases) {
        ASSERT_ANSWER("", d.q(c.line));
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c.error, d.err().c_str(), c.line);
    }
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1, d.app.channel(1).frequency); // nothing applied
    // An unknown header names itself.
    d.q("FOO:BAR 1");
    ASSERT_ANSWER("-113,\"Undefined header;FOO:BAR 1\"", d.err());
}

static void test_invalid_character_gives_one_error() {
    Device d;
    const char* const lines[] = {"FREQ 7MHZ 8MHZ", "FREQ 7MHZ FOO", "FREQ 0x10", "FR@Q 7MHZ",
                                 "FREQ 7 \"a;b\" 8", "FREQ 7MHZ\x01"};
    for (const char* l : lines) {
        ASSERT_ANSWER("", d.q(l));
        TEST_ASSERT_EQUAL_STRING_MESSAGE("-101,\"Invalid character\"", d.err().c_str(), l);
        TEST_ASSERT_EQUAL_STRING_MESSAGE("0,\"No error\"", d.err().c_str(), l);
    }
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1, d.app.channel(1).frequency);
    // The unit after the broken one runs.
    ASSERT_ANSWER("", d.q("FREQ 7 8;:SOUR2:FREQ 3.5MHZ"));
    TEST_ASSERT_EQUAL_UINT64(3500000, d.app.channel(2).frequency);
    ASSERT_ANSWER("-101,\"Invalid character\"", d.err());
    ASSERT_ANSWER("0,\"No error\"", d.err());
    ASSERT_ANSWER("", d.q("FREQ 7 \"x;y\";:SOUR2:FREQ 1MHZ"));
    TEST_ASSERT_EQUAL_UINT64(1000000, d.app.channel(2).frequency);
    ASSERT_ANSWER("-101,\"Invalid character\"", d.err());
    ASSERT_ANSWER("0,\"No error\"", d.err());
}

static void test_number_parser() {
    using namespace ScpiNumber;
    const Unit pct[] = {{"PCT", 0}, {nullptr, 0}};
    int64_t v = 0;
    TEST_ASSERT_EQUAL(Parse::Ok, decimal("-2.5", 4, 2, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(-250, v);
    TEST_ASSERT_EQUAL(Parse::Ok, decimal("-0.005", 6, 2, nullptr, &v)); // half away from zero
    TEST_ASSERT_EQUAL_INT64(-1, v);
    TEST_ASSERT_EQUAL(Parse::Ok, decimal("1.234", 5, 2, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(123, v);
    TEST_ASSERT_EQUAL(Parse::Ok, decimal("15e-1", 5, 0, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(2, v);
    TEST_ASSERT_EQUAL(Parse::Ok, decimal(".5", 2, 0, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(1, v);
    TEST_ASSERT_EQUAL(Parse::Ok, decimal("80 pct", 6, 0, pct, &v));
    TEST_ASSERT_EQUAL_INT64(80, v);
    TEST_ASSERT_EQUAL(Parse::Ok, decimal("7.10000000000000000000", 22, 0, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(7, v);
    TEST_ASSERT_EQUAL(Parse::Ok, decimal("1E-40", 5, 0, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(0, v);
    TEST_ASSERT_EQUAL(Parse::Syntax, decimal("9223372036854775808", 19, 0, nullptr, &v)); // > 18 digits
    TEST_ASSERT_EQUAL(Parse::Range, decimal("1E19", 4, 0, nullptr, &v));
    TEST_ASSERT_EQUAL(Parse::Range, decimal("100000000000000000", 18, 2, nullptr, &v));
    TEST_ASSERT_EQUAL(Parse::Syntax, decimal(".", 1, 0, nullptr, &v));
    TEST_ASSERT_EQUAL(Parse::Syntax, decimal("E5", 2, 0, nullptr, &v));
    TEST_ASSERT_EQUAL(Parse::Syntax, decimal("5E", 2, 0, nullptr, &v));
    TEST_ASSERT_EQUAL(Parse::Syntax, decimal("80 PCT", 6, 0, nullptr, &v)); // no unit allowed
    TEST_ASSERT_EQUAL(Parse::Syntax, decimal("1.2.3", 5, 0, nullptr, &v));
    // Many leading zeros of the fraction against a long exponent (fuzzing, S57b).
    std::string tiny = "0." + std::string(1216, '0') + "1E12345";
    TEST_ASSERT_EQUAL(Parse::Range, decimal(tiny.c_str(), tiny.size(), 0, nullptr, &v));
    tiny = "0." + std::string(1216, '0') + "1E1217";
    TEST_ASSERT_EQUAL(Parse::Ok, decimal(tiny.c_str(), tiny.size(), 0, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(1, v);
    tiny = "5E-" + std::string(30, '9');
    TEST_ASSERT_EQUAL(Parse::Ok, decimal(tiny.c_str(), tiny.size(), 0, nullptr, &v));
    TEST_ASSERT_EQUAL_INT64(0, v);
    tiny = "5E" + std::string(30, '9');
    TEST_ASSERT_EQUAL(Parse::Range, decimal(tiny.c_str(), tiny.size(), 0, nullptr, &v));
    uint64_t hz = 0;
    TEST_ASSERT_EQUAL(Parse::Ok, frequency("160MHZ", 6, &hz));
    TEST_ASSERT_EQUAL_UINT64(FREQ_MAX_HZ, hz);
    TEST_ASSERT_EQUAL(Parse::Range, frequency("160.000001MHZ", 13, &hz));
}

static void test_outputs_and_condition() {
    Device d;
    ASSERT_ANSWER("0", d.q("OUTP1?"));
    ASSERT_ANSWER("OFF", d.q("OUTP1:COND?"));
    d.q("OUTP1 ON");
    TEST_ASSERT_TRUE(d.app.channel(1).enabled);
    ASSERT_ANSWER("1", d.q("OUTPut1:STATe?"));
    ASSERT_ANSWER("ON", d.q("OUTP1:CONDition?"));
    d.q("FREQ 1MHZ;:OUTP3 1");
    ASSERT_ANSWER("WAIT", d.q("OUTP3:COND?")); // master below the IQ range
    d.q("FREQ 10MHZ");
    ASSERT_ANSWER("ON", d.q("OUTP3:COND?"));
    ASSERT_ANSWER("", d.q("SOUR3:PHAS 270;IQ:MAST 1"));
    ASSERT_ANSWER("270;1", d.q("SOUR3:PHAS?;IQ:MAST?"));
    ASSERT_ANSWER("0,\"No error\"", d.err());
    d.q("OUTP1 OFF");
    ASSERT_ANSWER("WAIT", d.q("OUTP3:COND?"));
    chip.present = false;
    for (int i = 0; i < 5; ++i) {
        Fake::advanceMs(HW_CHECK_INTERVAL_MS);
        d.app.update();
    }
    ASSERT_ANSWER("NOHW", d.q("OUTP2:COND?"));
    ASSERT_ANSWER("NFO", d.q("SYST:SI5351?"));
    ASSERT_ANSWER("1", d.q("*TST?"));
}

static void test_compound_lines() {
    Device d;
    ASSERT_ANSWER("7000000", d.q("FREQ 7MHZ;FREQ?"));
    ASSERT_ANSWER("7000000;0;CONN", d.q("FREQ?;:OUTP1?;:SYST:SI5351?"));
    std::string act = d.q("FREQ:ACT?");
    TEST_ASSERT_EQUAL_STRING("7000000.0", act.c_str());
    std::string range = d.q("SOUR3:IQ:RANG?");
    TEST_ASSERT_TRUE(range.find(',') != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("4730000", range.substr(0, range.find(',')).c_str());
}

static void test_settings_commands() {
    Device d;
    d.q("SYST:TUN LNO");
    ASSERT_ANSWER("LNO", d.q("SYST:TUN?"));
    d.q("SYSTEM:TUNING INTEGER");
    ASSERT_ANSWER("INT", d.q("SYST:TUN?"));
    d.q("SSC DOWN05");
    ASSERT_ANSWER("-221,\"Settings conflict;Not in Integer tuning\"", d.err());
    d.q("SYST:TUN FRAC;:SOUR1:SSC CENT05");
    ASSERT_ANSWER("CENT05", d.q("SSC?"));
    d.q("OUTP:OFFS HIZ;DRIV 8");
    ASSERT_ANSWER("HIZ;8", d.q("OUTP:OFFS?;DRIV?"));
    d.q("SYST:CORR -2.5");
    ASSERT_ANSWER("-2.50", d.q("SYST:CORR?"));
    d.q("SYST:CORR 1.234 PPM");
    ASSERT_ANSWER("1.23", d.q("SYST:CORR?"));
    d.q("SYST:STEP 1KHZ;BACK 50");
    ASSERT_ANSWER("1000;50", d.q("SYST:STEP?;BACK?"));
    ASSERT_ANSWER("0,\"No error\"", d.err());
    ASSERT_ANSWER("0,0,0", d.q("SYST:HEAP?"));
    d.q("SYST:IDEN");
    TEST_ASSERT_TRUE(d.app.identifying());
}

// W-E25 / W-S21: local only refuses switching on (-221), off stays allowed.
static void test_remote_outputs_local_only() {
    Device d;
    ASSERT_ANSWER("ALLOWED", d.q("OUTP:REM?"));
    d.app.setRemoteOutputsAllowed(false);
    ASSERT_ANSWER("LOCAL", d.q("OUTP:REM?"));
    d.q("OUTP1 ON");
    ASSERT_ANSWER("-221,\"Settings conflict;Remote outputs: local only\"", d.err());
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    d.app.setChannelEnabled(1, true);
    d.q("OUTP1 OFF");
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    ASSERT_ANSWER("0,\"No error\"", d.err());
}

// W-E19: *RST switches off and loads the defaults without saving; SYST:SAVE writes at once.
static void test_rst_and_save() {
    Device d;
    d.q("FREQ 7MHZ;:OUTP1 ON;:SYST:CORR 1");
    const unsigned writes = Fake::nvs().writeCount;
    d.q("*RST");
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL_INT32(100, d.app.system().freq_correction); // kept
    Fake::advanceMs(SETTINGS_REMOTE_AUTOSAVE_MS + 100);
    d.app.update();
    TEST_ASSERT_EQUAL_UINT(writes, Fake::nvs().writeCount);
    d.q("SYST:SAVE");
    TEST_ASSERT_TRUE(Fake::nvs().writeCount > writes);
    ASSERT_ANSWER("0,\"No error\"", d.err());
}

static void test_status_registers() {
    Device d;
    ASSERT_ANSWER("0", d.q("*ESR?"));
    d.q("FREQ 1GHZ");
    ASSERT_ANSWER("16", d.q("*ESR?")); // execution error bit
    d.q("FOO");
    ASSERT_ANSWER("32", d.q("*ESR?")); // command error bit
    d.q("*CLS");
    ASSERT_ANSWER("0,\"No error\"", d.err());
    d.q("*ESE 255;*SRE 32");
    ASSERT_ANSWER("255;32", d.q("*ESE?;*SRE?"));
}

static void test_rate_limit_gives_execution_error() {
    Device d;
    for (uint8_t i = 0; i < REMOTE_WRITES_PER_S; ++i) d.q("SYST:STEP 1000");
    d.q("SYST:STEP 100");
    ASSERT_ANSWER("-200,\"Execution error;Too many commands\"", d.err());
    TEST_ASSERT_EQUAL_UINT32(1000, d.app.system().delta_step);
    Fake::advanceMs(1000);
    d.q("SYST:STEP 100");
    TEST_ASSERT_EQUAL_UINT32(100, d.app.system().delta_step);
}

// W-E27: the first line starts a USB session; the log goes into the ring buffer until it ends.
static void test_usb_session_and_log() {
    Device d;
    ScpiUsbLink link(d.scpi, debugOutput);
    TEST_ASSERT_FALSE(link.active());
    out.clear();
    const char* in = "*IDN?\nSYST:LOG?\n";
    link.receive(in, strlen(in));
    TEST_ASSERT_TRUE(link.active());
    TEST_ASSERT_FALSE(debugOn);
    TEST_ASSERT_EQUAL(Logger::Output::ToBuffer, Logger::output());
    std::string expected = "CYD,HF-Generator,0123456789AB," APP_VERSION "\n";
    expected += "\"[SCPI] USB session started; log in SYSTem:LOG?\"\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), out.c_str());
    // Empty: "".
    out.clear();
    link.receive("SYST:LOG?\n", 10);
    TEST_ASSERT_EQUAL_STRING("\"\"\n", out.c_str());
    // Framing errors reach the error queue.
    const std::string longLine(LineFramer::MAX_LINE + 1, 'A');
    link.receive(longLine.c_str(), longLine.size());
    link.receive("\n", 1);
    out.clear();
    link.receive("SYST:ERR?\n", 10);
    TEST_ASSERT_EQUAL_STRING("-363,\"Input buffer overrun\"\n", out.c_str());
    // Idle: back to the serial port.
    Fake::advanceMs(SCPI_IDLE_TIMEOUT_MS - 1);
    link.update();
    TEST_ASSERT_TRUE(link.active());
    Fake::advanceMs(1);
    link.update();
    TEST_ASSERT_FALSE(link.active());
    TEST_ASSERT_TRUE(debugOn);
    TEST_ASSERT_EQUAL(Logger::Output::ToSerial, Logger::output());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idn_version_and_empty_error_queue);
    RUN_TEST(test_short_and_long_forms_and_suffixes);
    RUN_TEST(test_numbers_are_exact_integers);
    RUN_TEST(test_rejected_numbers);
    RUN_TEST(test_invalid_character_gives_one_error);
    RUN_TEST(test_number_parser);
    RUN_TEST(test_outputs_and_condition);
    RUN_TEST(test_compound_lines);
    RUN_TEST(test_settings_commands);
    RUN_TEST(test_remote_outputs_local_only);
    RUN_TEST(test_rst_and_save);
    RUN_TEST(test_status_registers);
    RUN_TEST(test_rate_limit_gives_execution_error);
    RUN_TEST(test_usb_session_and_log);
    return UNITY_END();
}
