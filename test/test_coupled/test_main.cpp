// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// CH2 coupled to CH1 (N14, E-213) on the fakes: offset arithmetic and limits, waiting and
// automatic restart, CH1 changes moving CH2, CH3 IQ with a coupled master, persistence and the
// remote commands. Frequencies are checked on the SI5351 register model (helpers/si5351_model.h).

#include <unity.h>
#include <Preferences.h>
#include <Wire.h>
#include <constants.h>
#include <core/app_controller.h>
#include <hal/i2c_bus.h>
#include <helpers/si5351_model.h>
#include <remote/command.h>
#include <utils/frequency_utils.h>
#include <utils/input_logic.h>
#include <utils/status_text.h>
#include <string.h>

using namespace Si5351Model;

static Fake::Si5351Chip& chip = Fake::si5351();
static Fake::Nvs& nvs = Fake::nvs();

struct Device {
    SettingsService settings;
    SI5351Manager si;
    BacklightDriver backlight;
    AppController app;

    Device() : app(settings, si, backlight) {
        I2CBus::begin();
        settings.begin();
        PersistentState loaded;
        settings.load(loaded);
        app.begin(loaded);
        backlight.begin();
        app.connectHardware();
        backlight.setPercent(loaded.sys.backlight);
        app.applyStoredChannelSettings();
    }

    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 50) {
            Fake::advanceMs(50);
            app.update();
        }
    }
};

void setUp() {
    chip.reset();
    nvs.reset();
    Fake::clockMs() = 0;
    Fake::wireMisuse() = false;
    Fake::gpio() = Fake::Gpio {};
    Wire.failBegin = false;
    Wire.failEnd = false;
    Wire.resetCounters();
}
void tearDown() { TEST_ASSERT_FALSE(Fake::wireMisuse()); }

static void assertHz(uint8_t clk, uint64_t expected) {
    TEST_ASSERT_NULL(limitViolation(clk, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1.35, (double)expected, outputHz(clk, 0));
}

// ---------------------------------------------------------------------------
// Controller

static void test_defaults_free_and_zero() {
    Device d;
    TEST_ASSERT_FALSE(d.app.ch2Coupled());
    TEST_ASSERT_EQUAL_INT32(0, d.app.ch2Coupling().offset);
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH2, d.app.frequency(2));
    TEST_ASSERT_FALSE(d.app.ch2Waiting());
}

static void test_coupled_runs_at_ch1_plus_offset() {
    Device d;
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setCh2Offset(455000));
    assertHz(1, DEFAULT_FREQ_CH2); // free: the offset is only stored
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setCh2Coupled(true));
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setChannelEnabled(2, true));
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1 + 455000, d.app.frequency(2));
    TEST_ASSERT_EQUAL(ChannelStatus::On, d.app.channelStatus(2));
    TEST_ASSERT_TRUE(enabled(1));
    assertHz(1, DEFAULT_FREQ_CH1 + 455000);
    TEST_ASSERT_TRUE(usesPllB(1)); // CH2 keeps PLL B
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setCh2Offset(-455000));
    assertHz(1, DEFAULT_FREQ_CH1 - 455000);
    TEST_ASSERT_EQUAL(Result::NoChange, d.app.setCh2Offset(-455000));
    TEST_ASSERT_EQUAL(Result::NoChange, d.app.setCh2Coupled(true));
}

static void test_offset_limits_set_rejects_adjust_clamps() {
    Device d;
    TEST_ASSERT_EQUAL(Result::OutOfRange, d.app.setCh2Offset((int64_t)CH2_OFFSET_MAX_HZ + 1));
    TEST_ASSERT_EQUAL(Result::OutOfRange, d.app.setCh2Offset(-(int64_t)CH2_OFFSET_MAX_HZ - 1));
    TEST_ASSERT_EQUAL(Result::OutOfRange, d.app.setCh2Offset(INT64_MIN));
    TEST_ASSERT_EQUAL_INT32(0, d.app.ch2Coupling().offset);
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setCh2Offset(CH2_OFFSET_MAX_HZ));
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setCh2Offset(-CH2_OFFSET_MAX_HZ));
    d.app.adjustCh2Offset(-1000);
    TEST_ASSERT_EQUAL_INT32(-CH2_OFFSET_MAX_HZ, d.app.ch2Coupling().offset);
    d.app.adjustCh2Offset(INT64_MAX / 2);
    TEST_ASSERT_EQUAL_INT32(CH2_OFFSET_MAX_HZ, d.app.ch2Coupling().offset);
    d.app.adjustCh2Offset(-CH2_OFFSET_MAX_HZ - 5);
    TEST_ASSERT_EQUAL_INT32(-5, d.app.ch2Coupling().offset);
}

