// AppController with SettingsService, SI5351Manager and BacklightDriver on the fakes
// (test/fakes): output states, CH3 IQ behavior, supervision and persistence.

#include <unity.h>
#include <Preferences.h>
#include <Wire.h>
#include <constants.h>
#include <core/app_controller.h>
#include <helpers/si5351_model.h>
#include <string.h>

using namespace Si5351Model;

static Fake::Si5351Chip& chip = Fake::si5351();
static Fake::Nvs& nvs = Fake::nvs();

static unsigned errorCount;
static char lastErrorTitle[32];
static char lastErrorText[160];

static void onError(const char* title, const char* text) {
    ++errorCount;
    snprintf(lastErrorTitle, sizeof(lastErrorTitle), "%s", title);
    snprintf(lastErrorText, sizeof(lastErrorText), "%s", text);
}

// Boots like main.cpp: settings, SI5351, backlight, stored channel settings.
struct Device {
    SettingsService settings;
    SI5351Manager si;
    BacklightDriver backlight;
    AppController app;

    Device() : app(settings, si, backlight) {
        settings.begin();
        PersistentState loaded;
        settings.load(loaded);
        app.begin(loaded);
        backlight.begin();
        si.begin(loaded.sys.freq_correction);
        app.setErrorCallback(onError);
        backlight.setPercent(loaded.sys.backlight);
        app.applyStoredChannelSettings();
    }

    // Main loop for `ms` milliseconds.
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
    errorCount = 0;
    lastErrorTitle[0] = lastErrorText[0] = '\0';
}
void tearDown() {}

static void assertHz(uint8_t clk, uint64_t expected, int32_t corr = 0) {
    TEST_ASSERT_NULL(limitViolation(clk, corr));
    TEST_ASSERT_DOUBLE_WITHIN(1.35, (double)expected, outputHz(clk, corr));
}

static void startIQ(Device& d) {
    d.app.setChannelEnabled(1, true);
    d.app.setChannelEnabled(3, true);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
}

// ---------------------------------------------------------------------------
// Outputs

static void test_boot_with_defaults_all_outputs_off() {
    Device d;
    TEST_ASSERT_TRUE(d.app.hardwareReady());
    TEST_ASSERT_EQUAL_HEX8(0xFF, chip.regs[3]);
    TEST_ASSERT_EQUAL(ChannelStatus::Off, d.app.channelStatus(1));
    TEST_ASSERT_EQUAL(ChannelStatus::Off, d.app.channelStatus(2));
    TEST_ASSERT_EQUAL(Ch3Status::Off, d.app.ch3Status());
    // Frequencies are programmed, so switching on needs no retune.
    assertHz(0, DEFAULT_FREQ_CH1);
    assertHz(1, DEFAULT_FREQ_CH2);
    TEST_ASSERT_TRUE(poweredDown(2));
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_DRIVE_MA, driveMa(0));
    TEST_ASSERT_EQUAL_UINT32(DEFAULT_BACKLIGHT_PERCENT * 255 / 100, Fake::ledc().duty);
    TEST_ASSERT_EQUAL_UINT(0, errorCount);
}

static void test_switch_channel_on_and_off() {
    Device d;
    d.app.setChannelEnabled(2, true);
    TEST_ASSERT_TRUE(enabled(1));
    TEST_ASSERT_FALSE(enabled(0));
    TEST_ASSERT_EQUAL(ChannelStatus::On, d.app.channelStatus(2));
    d.app.setChannelEnabled(2, false);
    TEST_ASSERT_FALSE(enabled(1));
    TEST_ASSERT_EQUAL(ChannelStatus::Off, d.app.channelStatus(2));
}

static void test_retune_running_channel_without_dropout() {
    Device d;
    d.app.setChannelEnabled(1, true);
    chip.clearLog();
    d.app.setChannelFrequency(1, 7074000);
    for (int i = 0; i < 100; ++i) d.app.adjustChannelFrequency(1, 10);
    TEST_ASSERT_EQUAL_UINT(0, chip.writesTo(3));
    TEST_ASSERT_EQUAL_UINT(0, chip.totalPllResets());
    TEST_ASSERT_TRUE(enabled(0));
    TEST_ASSERT_EQUAL_UINT64(7075000ULL, d.app.channel(1).frequency);
    assertHz(0, 7075000);
}

