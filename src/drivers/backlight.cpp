#include "backlight.h"
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>

// Arduino 3.x picks the LEDC channel itself and addresses the PWM by pin.
static constexpr uint32_t BACKLIGHT_LEDC_FREQ = 5000;
static constexpr uint8_t BACKLIGHT_LEDC_BITS = 8;

void BacklightDriver::begin() {
    ledcAttach(PIN_BACKLIGHT, BACKLIGHT_LEDC_FREQ, BACKLIGHT_LEDC_BITS);
    // Stays dark until the first frame is drawn; the panel shows noise before its init.
    setPercent(0);
    Logger::info("BACKLIGHT", "PWM on GPIO21 initialized");
}

void BacklightDriver::setPercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    percent_ = percent;
    uint8_t duty = (uint8_t)((uint16_t)percent * 255U / 100U);
    ledcWrite(PIN_BACKLIGHT, duty);
    Logger::debugValue("BACKLIGHT", "Percent", percent_);
}
