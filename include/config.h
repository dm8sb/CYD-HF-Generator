#pragma once

#define APP_NAME "CYD HF Generator"
#define APP_VERSION "1.1.0"
#define APP_SERIAL_BAUD 115200

// 1 = verbose SI5351/backlight logging. Serial output blocks the UI, so keep it off in normal use.
#ifndef LOG_DEBUG
#define LOG_DEBUG 0
#endif
