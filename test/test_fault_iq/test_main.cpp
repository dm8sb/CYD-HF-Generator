// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// IQ error state (ERR) with TEST_FAULT_IQ=1: the driver refuses the IQ pair at 270°.
// Runs only in the native-fault / native-win-fault environments (audit T-1, C-1).

#include <unity.h>
#include <Preferences.h>
#include <Wire.h>
#include <config.h>
#include <constants.h>
#include <core/app_controller.h>
#include <hal/i2c_bus.h>
#include <helpers/si5351_model.h>
#include <stdio.h>

#if !TEST_FAULT_IQ
#error "test_fault_iq needs -DTEST_FAULT_IQ=1 (env native-fault / native-win-fault)"
#endif

using namespace Si5351Model;

static Fake::Si5351Chip& chip = Fake::si5351();
static Fake::Nvs& nvs = Fake::nvs();
static unsigned errorCount;
static char lastErrorText[160];

static void onError(const char*, const char* text) {
    ++errorCount;
    snprintf(lastErrorText, sizeof(lastErrorText), "%s", text);
}

static Event lastEvent;
static void onEvent(const Event& e, void*) {
    lastEvent = e;
    onError(e.title, e.text);
}

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
        app.addListener(onEvent, nullptr);
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
    errorCount = 0;
    lastErrorText[0] = '\0';
    Fake::wireMisuse() = false;
    Wire.failBegin = false;
    Wire.resetCounters();
}
void tearDown() { TEST_ASSERT_FALSE(Fake::wireMisuse()); }

// CH1 on, CH3 following CH1 at 270° -> the IQ setup fails, CH3 shows ERR.
static void startFaultyIQ(Device& d) {
    d.app.setChannelEnabled(1, true);
    d.app.setCH3Phase(270);
    d.app.setChannelEnabled(3, true);
    TEST_ASSERT_EQUAL(Ch3Status::Error, d.app.ch3Status());
    // The dialog names the channel and the running master (audit U-21).
    TEST_ASSERT_EQUAL_STRING("CH3: IQ setup failed, CLK2 stays off. CH1 keeps running. Retrying every 2 s.", lastErrorText);
}

static void assertMasterRuns() {
    TEST_ASSERT_TRUE(enabled(0));
    TEST_ASSERT_NULL(limitViolation(0));
    TEST_ASSERT_DOUBLE_WITHIN(0.5, (double)DEFAULT_FREQ_CH1, outputHz(0));
}

static void test_error_keeps_master_running_and_clk2_off() {
    Device d;
    startFaultyIQ(d);
    assertMasterRuns();
    TEST_ASSERT_FALSE(enabled(2));
    TEST_ASSERT_EQUAL(1, errorCount);
}

// C-1: switching the master off and on again must not enable CLK2 while CH3 is in ERR,
// for every off state (High and High-Z keep the output driver powered).
static void test_master_toggle_keeps_clk2_off_in_error() {
    const OffState states[] = {OffState::Low, OffState::High, OffState::HighZ};
    for (OffState s : states) {
        setUp();
        Device d;
        d.app.setOffState(s);
        startFaultyIQ(d);
        d.app.setChannelEnabled(1, false);
        TEST_ASSERT_FALSE(enabled(0));
        d.app.setChannelEnabled(1, true);
        char msg[32];
        snprintf(msg, sizeof(msg), "off state %u", (unsigned)s);
        TEST_ASSERT_FALSE_MESSAGE(enabled(2), msg);
        assertMasterRuns();
    }
}

// Master switched on only after CH3 went to ERR.
static void test_master_on_after_error_keeps_clk2_off() {
    Device d;
    d.app.setOffState(OffState::High);
    d.app.setCH3Phase(270);
    d.app.setChannelEnabled(3, true);
    d.app.setChannelEnabled(1, true);
    TEST_ASSERT_EQUAL(Ch3Status::Error, d.app.ch3Status());
    TEST_ASSERT_FALSE(enabled(2));
    assertMasterRuns();
}

// E-12: one dialog, silent retries every 2 s, recovery after a working phase.
static void test_error_retries_quietly_and_clears() {
    Device d;
    startFaultyIQ(d);
    d.run(10000);
    TEST_ASSERT_EQUAL(1, errorCount);
    TEST_ASSERT_EQUAL(Ch3Status::Error, d.app.ch3Status());
    TEST_ASSERT_FALSE(enabled(2));
    d.app.setCH3Phase(90);
    d.run(HW_CHECK_INTERVAL_MS);
    TEST_ASSERT_EQUAL(Ch3Status::Active, d.app.ch3Status());
    TEST_ASSERT_TRUE(enabled(2));
    TEST_ASSERT_EQUAL(1, errorCount);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_error_keeps_master_running_and_clk2_off);
    RUN_TEST(test_master_toggle_keeps_clk2_off_in_error);
    RUN_TEST(test_master_on_after_error_keeps_clk2_off);
    RUN_TEST(test_error_retries_quietly_and_clears);
    return UNITY_END();
}