static void test_ch2_frequency_refused_while_coupled() {
    Device d;
    d.app.setCh2Coupled(true);
    TEST_ASSERT_EQUAL(Result::Coupled, d.app.setChannelFrequency(2, 7000000));
    TEST_ASSERT_EQUAL(Result::Coupled, d.app.adjustChannelFrequency(2, 1000));
    TEST_ASSERT_EQUAL(Result::OutOfRange, d.app.setChannelFrequency(2, 1)); // data first
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1, d.app.frequency(2));
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setChannelFrequency(1, 7000000)); // CH1 stays free
}

static void test_ch1_change_retunes_ch2_without_dropout() {
    Device d;
    d.app.setCh2Offset(455000);
    d.app.setCh2Coupled(true);
    d.app.setChannelEnabled(1, true);
    d.app.setChannelEnabled(2, true);
    chip.clearLog();
    d.app.setChannelFrequency(1, 7074000);
    for (int i = 0; i < 50; ++i) d.app.adjustChannelFrequency(1, 10);
    TEST_ASSERT_EQUAL_UINT(0, chip.writesTo(3));
    TEST_ASSERT_EQUAL_UINT(0, chip.totalPllResets());
    TEST_ASSERT_TRUE(enabled(0) && enabled(1));
    assertHz(0, 7074500);
    assertHz(1, 7074500 + 455000);
    TEST_ASSERT_EQUAL_UINT64(7074500 + 455000, d.app.channel(2).frequency);
}

static void test_wait_low_then_auto_start() {
    Device d;
    d.app.setChannelFrequency(1, 1000000);
    d.app.setCh2Offset(-995000);
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setCh2Coupled(true));
    TEST_ASSERT_TRUE(d.app.ch2Waiting());
    TEST_ASSERT_EQUAL(ChannelStatus::Off, d.app.channelStatus(2)); // off: OFF, not WAIT
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setChannelEnabled(2, true));
    TEST_ASSERT_EQUAL(ChannelStatus::FreqTooLow, d.app.channelStatus(2));
    TEST_ASSERT_FALSE(enabled(1));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, d.app.actualHz(2));
    TEST_ASSERT_EQUAL_UINT64(5000, d.app.frequency(2));
    TEST_ASSERT_EQUAL_INT64(5000, d.app.ch2TargetHz());
    // CH1 moves CH2 into the range: CH2 starts by itself.
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setChannelFrequency(1, 1003000));
    TEST_ASSERT_EQUAL(ChannelStatus::On, d.app.channelStatus(2));
    TEST_ASSERT_TRUE(enabled(1));
    assertHz(1, 8000);
    // Out again below 0: frequency(2) shows 0.
    d.app.setCh2Offset(-2000000);
    TEST_ASSERT_EQUAL(ChannelStatus::FreqTooLow, d.app.channelStatus(2));
    TEST_ASSERT_EQUAL_UINT64(0, d.app.frequency(2));
    TEST_ASSERT_FALSE(enabled(1));
}

static void test_wait_high_then_auto_start() {
    Device d;
    d.app.setChannelFrequency(1, 150000000);
    d.app.setCh2Offset(20000000);
    d.app.setCh2Coupled(true);
    d.app.setChannelEnabled(2, true);
    TEST_ASSERT_EQUAL(ChannelStatus::FreqTooHigh, d.app.channelStatus(2));
    TEST_ASSERT_EQUAL_UINT64(170000000, d.app.frequency(2));
    TEST_ASSERT_FALSE(enabled(1));
    d.app.adjustChannelFrequency(1, -10000000);
    TEST_ASSERT_EQUAL(ChannelStatus::On, d.app.channelStatus(2));
    assertHz(1, 160000000);
}

