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

// SI5351 I2C
// Verified on this build by I2C scan: SI5351 found at 0x60 with SDA=22/SCL=27.
// If your wiring follows the original spec, swap these back to SDA=27/SCL=22.
static constexpr int PIN_I2C_SDA = 22;
static constexpr int PIN_I2C_SCL = 27;
static constexpr uint32_t I2C_FREQ_HZ = 400000;

// Backlight - reserved exclusively for PWM backlight
static constexpr int PIN_BACKLIGHT = 21;

// RGB LED, active low
static constexpr int PIN_LED_R = 4;
static constexpr int PIN_LED_G = 16;
static constexpr int PIN_LED_B = 17;

static constexpr int PIN_FREE_INPUT_ONLY = 35;