static void test_frequency_is_clamped_to_range() {
    Device d;
    d.app.setChannelFrequency(1, 0);
    TEST_ASSERT_EQUAL_UINT64(FREQ_MIN_HZ, d.app.channel(1).frequency);
    d.app.adjustChannelFrequency(1, -1000000);
    TEST_ASSERT_EQUAL_UINT64(FREQ_MIN_HZ, d.app.channel(1).frequency);
    d.app.setChannelFrequency(1, 250000000ULL);
    TEST_ASSERT_EQUAL_UINT64(FREQ_MAX_HZ, d.app.channel(1).frequency);
    d.app.adjustChannelFrequency(1, 1000000);
    TEST_ASSERT_EQUAL_UINT64(FREQ_MAX_HZ, d.app.channel(1).frequency);
    d.app.setChannelFrequency(1, 1500000);
    d.app.adjustChannelFrequency(1, -1000000);
    TEST_ASSERT_EQUAL_UINT64(FREQ_MIN_HZ, d.app.channel(1).frequency);
    assertHz(0, FREQ_MIN_HZ);
    // Only CH1 and CH2 have their own frequency.
    d.app.setChannelFrequency(3, 5000000);
    d.app.adjustChannelFrequency(0, 1);
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH2, d.app.channel(2).frequency);
}

static void test_global_drive_strength() {
    Device d;
    startIQ(d);
    d.app.setGlobalDriveStrength(8);
    for (uint8_t clk = 0; clk < 3; ++clk) TEST_ASSERT_EQUAL_UINT8(8, driveMa(clk));
    d.app.setGlobalDriveStrength(5);
    TEST_ASSERT_EQUAL_UINT8(8, d.app.system().default_drive_strength);
    TEST_ASSERT_EQUAL_UINT8(8, driveMa(0));
}

static void test_correction_retunes_all_outputs() {
    Device d;
    startIQ(d);
    d.app.setChannelEnabled(2, true);
    d.app.setCorrection(970);
    assertHz(0, DEFAULT_FREQ_CH1, 970);
    assertHz(1, DEFAULT_FREQ_CH2, 970);
    assertHz(2, DEFAULT_FREQ_CH1, 970);
    TEST_ASSERT_TRUE(enabled(0) && enabled(1) && enabled(2));
    d.app.setCorrection(1000000);
    TEST_ASSERT_EQUAL_INT32(CORRECTION_MAX, d.app.system().freq_correction);
    d.app.setCorrection(-1000000);
    TEST_ASSERT_EQUAL_INT32(-CORRECTION_MAX, d.app.system().freq_correction);
    assertHz(0, DEFAULT_FREQ_CH1, -CORRECTION_MAX);
}

static void test_backlight_and_step_are_limited() {
    Device d;
    d.app.setBacklight(0);
    TEST_ASSERT_EQUAL_UINT8(BACKLIGHT_MIN_PERCENT, d.app.system().backlight);
    TEST_ASSERT_EQUAL_UINT32(BACKLIGHT_MIN_PERCENT * 255 / 100, Fake::ledc().duty);
    d.app.setBacklight(150);
    TEST_ASSERT_EQUAL_UINT8(100, d.app.system().backlight);
    TEST_ASSERT_EQUAL_UINT32(255, Fake::ledc().duty);
    d.app.setDeltaStep(5000);
    TEST_ASSERT_EQUAL_UINT32(1000, d.app.system().delta_step);
}

// ---------------------------------------------------------------------------
// CH3 IQ

static void test_ch3_waits_for_master_then_runs() {
    Device d;
    d.app.setChannelEnabled(3, true);
    TEST_ASSERT_EQUAL(Ch3Status::MasterOff, d.app.ch3Status());
    TEST_ASSERT_FALSE(enabled(2));
    d.app.setChannelEnabled(1, true);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    TEST_ASSERT_TRUE(enabled(0) && enabled(2));
    assertHz(2, DEFAULT_FREQ_CH1);
    TEST_ASSERT_EQUAL_UINT8(90, phase(2)); // 10 MHz: divider 90
    TEST_ASSERT_EQUAL_MEMORY(chip.regs + 42, chip.regs + 58, 8);
}

static void test_ch3_follows_master_retune() {
    Device d;
    startIQ(d);
    d.app.setChannelFrequency(1, 14200000);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    TEST_ASSERT_TRUE(enabled(0) && enabled(2));
    assertHz(0, 14200000);
    assertHz(2, 14200000);
    TEST_ASSERT_EQUAL_UINT8(63, phase(2));
    TEST_ASSERT_EQUAL_UINT64(14200000ULL, d.app.ch3Frequency());
}

