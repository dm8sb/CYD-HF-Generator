#include "backlight.h"
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>

// Arduino 3.x picks the LEDC channel itself and addresses the PWM by pin; 2.0.x needs a
// channel. The 2.0.x branch goes with the 2.x env in S41.
static constexpr uint8_t BACKLIGHT_LEDC_CHANNEL = 0;
static constexpr uint32_t BACKLIGHT_LEDC_FREQ = 5000;
static constexpr uint8_t BACKLIGHT_LEDC_BITS = 8;

void BacklightDriver::begin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_BACKLIGHT, BACKLIGHT_LEDC_FREQ, BACKLIGHT_LEDC_BITS);
#else
    ledcSetup(BACKLIGHT_LEDC_CHANNEL, BACKLIGHT_LEDC_FREQ, BACKLIGHT_LEDC_BITS);
    ledcAttachPin(PIN_BACKLIGHT, BACKLIGHT_LEDC_CHANNEL);
#endif
    // Stays dark until the first frame is drawn; the panel shows noise before its init.
    setPercent(0);
    Logger::info("BACKLIGHT", "PWM on GPIO21 initialized");
}

void BacklightDriver::setPercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    percent_ = percent;
    uint8_t duty = (uint8_t)((uint16_t)percent * 255U / 100U);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_BACKLIGHT, duty);
#else
    ledcWrite(BACKLIGHT_LEDC_CHANNEL, duty);
#endif
    Logger::debugValue("BACKLIGHT", "Percent", percent_);
}
