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

static void showError(const char* title, const char* text) {
    gui.showMessage(title, text);
}

// Touch held from power-on for 1.5 s: use the other display controller. Works without a
// readable screen, so it helps when the display shows only noise.
static bool bootGesture() {
    if (!touch.pressed()) return false;
    uint8_t held = 0;
    for (uint8_t i = 0; i < BOOT_GESTURE_SAMPLES; ++i) {
        held += touch.pressed() ? 1 : 0;
        delay(BOOT_GESTURE_SAMPLE_MS);
    }
    return TouchFilter::bootGestureHeld(held);
}

static void blinkBacklight(uint8_t times) {
    for (uint8_t i = 0; i < times; ++i) {
        backlight.setPercent(100);
        delay(BOOT_GESTURE_BLINK_MS);
        backlight.setPercent(0);
        delay(BOOT_GESTURE_BLINK_MS);
    }
}

// A finger on the screen at boot must not reach LVGL as the first tap. The pressure
// reading flickers, so released means 200 ms without touch (at most 10 s).
static void waitForRelease() {
    const uint32_t t0 = millis();
    uint8_t free = 0;
    while (free < TOUCH_RELEASE_QUIET_SAMPLES && millis() - t0 < TOUCH_RELEASE_TIMEOUT_MS) {
        free = touch.pressed() ? 0 : free + 1;
        delay(BOOT_GESTURE_SAMPLE_MS);
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
    if (!touch.begin(TOUCH_CAL_DEFAULT)) {
        Logger::error("BOOT", "Touch init failed");
    }

    // 7. Display: detect the controller (ILI9341 / ST7789), boot gesture overrides
    const bool touchedAtBoot = touch.pressed();
    const bool gesture = touchedAtBoot && bootGesture();
    const PanelDecision panel = display.begin((PanelType)settings.loadDisplayPanel(), gesture);
    if (panel.save && !settings.saveDisplayPanel((uint8_t)panel.saveValue)) {
        Logger::error("DISPLAY", "Saving the display choice failed");
    }
    // Two blinks: the boot gesture switched the controller (ignored with DISPLAY_FORCE_PANEL).
    if (panel.source == PanelSource::Gesture) blinkBacklight(2);
    if (touchedAtBoot) waitForRelease();

    // 8. LVGL and GUI init
    gui.begin(app, touch, display);
    app.setErrorCallback(showError);
    if (!si5351Ok) {
        gui.showMessage("SI5351", AppController::linkText(app.linkState()));
    }
    if (!settingsOk) {
        gui.showMessage("Settings", "Flash storage (NVS) unavailable. Settings will not be saved.");
    }
    lv_refr_now(nullptr);
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
    app.update();
    gui.update();
    MemMonitor::update();
    runRestartTests();
    delay(5);
}
