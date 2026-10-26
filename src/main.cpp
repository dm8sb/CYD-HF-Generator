#include <Arduino.h>
#include <config.h>
#include <constants.h>
#include <pins.h>
#include <types.h>
#include <hal/i2c_bus.h>
#include <drivers/backlight.h>
#include <drivers/si5351_manager.h>
#include <drivers/touch.h>
#include <services/settings.h>
#include <core/app_controller.h>
#include <gui/gui.h>
#include <utils/logger.h>

static SettingsService settings;
static BacklightDriver backlight;
static SI5351Manager si5351;
static TouchDriver touch;
static AppController app(settings, si5351, backlight);
static PersistentState loadedState;

static void showError(const char* title, const char* text) {
    GUI.showMessage(title, text);
}

void setup() {
    // 1. Serial / Logger
    Logger::begin(APP_SERIAL_BAUD);
    Logger::info("BOOT", APP_NAME " " APP_VERSION);

    // 2. I2C explicit pins from include/pins.h @ 400kHz
    I2CBus::begin();

    // 3. Backlight on GPIO21 PWM only
    backlight.begin();

    // 4. Settings load
    if (!settings.begin()) {
        Logger::error("BOOT", "NVS unavailable; using defaults, changes will not be saved");
    }
    settings.load(loadedState);
    app.begin(loadedState);

    // 5. SI5351 init
    bool si5351Ok = si5351.begin(loadedState.sys.freq_correction);
    if (!si5351Ok) {
        Logger::error("BOOT", "SI5351 init failed");
    }

    // 6. Touch init
    if (!touch.begin(TOUCH_CAL_DEFAULT)) {
        Logger::error("BOOT", "Touch init failed");
    }

    // 7-8. LVGL and GUI init
    GUI.begin(app, touch);
    app.setErrorCallback(showError);
    if (!si5351Ok) {
        GUI.showMessage("SI5351", "No I2C ACK at 0x60. Check SDA/SCL pins, VCC, GND, OE pin and pullups.");
    }

    // 9. Apply stored channel settings safely (no-op without SI5351)
    app.applyStoredChannelSettings();

    Logger::info("BOOT", "Setup complete");
}

void loop() {
    lv_timer_handler();
    app.update();
    GUI.update();
    delay(5);
}
