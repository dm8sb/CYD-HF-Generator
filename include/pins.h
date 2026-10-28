#pragma once

#include <Arduino.h>

// Display (HSPI: MOSI 13, MISO 12, SCK 14, CS 15, DC 2) is configured for TFT_eSPI
// by the build flags in platformio.ini, one environment per display controller.

// XPT2046 touch on VSPI
static constexpr int PIN_TOUCH_MOSI = 32;
static constexpr int PIN_TOUCH_MISO = 39;
static constexpr int PIN_TOUCH_SCK  = 25;
static constexpr int PIN_TOUCH_CS   = 33;
static constexpr int PIN_TOUCH_IRQ  = 36;

// SI5351 I2C on CN1. The firmware detects which pin is SDA, so the wires may be swapped.
static constexpr int PIN_I2C_A = 22;              // tried first as SDA (verified wiring)
static constexpr int PIN_I2C_B = 27;
static constexpr uint32_t I2C_DETECT_HZ = 100000; // detection tolerates weak pull-ups
static constexpr uint32_t I2C_FREQ_HZ = 400000;   // after a successful init
static constexpr uint16_t I2C_TIMEOUT_MS = 20;    // > 13.1 ms ESP32 hardware timeout
// GPIO21 (backlight, also on P3) is never an I2C candidate.

// Backlight - reserved exclusively for PWM backlight
static constexpr int PIN_BACKLIGHT = 21;

// RGB LED, active low
static constexpr int PIN_LED_R = 4;
static constexpr int PIN_LED_G = 16;
static constexpr int PIN_LED_B = 17;

static constexpr int PIN_FREE_INPUT_ONLY = 35;
