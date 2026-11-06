#include <Arduino.h>
#include <config.h>
#include <constants.h>
#include <pins.h>
#include <types.h>
#include <hal/i2c_bus.h>
#include <drivers/backlight.h>
#include <drivers/display_panel.h>
#include <drivers/si5351_manager.h>
#include <drivers/touch.h>
#include <services/settings.h>
#include <core/app_controller.h>
#include <remote/command.h>
#include <gui/gui.h>
#include <utils/build_info.h>
#include <utils/logger.h>
#include <utils/mem_monitor.h>

static SettingsService settings;
static BacklightDriver backlight;
static SI5351Manager si5351;
static TouchDriver touch;
static DisplayPanel display;
static AppController app(settings, si5351, backlight);
static Gui gui;
static PersistentState loadedState;
// Remote commands and the state copy for network tasks (V-6); filled from S50 on.
static CommandQueue commands;
static StateSnapshot snapshot;
static uint32_t publishedRevision = 0xFFFFFFFF;

static bool saveTouchCalibration(const TouchCalibration& cal) { return settings.saveTouchCalibration(cal); }

// Factory reset (U-13): the outputs are already off; erase and start like a new device.
static void factoryReset() {
    settings.eraseAll();
    Logger::info("BOOT", "Factory reset; restarting");
    delay(100);
    ESP.restart();
}

// A finger on the screen at boot must not reach LVGL as the first tap. The pressure
// reading flickers, so released means 200 ms without touch (at most 10 s).
static void waitForRelease() {
    const uint32_t t0 = millis();
    uint8_t free = 0;
    while (free < TOUCH_RELEASE_QUIET_SAMPLES && millis() - t0 < TOUCH_RELEASE_TIMEOUT_MS) {
        free = touch.pressed() ? 0 : free + 1;
        delay(TOUCH_RELEASE_SAMPLE_MS);
    }
}

void setup() {
    // 1. Serial / Logger
    Logger::begin(APP_SERIAL_BAUD);
    char line[128];
    snprintf(line, sizeof(line), "%s %s (%s %s, %s)", APP_NAME, BuildInfo::version(), BuildInfo::branch(),
             BuildInfo::commit(), BuildInfo::time());
    Logger::info("BOOT", line);

    // 2. I2C on CN1; which pin is SDA is detected in step 5
    I2CBus::begin();

    // 3. Backlight on GPIO21 PWM only; dark until the first frame
    backlight.begin();

    // 4. Settings load
    const bool settingsOk = settings.begin();
    if (!settingsOk) {
        Logger::error("BOOT", "NVS unavailable; using defaults, changes will not be saved");
    }
    settings.load(loadedState);
    app.begin(loadedState);

    // 5. SI5351 init, SDA/SCL in either order
    const bool si5351Ok = app.connectHardware();
    if (!si5351Ok) {
        Logger::error("BOOT", "SI5351 init failed");
    }

    // 6. Touch init
    // Calibration of this board (U-26); without one the wizard runs after the GUI start.
    TouchCalibration touchCal = TOUCH_CAL_DEFAULT;
    const bool touchCalStored = settings.loadTouchCalibration(&touchCal);
    if (!touch.begin(touchCal, touchCalStored)) {
        Logger::error("BOOT", "Touch init failed");
    }

    // 7. Display: detect the controller (ILI9341 / ST7789); a choice stored by the boot gesture
    // of 1.3.0-1.3.1 would override nothing any more and is deleted.
    settings.removeDisplayPanel();
    display.begin();
    if (touch.pressed()) waitForRelease();

    // 8. LVGL and GUI init
    gui.begin(app, touch, display);
    gui.setTouchCalibrationStore(saveTouchCalibration);
    gui.setFactoryReset(factoryReset);
    // The hit map build (TEST_HIT_MAP) checks a stored calibration and skips the wizard.
    if (!touchCalStored && !TEST_HIT_MAP) gui.calibrateTouch(true);
    app.addListener(Gui::onAppEvent, &gui);
    if (!si5351Ok) {
        gui.showMessage("SI5351", AppController::linkText(app.linkState()), (uint8_t)EventCategory::Link);
    }
    if (!settingsOk) {
        gui.showMessage("Settings", "Flash storage (NVS) unavailable. Settings will not be saved.");
    }
    const uint32_t frameStart = millis();
    lv_refr_now(nullptr);
    Logger::debugValue("GUI", "First frame ms", millis() - frameStart);
    backlight.setPercent(loadedState.sys.backlight);

    // 9. Program stored frequencies; all outputs start off (no-op without SI5351)
    app.applyStoredChannelSettings();
    // MAIN was drawn before the dividers existed; the actual frequencies ("~" in Integer) are known now.
    gui.refresh();

    Logger::info("BOOT", "Setup complete");
    MemMonitor::log("boot");

    // A blocked loop() restarts the device after 5 s instead of freezing with running outputs
    // (task watchdog with panic and reboot in the core configuration, audit C-3 / S-23).
    enableLoopWDT();
}

// Test hooks for the restart paths (config.h, MESSPLAN Q4/Q5); compiled out by default.
static void runRestartTests() {
    if (!TEST_LVGL_ASSERT && !TEST_LOOP_HANG) return;
    static bool done = false;
    if (done || millis() < TEST_TRIGGER_MS) return;
    done = true;
    if (TEST_LVGL_ASSERT) {
        Logger::info("TEST", "Filling the LVGL pool, then creating a label");
        while (lv_mem_alloc(256) != nullptr) {}
        lv_label_create(lv_scr_act());
    }
    if (TEST_LOOP_HANG) {
        Logger::info("TEST", "Blocking loop()");
        while (true) {}
    }
}

void loop() {
    lv_timer_handler();
    commands.drain(app);
    app.update();
    if (app.revision() != publishedRevision) {
        publishedRevision = app.revision();
        snapshot.publish(app);
    }
    gui.update();
    MemMonitor::update();
    runRestartTests();
    delay(5);
}
