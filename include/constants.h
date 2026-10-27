#pragma once

#include <Arduino.h>
#include <types.h>

static constexpr uint16_t SCREEN_WIDTH = 320;
static constexpr uint16_t SCREEN_HEIGHT = 240;

static constexpr uint64_t FREQ_MIN_HZ = 1000000ULL;
static constexpr uint64_t FREQ_MAX_HZ = 100000000ULL;
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
static constexpr uint8_t BACKLIGHT_MIN_PERCENT = 5; // 0 % would leave the device unusable
static constexpr uint8_t DEFAULT_DRIVE_MA = 4;

static constexpr uint32_t SETTINGS_AUTOSAVE_MS = 5000;
// SI5351 health check / reconnect interval.
static constexpr uint32_t HW_CHECK_INTERVAL_MS = 2000;
static constexpr const char* SETTINGS_NAMESPACE = "hfgen";

// Touch calibration is fixed; there is no runtime calibration UI.
static constexpr TouchCalibration TOUCH_CAL_DEFAULT = {300, 3800, 300, 3800};

static constexpr uint8_t CLK_CH1 = 0;
static constexpr uint8_t CLK_CH2 = 1;
static constexpr uint8_t CLK_CH3 = 2;

inline bool isValidDrive(uint8_t ma) { return ma == 2 || ma == 4 || ma == 6 || ma == 8; }
inline bool isValidPhase(uint16_t deg) { return deg == 0 || deg == 90 || deg == 180 || deg == 270; }
