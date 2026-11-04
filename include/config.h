#pragma once

#define APP_NAME "CYD HF Generator"
#define APP_VERSION "1.3.2"
#define APP_SERIAL_BAUD 115200

// 1 = verbose SI5351/backlight logging. Serial output blocks the UI, so keep it off in normal use.
#ifndef LOG_DEBUG
#define LOG_DEBUG 0
#endif

// Display controller: 0 = detect at boot, 1 = always ILI9341, 2 = always ST7789 (developer override).
#ifndef DISPLAY_FORCE_PANEL
#define DISPLAY_FORCE_PANEL 0
#endif

// 1 = test hook: the CH3 IQ setup fails at 270 deg, to check the ERR state on the device.
#ifndef TEST_FAULT_IQ
#define TEST_FAULT_IQ 0
#endif

// 1 = test hooks for the restart paths (MESSPLAN Q4, Q5), triggered TEST_TRIGGER_MS after boot:
// TEST_LVGL_ASSERT fills the LVGL pool and creates a widget (malloc assert),
// TEST_LOOP_HANG blocks loop() so the task watchdog restarts the device.
#ifndef TEST_LVGL_ASSERT
#define TEST_LVGL_ASSERT 0
#endif
#ifndef TEST_LOOP_HANG
#define TEST_LOOP_HANG 0
#endif

// Touch test builds (MESSPLAN B8, B9, B12), never in the release: TEST_TOUCH_LOG logs one line
// per touch (position, pressure, duration, dropouts, target); TEST_HIT_MAP shows the hit map
// with 20 target crosses instead of the GUI and implies the touch log.
#ifndef TEST_HIT_MAP
#define TEST_HIT_MAP 0
#endif
#ifndef TEST_TOUCH_LOG
#define TEST_TOUCH_LOG TEST_HIT_MAP
#endif