// With off state High-Z the driver of a waiting output stays powered: switching on must not
// enable it with the old MultiSynth setting.
static void test_switch_on_while_waiting_keeps_output_off() {
    Device d;
    d.app.setOffState(OffState::HighZ);
    d.app.setChannelFrequency(1, 1000000);
    d.app.setCh2Offset(-999000);
    d.app.setCh2Coupled(true);
    d.app.setChannelEnabled(2, true);
    TEST_ASSERT_TRUE(regs()[3] & (1U << 1)); // output disabled in register 3
    TEST_ASSERT_EQUAL(ChannelStatus::FreqTooLow, d.app.channelStatus(2));
}

static void test_decouple_keeps_last_frequency() {
    Device d;
    d.app.setChannelFrequency(1, 7000000);
    d.app.setCh2Offset(455000);
    d.app.setCh2Coupled(true);
    d.app.setChannelEnabled(2, true);
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setCh2Coupled(false));
    TEST_ASSERT_EQUAL_UINT64(7455000, d.app.frequency(2));
    assertHz(1, 7455000);
    d.app.setChannelFrequency(1, 8000000); // CH2 no longer follows
    assertHz(1, 7455000);
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setChannelFrequency(2, 9000000));
}

static void test_ch3_follows_coupled_master() {
    Device d;
    d.app.setCH3Master(2);
    d.app.setChannelFrequency(1, 7000000);
    d.app.setCh2Offset(100000);
    d.app.setCh2Coupled(true);
    d.app.setChannelEnabled(2, true);
    d.app.setChannelEnabled(3, true);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.ch3Frequency());
    assertHz(1, 7100000);
    assertHz(2, 7100000);
    d.app.setChannelFrequency(1, 10000000);
    TEST_ASSERT_EQUAL_UINT64(10100000, d.app.ch3Frequency());
    assertHz(2, 10100000);
    // CH2 in range but below the IQ range: CH2 runs, CH3 waits.
    d.app.setChannelFrequency(1, 3000000);
    TEST_ASSERT_EQUAL(ChannelStatus::On, d.app.channelStatus(2));
    TEST_ASSERT_EQUAL(Ch3Status::FreqTooLow, d.app.ch3Status());
    TEST_ASSERT_TRUE(poweredDown(2));
    // CH2 waits below 8 kHz: CH3 waits too.
    d.app.setCh2Offset(-2995000);
    TEST_ASSERT_EQUAL(ChannelStatus::FreqTooLow, d.app.channelStatus(2));
    TEST_ASSERT_EQUAL(Ch3Status::FreqTooLow, d.app.ch3Status());
    TEST_ASSERT_FALSE(enabled(1));
    TEST_ASSERT_FALSE(enabled(2));
    // CH2 above 160 MHz: CH3 above its IQ range.
    d.app.setChannelFrequency(1, 150000000);
    d.app.setCh2Offset(20000000);
    TEST_ASSERT_EQUAL(ChannelStatus::FreqTooHigh, d.app.channelStatus(2));
    TEST_ASSERT_EQUAL(Ch3Status::FreqTooHigh, d.app.ch3Status());
    // Back into the IQ range: both start by themselves.
    d.app.setChannelFrequency(1, 14000000);
    d.app.setCh2Offset(-100000);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    assertHz(1, 13900000);
    assertHz(2, 13900000);
}

static void test_persisted_and_restored() {
    {
        Device d;
        d.app.setCh2Offset(-455000);
        d.app.setCh2Coupled(true);
        d.app.setChannelEnabled(2, true);
        d.run(SETTINGS_AUTOSAVE_MS + 200);
        Preferences p;
        p.begin("hfgen");
        TEST_ASSERT_EQUAL_UINT8(1, p.getUChar("ch2_cpl", 0));
        TEST_ASSERT_EQUAL_INT32(-455000, p.getInt("ch2_off", 0));
    }
    Device d;
    TEST_ASSERT_TRUE(d.app.ch2Coupled());
    TEST_ASSERT_EQUAL_INT32(-455000, d.app.ch2Coupling().offset);
    TEST_ASSERT_FALSE(d.app.channel(2).enabled); // outputs start off
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1 - 455000, d.app.frequency(2));
    assertHz(1, DEFAULT_FREQ_CH1 - 455000);
}

