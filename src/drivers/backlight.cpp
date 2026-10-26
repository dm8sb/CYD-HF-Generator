#include "backlight.h"
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>

static constexpr uint8_t BACKLIGHT_LEDC_CHANNEL = 0;
static constexpr uint32_t BACKLIGHT_LEDC_FREQ = 5000;
static constexpr uint8_t BACKLIGHT_LEDC_BITS = 8;

void BacklightDriver::begin() {
    ledcSetup(BACKLIGHT_LEDC_CHANNEL, BACKLIGHT_LEDC_FREQ, BACKLIGHT_LEDC_BITS);
    ledcAttachPin(PIN_BACKLIGHT, BACKLIGHT_LEDC_CHANNEL);
    setPercent(DEFAULT_BACKLIGHT_PERCENT);
    Logger::info("BACKLIGHT", "PWM on GPIO21 initialized");
}

void BacklightDriver::setPercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    percent_ = percent;
    uint8_t duty = (uint8_t)((uint16_t)percent * 255U / 100U);
    ledcWrite(BACKLIGHT_LEDC_CHANNEL, duty);
    Logger::debugValue("BACKLIGHT", "Percent", percent_);
}