static void test_ch3_below_iq_range_waits_and_resumes() {
    Device d;
    startIQ(d);
    d.app.setChannelFrequency(1, IQ_MIN_HZ - 10000);
    TEST_ASSERT_EQUAL(Ch3Status::FreqTooLow, d.app.ch3Status());
    TEST_ASSERT_TRUE(d.app.ch3().enabled); // the request is kept
    TEST_ASSERT_TRUE(poweredDown(2));
    TEST_ASSERT_TRUE(enabled(0));
    assertHz(0, IQ_MIN_HZ - 10000);
    d.app.setChannelFrequency(1, IQ_MIN_HZ);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    TEST_ASSERT_TRUE(enabled(2));
    assertHz(2, IQ_MIN_HZ);
}

static void test_ch3_master_switch() {
    Device d;
    startIQ(d);
    d.app.setCH3Master(2);
    TEST_ASSERT_EQUAL(Ch3Status::MasterOff, d.app.ch3Status());
    TEST_ASSERT_FALSE(enabled(2));
    // CH1 keeps running standalone on PLL A.
    TEST_ASSERT_TRUE(enabled(0));
    TEST_ASSERT_FALSE(usesPllB(0));
    assertHz(0, DEFAULT_FREQ_CH1);
    d.app.setChannelEnabled(2, true);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    TEST_ASSERT_TRUE(usesPllB(1) && usesPllB(2));
    assertHz(2, DEFAULT_FREQ_CH2);
    TEST_ASSERT_EQUAL_MEMORY(chip.regs + 50, chip.regs + 58, 8);
    // Invalid masters are ignored.
    d.app.setCH3Master(3);
    TEST_ASSERT_EQUAL_UINT8(2, d.app.ch3().master);
}

static void test_ch3_phase_change() {
    Device d;
    startIQ(d);
    d.app.setCH3Phase(180);
    TEST_ASSERT_TRUE(inverted(2));
    TEST_ASSERT_EQUAL_UINT8(0, phase(2));
    d.app.setCH3Phase(270);
    TEST_ASSERT_TRUE(inverted(2));
    TEST_ASSERT_EQUAL_UINT8(90, phase(2));
    d.app.setCH3Phase(45);
    TEST_ASSERT_EQUAL_UINT16(270, d.app.ch3().phase);
    TEST_ASSERT_TRUE(enabled(0) && enabled(2));
}

static void test_master_off_switches_follower_off() {
    Device d;
    startIQ(d);
    d.app.setChannelEnabled(1, false);
    TEST_ASSERT_FALSE(enabled(0));
    TEST_ASSERT_FALSE(enabled(2));
    TEST_ASSERT_EQUAL(Ch3Status::MasterOff, d.app.ch3Status());
    d.app.setChannelEnabled(1, true);
    TEST_ASSERT_TRUE(enabled(0) && enabled(2));
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
}

static void test_ch3_off_powers_clk2_down() {
    Device d;
    startIQ(d);
    d.app.setChannelEnabled(3, false);
    TEST_ASSERT_EQUAL(Ch3Status::Off, d.app.ch3Status());
    TEST_ASSERT_TRUE(poweredDown(2));
    TEST_ASSERT_TRUE(enabled(0));
    assertHz(0, DEFAULT_FREQ_CH1);
}

// ---------------------------------------------------------------------------
// Supervision

static void test_boot_without_si5351_then_hot_plug() {
    chip.present = false;
    Device d;
    TEST_ASSERT_FALSE(d.app.hardwareReady());
    TEST_ASSERT_EQUAL(ChannelStatus::NoDevice, d.app.channelStatus(1));
    // Settings can still be changed.
    d.app.setChannelFrequency(1, 7000000);
    d.app.setChannelEnabled(1, true);
    d.app.setChannelEnabled(3, true);
    TEST_ASSERT_EQUAL(Ch3Status::NoDevice, d.app.ch3Status());
    TEST_ASSERT_EQUAL_UINT(0, errorCount); // main.cpp shows the boot dialog itself
    chip.present = true;
    d.run(HW_CHECK_INTERVAL_MS + 100);
    TEST_ASSERT_TRUE(d.app.hardwareReady());
    TEST_ASSERT_EQUAL(ChannelStatus::On, d.app.channelStatus(1));
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    TEST_ASSERT_TRUE(enabled(0) && enabled(2));
    assertHz(0, 7000000);
    assertHz(2, 7000000);
}