// NVS from before N14 has neither key: CH2 stays free with its stored frequency.
static void test_old_nvs_stays_free() {
    Preferences p;
    p.begin("hfgen");
    p.putULong64("ch1_freq", 7000000);
    p.putULong64("ch2_freq", 21000000);
    Device d;
    TEST_ASSERT_FALSE(d.app.ch2Coupled());
    TEST_ASSERT_EQUAL_INT32(0, d.app.ch2Coupling().offset);
    TEST_ASSERT_EQUAL_UINT64(21000000, d.app.frequency(2));
}

static void test_invalid_nvs_values() {
    Preferences p;
    p.begin("hfgen");
    p.putUChar("ch2_cpl", 7);
    p.putInt("ch2_off", 200000000);
    Device d;
    TEST_ASSERT_FALSE(d.app.ch2Coupled());
    TEST_ASSERT_EQUAL_INT32(0, d.app.ch2Coupling().offset);
}

static void test_defaults_and_rst_set_free() {
    Device d;
    d.app.setCh2Offset(1000);
    d.app.setCh2Coupled(true);
    d.app.resetForRemote();
    TEST_ASSERT_FALSE(d.app.ch2Coupled());
    TEST_ASSERT_EQUAL_INT32(0, d.app.ch2Coupling().offset);
    d.app.setCh2Offset(1000);
    d.app.setCh2Coupled(true);
    d.app.resetToDefaults();
    TEST_ASSERT_FALSE(d.app.ch2Coupled());
    d.run(SETTINGS_AUTOSAVE_MS + 200);
    Preferences p;
    p.begin("hfgen");
    TEST_ASSERT_EQUAL_UINT8(0, p.getUChar("ch2_cpl", 9));
    TEST_ASSERT_EQUAL_INT32(0, p.getInt("ch2_off", 9));
}

static void test_commands_and_snapshot() {
    Device d;
    Command c {};
    c.source = Source::Web;
    c.channel = 2;
    c.type = CommandType::Ch2Offset;
    c.value = 455000;
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, c));
    c.value = (int64_t)CH2_OFFSET_MAX_HZ + 1;
    TEST_ASSERT_EQUAL(Result::OutOfRange, execute(d.app, c));
    c.type = CommandType::Ch2Couple;
    c.value = 2;
    TEST_ASSERT_EQUAL(Result::Invalid, execute(d.app, c));
    c.value = 1;
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, c));
    c.type = CommandType::SetFrequency;
    c.value = 7000000;
    TEST_ASSERT_EQUAL(Result::Coupled, execute(d.app, c));
    StateSnapshot snap;
    snap.publish(d.app);
    Snapshot s;
    TEST_ASSERT_TRUE(snap.read(&s));
    TEST_ASSERT_TRUE(s.state.ch2c.coupled);
    TEST_ASSERT_EQUAL_INT32(455000, s.state.ch2c.offset);
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH1 + 455000, s.state.ch2.frequency);
}

// ---------------------------------------------------------------------------
// Pure helpers

