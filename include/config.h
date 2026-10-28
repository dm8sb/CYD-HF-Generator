#pragma once

#define APP_NAME "CYD HF Generator"
#define APP_VERSION "1.2.0"
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