static void test_power_glitch_reports_once_and_restores() {
    Device d;
    d.app.setChannelFrequency(1, 7100000);
    startIQ(d);
    d.app.setCH3Phase(270);
    chip.powerOn(); // SI5351 lost its configuration
    d.run(HW_CHECK_INTERVAL_MS + 100);
    TEST_ASSERT_EQUAL_UINT(1, errorCount);
    TEST_ASSERT_NOT_NULL(strstr(lastErrorText, "Connection lost"));
    TEST_ASSERT_EQUAL(ChannelStatus::NoDevice, d.app.channelStatus(1));
    d.run(HW_CHECK_INTERVAL_MS + 100);
    TEST_ASSERT_TRUE(d.app.hardwareReady());
    TEST_ASSERT_EQUAL_UINT(1, errorCount);
    TEST_ASSERT_TRUE(enabled(0) && enabled(2));
    assertHz(0, 7100000);
    TEST_ASSERT_TRUE(inverted(2));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)ms(0).ratio(), phase(2));
}

static void test_bus_failure_while_tuning_shows_no_hw() {
    Device d;
    d.app.setChannelEnabled(1, true);
    chip.failAfter = 0;
    d.app.setChannelFrequency(1, 8000000);
    TEST_ASSERT_EQUAL_UINT(1, errorCount);
    TEST_ASSERT_NOT_NULL(strstr(lastErrorText, "Connection lost"));
    // A lost device is NO HW, not an output error.
    TEST_ASSERT_EQUAL(ChannelStatus::NoDevice, d.app.channelStatus(1));
    chip.failAfter = -1;
    d.run(HW_CHECK_INTERVAL_MS + 100);
    TEST_ASSERT_EQUAL(ChannelStatus::On, d.app.channelStatus(1));
    assertHz(0, 8000000);
}

static void test_healthy_device_is_not_reconfigured() {
    Device d;
    startIQ(d);
    chip.clearLog();
    d.run(10 * HW_CHECK_INTERVAL_MS);
    // Supervision only reads; it never touches a running output.
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    TEST_ASSERT_EQUAL_UINT(0, errorCount);
}

// ---------------------------------------------------------------------------
// Persistence

static void test_autosave_after_quiet_period() {
    Device d;
    d.app.setChannelFrequency(1, 7100000);
    TEST_ASSERT_EQUAL(SaveState::Pending, d.app.saveState());
    // Every change restarts the delay, so holding +/- does not wear the flash.
    for (int i = 0; i < 10; ++i) {
        d.run(1000);
        d.app.adjustChannelFrequency(1, 1000);
    }
    TEST_ASSERT_EQUAL_UINT(0, nvs.writeCount);
    d.run(SETTINGS_AUTOSAVE_MS - 200);
    TEST_ASSERT_EQUAL_UINT(0, nvs.writeCount);
    d.run(300);
    TEST_ASSERT_TRUE(nvs.writeCount > 0);
    TEST_ASSERT_EQUAL(SaveState::Saved, d.app.saveState());
    Preferences p;
    TEST_ASSERT_EQUAL_UINT64(7110000ULL, p.getULong64("ch1_freq", 0));
}

static void test_save_failure_is_reported() {
    Device d;
    nvs.failWrites = true;
    d.app.setDeltaStep(100);
    d.run(SETTINGS_AUTOSAVE_MS + 100);
    TEST_ASSERT_EQUAL_UINT(1, errorCount);
    TEST_ASSERT_EQUAL_STRING("Settings", lastErrorTitle);
    TEST_ASSERT_EQUAL(SaveState::Failed, d.app.saveState());
    nvs.failWrites = false;
    d.app.setDeltaStep(10);
    d.run(SETTINGS_AUTOSAVE_MS + 100);
    TEST_ASSERT_EQUAL(SaveState::Saved, d.app.saveState());
    TEST_ASSERT_EQUAL_UINT(1, errorCount);
}

static void test_nvs_unavailable() {
    nvs.available = false;
    Device d;
    TEST_ASSERT_EQUAL(SaveState::Unavailable, d.app.saveState());
    d.app.setChannelFrequency(1, 7100000);
    d.run(SETTINGS_AUTOSAVE_MS + 100);
    TEST_ASSERT_EQUAL_UINT(0, nvs.writeCount);
    TEST_ASSERT_EQUAL(SaveState::Unavailable, d.app.saveState());
    assertHz(0, 7100000);
}