static void test_format_offset() {
    char b[32];
    FrequencyUtils::formatOffset(455000, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("+455.000 kHz", b);
    FrequencyUtils::formatOffset(-10700000, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("-10.700 000 MHz", b);
    FrequencyUtils::formatOffset(0, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("+0.000 kHz", b);
    FrequencyUtils::formatOffset(-CH2_OFFSET_MAX_HZ, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("-160.000 000 MHz", b);
    FrequencyUtils::formatOffsetCompact(455000, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("+455 kHz", b);
    FrequencyUtils::formatOffsetCompact(-10700000, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("-10.7 MHz", b);
    FrequencyUtils::formatOffsetCompact(1500, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("+1.5 kHz", b);
    FrequencyUtils::formatOffsetCompact(-25, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("-25 Hz", b);
    FrequencyUtils::formatOffsetCompact(0, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("+0 Hz", b);
    FrequencyUtils::formatOffsetCompact(10000001, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("+10.000001 MHz", b);
    FrequencyUtils::formatOffsetCompact(INT64_MIN, b, sizeof(b)); // no overflow
    TEST_ASSERT_EQUAL_CHAR('-', b[0]);
}

static void test_coupled_wait_text_and_status() {
    TEST_ASSERT_EQUAL_STRING("Waiting: CH1 + offset below 8 kHz", StatusText::coupledWait(7999));
    TEST_ASSERT_EQUAL_STRING("Waiting: CH1 + offset below 8 kHz", StatusText::coupledWait(-5));
    TEST_ASSERT_EQUAL_STRING("", StatusText::coupledWait(8000));
    TEST_ASSERT_EQUAL_STRING("", StatusText::coupledWait(160000000));
    TEST_ASSERT_EQUAL_STRING("Waiting: CH1 + offset above 160 MHz", StatusText::coupledWait(160000001));
    TEST_ASSERT_EQUAL_STRING("WAIT", StatusText::channel(ChannelStatus::FreqTooLow).text);
    TEST_ASSERT_EQUAL(StatusText::Tone::Warn, StatusText::channel(ChannelStatus::FreqTooHigh).tone);
}

static int32_t offsetEntry(const char* const* keys, InputLogic::NumpadEvent* last, InputLogic::Numpad* pad = nullptr) {
    InputLogic::Numpad n;
    int32_t hz = 12345;
    for (; *keys; ++keys) *last = InputLogic::numpadOffsetKey(n, *keys, &hz);
    if (pad) *pad = n;
    return hz;
}

static void test_numpad_offset_entry() {
    using InputLogic::NumpadEvent;
    NumpadEvent e;
    const char* a[] = {"4", "5", "5", "kHz", nullptr};
    TEST_ASSERT_EQUAL_INT32(455000, offsetEntry(a, &e));
    TEST_ASSERT_EQUAL(NumpadEvent::Confirmed, e);
    const char* b[] = {"1", "0", ".", "7", InputLogic::KEY_SIGN, "MHz", nullptr};
    TEST_ASSERT_EQUAL_INT32(-10700000, offsetEntry(b, &e));
    const char* c[] = {"0", "Hz", nullptr};
    TEST_ASSERT_EQUAL_INT32(0, offsetEntry(c, &e));
    TEST_ASSERT_EQUAL(NumpadEvent::Confirmed, e);
    const char* d[] = {"1", "6", "0", ".", "0", "0", "0", "0", "0", "1", "MHz", nullptr};
    InputLogic::Numpad pad;
    offsetEntry(d, &e, &pad);
    TEST_ASSERT_EQUAL(NumpadEvent::Rejected, e);
    TEST_ASSERT_EQUAL(InputLogic::NumpadError::OffsetRange, pad.error);
    TEST_ASSERT_EQUAL_STRING("-160 to +160 MHz", InputLogic::numpadErrorText(pad.error));
    const char* f[] = {InputLogic::KEY_SIGN, "MHz", nullptr};
    offsetEntry(f, &e, &pad);
    TEST_ASSERT_EQUAL(InputLogic::NumpadError::Empty, pad.error);
    const char* g[] = {InputLogic::KEY_SIGN, "5", "C", "5", "Hz", nullptr};
    TEST_ASSERT_EQUAL_INT32(5, offsetEntry(g, &e)); // C clears the sign too
    const char* h[] = {"1", "6", "0", InputLogic::KEY_SIGN, "MHz", nullptr};
    TEST_ASSERT_EQUAL_INT32(-160000000, offsetEntry(h, &e));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_free_and_zero);
    RUN_TEST(test_coupled_runs_at_ch1_plus_offset);
    RUN_TEST(test_offset_limits_set_rejects_adjust_clamps);
    RUN_TEST(test_ch2_frequency_refused_while_coupled);
    RUN_TEST(test_ch1_change_retunes_ch2_without_dropout);
    RUN_TEST(test_wait_low_then_auto_start);
    RUN_TEST(test_wait_high_then_auto_start);
    RUN_TEST(test_switch_on_while_waiting_keeps_output_off);
    RUN_TEST(test_decouple_keeps_last_frequency);
    RUN_TEST(test_ch3_follows_coupled_master);
    RUN_TEST(test_persisted_and_restored);
    RUN_TEST(test_old_nvs_stays_free);
    RUN_TEST(test_invalid_nvs_values);
    RUN_TEST(test_defaults_and_rst_set_free);
    RUN_TEST(test_commands_and_snapshot);
    RUN_TEST(test_format_offset);
    RUN_TEST(test_coupled_wait_text_and_status);
    RUN_TEST(test_numpad_offset_entry);
    return UNITY_END();
}
