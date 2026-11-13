// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)
// The pool is taken from the heap in lv_init() instead of a static array: with the WiFi stack
// the static DRAM segment (dram0_0_seg) is full (S53b); the heap has about 245 KB free.
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC malloc
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
// LVGL log lines go through the Logger (lv_log_register_print_cb in Gui::begin), so they can be
// buffered while SCPI uses the serial port (C-16).
#define LV_LOG_PRINTF 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
// A failed assert (e.g. LVGL pool full) restarts instead of the default while(1) (audit C-3).
#define LV_ASSERT_HANDLER_INCLUDE <app_fatal.h>
#define LV_ASSERT_HANDLER app_fatal_restart("LVGL assert");
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#define LV_DPI_DEF 130

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_USER_DATA 1

#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_SWITCH 1
#define LV_USE_SLIDER 0
#define LV_USE_DROPDOWN 1
#define LV_USE_MSGBOX 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1 /* WIFI text entry (S53b) */
#define LV_USE_BAR 0

/* Unused widgets (LVGL enables them by default) */
#define LV_USE_ARC 0
#define LV_USE_CANVAS 1 /* needed by the QR code */
#define LV_USE_CHECKBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_TABLE 0
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_LED 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/* QR code of the access point page (S53c) */
#define LV_USE_QRCODE 1

#endif