static void test_settings_survive_reboot_but_outputs_start_off() {
    {
        Device d;
        d.app.setChannelFrequency(1, 7100000);
        d.app.setChannelFrequency(2, 21000000);
        d.app.setChannelEnabled(1, true);
        d.app.setChannelEnabled(2, true);
        d.app.setChannelEnabled(3, true);
        d.app.setCH3Master(2);
        d.app.setCH3Phase(270);
        d.app.setCorrection(-1234);
        d.app.setGlobalDriveStrength(6);
        d.app.setBacklight(50);
        d.app.setDeltaStep(100);
        d.run(SETTINGS_AUTOSAVE_MS + 100);
        TEST_ASSERT_EQUAL(SaveState::Saved, d.app.saveState());
    }
    chip.powerOn();
    Device d;
    TEST_ASSERT_EQUAL_UINT64(7100000ULL, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL_UINT64(21000000ULL, d.app.channel(2).frequency);
    TEST_ASSERT_EQUAL_UINT8(2, d.app.ch3().master);
    TEST_ASSERT_EQUAL_UINT16(270, d.app.ch3().phase);
    TEST_ASSERT_EQUAL_INT32(-1234, d.app.system().freq_correction);
    TEST_ASSERT_EQUAL_UINT8(6, d.app.system().default_drive_strength);
    TEST_ASSERT_EQUAL_UINT8(50, d.app.system().backlight);
    TEST_ASSERT_EQUAL_UINT32(100, d.app.system().delta_step);
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    TEST_ASSERT_FALSE(d.app.channel(2).enabled);
    TEST_ASSERT_FALSE(d.app.ch3().enabled);
    TEST_ASSERT_EQUAL_HEX8(0xFF, chip.regs[3]);
    TEST_ASSERT_EQUAL_UINT8(6, driveMa(0));
    assertHz(0, 7100000, -1234);
}

static void test_settings_load_sanitizes_invalid_values() {
    Preferences p;
    p.putULong64("ch1_freq", 0);
    p.putULong64("ch2_freq", 1000000000ULL);
    p.putUChar("ch3_master", 3);
    p.putUInt("ch3_phase", 45);
    p.putUChar("backlight", 0);
    p.putUInt("delta", 7);
    p.putInt("corr", 99999);
    p.putUChar("drv_def", 5);
    p.putUChar("ch1_en", 1); // stored by firmware 1.1.0, ignored since 1.2.0
    Device d;
    TEST_ASSERT_EQUAL_UINT64(FREQ_MIN_HZ, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL_UINT64(FREQ_MAX_HZ, d.app.channel(2).frequency);
    TEST_ASSERT_EQUAL_UINT8(1, d.app.ch3().master);
    TEST_ASSERT_EQUAL_UINT16(90, d.app.ch3().phase);
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_BACKLIGHT_PERCENT, d.app.system().backlight);
    TEST_ASSERT_EQUAL_UINT32(1, d.app.system().delta_step);
    TEST_ASSERT_EQUAL_INT32(CORRECTION_MAX, d.app.system().freq_correction);
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_DRIVE_MA, d.app.system().default_drive_strength);
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    p.putUChar("backlight", 101);
    p.putInt("corr", -99999);
    Device d2;
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_BACKLIGHT_PERCENT, d2.app.system().backlight);
    TEST_ASSERT_EQUAL_INT32(-CORRECTION_MAX, d2.app.system().freq_correction);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_boot_with_defaults_all_outputs_off);
    RUN_TEST(test_switch_channel_on_and_off);
    RUN_TEST(test_retune_running_channel_without_dropout);
    RUN_TEST(test_frequency_is_clamped_to_range);
    RUN_TEST(test_global_drive_strength);
    RUN_TEST(test_correction_retunes_all_outputs);
    RUN_TEST(test_backlight_and_step_are_limited);
    RUN_TEST(test_ch3_waits_for_master_then_runs);
    RUN_TEST(test_ch3_follows_master_retune);
    RUN_TEST(test_ch3_below_iq_range_waits_and_resumes);
    RUN_TEST(test_ch3_master_switch);
    RUN_TEST(test_ch3_phase_change);
    RUN_TEST(test_master_off_switches_follower_off);
    RUN_TEST(test_ch3_off_powers_clk2_down);
    RUN_TEST(test_boot_without_si5351_then_hot_plug);
    RUN_TEST(test_power_glitch_reports_once_and_restores);
    RUN_TEST(test_bus_failure_while_tuning_shows_no_hw);
    RUN_TEST(test_healthy_device_is_not_reconfigured);
    RUN_TEST(test_autosave_after_quiet_period);
    RUN_TEST(test_save_failure_is_reported);
    RUN_TEST(test_nvs_unavailable);
    RUN_TEST(test_settings_survive_reboot_but_outputs_start_off);
    RUN_TEST(test_settings_load_sanitizes_invalid_values);
    return UNITY_END();
}
