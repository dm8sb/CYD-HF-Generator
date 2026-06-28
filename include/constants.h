#pragma once

#include <Arduino.h>

static constexpr uint16_t SCREEN_WIDTH = 320;
static constexpr uint16_t SCREEN_HEIGHT = 240;

static constexpr uint64_t FREQ_MIN_HZ = 1000000ULL;
static constexpr uint64_t FREQ_MAX_HZ = 100000000ULL;
static constexpr uint64_t DEFAULT_FREQ_CH1 = 10000000ULL;
static constexpr uint64_t DEFAULT_FREQ_CH2 = 14000000ULL;
static constexpr uint32_t DEFAULT_DELTA_STEP = 1000;
static constexpr uint8_t DEFAULT_BACKLIGHT_PERCENT = 80;
static constexpr uint8_t DEFAULT_DRIVE_MA = 4;

static constexpr uint32_t SETTINGS_AUTOSAVE_MS = 5000;
static constexpr const char* SETTINGS_NAMESPACE = "hfgen";

static constexpr uint8_t CLK_CH1 = 0;
static constexpr uint8_t CLK_CH2 = 1;
static constexpr uint8_t CLK_CH3 = 2;
