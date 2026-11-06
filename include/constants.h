#pragma once

#include <stdint.h>
#include <types.h>

static constexpr uint16_t SCREEN_WIDTH = 320;
static constexpr uint16_t SCREEN_HEIGHT = 240;

// CH1/CH2 range (E-16): below 1 MHz the R divider, above 112.5 MHz an even integer MultiSynth.
// CH3 (IQ) stays within IQ_MIN_HZ..IQ_MAX_HZ (4.730-100 MHz).
static constexpr uint64_t FREQ_MIN_HZ = 8000ULL;
static constexpr uint64_t FREQ_MAX_HZ = 160000000ULL;
static constexpr uint64_t DEFAULT_FREQ_CH1 = 10000000ULL;
static constexpr uint64_t DEFAULT_FREQ_CH2 = 14000000ULL;

// CH3 IQ range: the phase offset register limits the MultiSynth divider to 127, so the VCO
// stays >= 600 MHz (SI5351 spec) only from 600 MHz / 127 upwards. The fractional-mode divider
// must be >= 9, which limits the top end to 900 MHz / 9.
static constexpr uint64_t IQ_MIN_HZ = 4730000ULL;
static constexpr uint64_t IQ_MAX_HZ = 100000000ULL;

// Frequency step presets in Hz (CH pages and CONFIG x10 / /10).
static constexpr uint32_t DELTA_STEPS[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
static constexpr uint8_t DELTA_STEP_COUNT = sizeof(DELTA_STEPS) / sizeof(DELTA_STEPS[0]);
static constexpr uint32_t DEFAULT_DELTA_STEP = 1000;

// Frequency correction unit is 0.01 ppm (= Hz deviation @ 100 MHz); positive = crystal runs fast.
static constexpr int32_t CORRECTION_MAX = 5000; // +-50 ppm
static constexpr int32_t CORRECTION_STEPS[] = {1, 10, 100}; // 0.01 / 0.1 / 1 ppm
static constexpr uint8_t CORRECTION_STEP_COUNT = sizeof(CORRECTION_STEPS) / sizeof(CORRECTION_STEPS[0]);

static constexpr uint8_t DEFAULT_BACKLIGHT_PERCENT = 80;
static constexpr uint8_t BACKLIGHT_MIN_PERCENT = 10; // 0 % would leave the device unusable
static constexpr uint8_t BACKLIGHT_STEP_PERCENT = 10;
static constexpr uint8_t DEFAULT_DRIVE_MA = 4;

static constexpr uint32_t SETTINGS_AUTOSAVE_MS = 5000;
// After a failed save the change is kept and saved again after this delay (audit C-13).
static constexpr uint32_t SETTINGS_RETRY_MS = 30000;
// Changes from remote commands are saved after this much quiet (W-E17: scripts change often).
static constexpr uint32_t SETTINGS_REMOTE_AUTOSAVE_MS = 30000;
// Write commands per second and remote source (W-S11); more are refused (HTTP 429, SCPI -200 busy).
static constexpr uint8_t REMOTE_WRITES_PER_S = 20;
// SYSTem:IDENtify / web Identify (W-E28): backlight and RGB LED blink for this long.
static constexpr uint32_t IDENTIFY_MS = 5000;
static constexpr uint32_t IDENTIFY_BLINK_MS = 250;
// SCPI over USB: the session (log only into the ring buffer, W-E27) ends after this idle time.
static constexpr uint32_t SCPI_IDLE_TIMEOUT_MS = 600000;
// SI5351 health check / reconnect interval.
static constexpr uint32_t HW_CHECK_INTERVAL_MS = 2000;
// Heap / LVGL memory line in the debug log (LOG_DEBUG 1 only).
static constexpr uint32_t MEM_LOG_INTERVAL_MS = 30000;
// GUI and touch timing.
static constexpr uint32_t GUI_UPDATE_INTERVAL_MS = 300;  // save state, connection, output errors
// Pressure dropouts (and weak pressure) shorter than this keep a press (audit U-20: 60-80 ms;
// a tap is reported this much later as released).
static constexpr uint32_t TOUCH_RELEASE_BRIDGE_MS = 70;
// Holding +/- (U-20): the first repeat after REPEAT_DELAY_MS, then one step per
// REPEAT_SLOW_INTERVAL_MS (about 3/s), from REPEAT_FAST_MS every LVGL repeat (about 10/s),
// from REPEAT_FASTER_MS 2 steps per repeat (about 20/s).
static constexpr uint32_t REPEAT_DELAY_MS = 600;
static constexpr uint32_t REPEAT_SLOW_INTERVAL_MS = 300;
static constexpr uint32_t REPEAT_FAST_MS = 3000;
static constexpr uint32_t REPEAT_FASTER_MS = 6000;
// Released after a touch at boot: TOUCH_RELEASE_QUIET_SAMPLES x TOUCH_RELEASE_SAMPLE_MS without touch.
static constexpr uint32_t TOUCH_RELEASE_SAMPLE_MS = 20;
static constexpr uint8_t TOUCH_RELEASE_QUIET_SAMPLES = 10;
static constexpr uint32_t TOUCH_RELEASE_TIMEOUT_MS = 10000;
// Screen lock (U-13): holding this long anywhere unlocks.
static constexpr uint32_t UNLOCK_HOLD_MS = 2000;
// Display controller detection: probes before the default (E-18).
static constexpr uint8_t DISPLAY_PROBE_ATTEMPTS = 3;
// Delay of the restart test hooks (TEST_LVGL_ASSERT, TEST_LOOP_HANG): time to switch outputs on.
static constexpr uint32_t TEST_TRIGGER_MS = 20000;
static constexpr const char* SETTINGS_NAMESPACE = "hfgen";

// Touch calibration is fixed; there is no runtime calibration UI.
static constexpr TouchCalibration TOUCH_CAL_DEFAULT = {300, 3800, 300, 3800};

static constexpr uint8_t CLK_CH1 = 0;
static constexpr uint8_t CLK_CH2 = 1;
static constexpr uint8_t CLK_CH3 = 2;

inline bool isValidDrive(uint8_t ma) { return ma == 2 || ma == 4 || ma == 6 || ma == 8; }
inline bool isValidPhase(uint16_t deg) { return deg == 0 || deg == 90 || deg == 180 || deg == 270; }
// Nearest backlight step within BACKLIGHT_MIN_PERCENT..100.
inline uint8_t snapBacklight(int percent) {
    int p = (percent + BACKLIGHT_STEP_PERCENT / 2) / BACKLIGHT_STEP_PERCENT * BACKLIGHT_STEP_PERCENT;
    if (p < BACKLIGHT_MIN_PERCENT) p = BACKLIGHT_MIN_PERCENT;
    if (p > 100) p = 100;
    return (uint8_t)p;
}
