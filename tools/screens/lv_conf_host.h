// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
// LVGL configuration of the host screen renderer (env:screens-win / env:screens, selected by
// LV_CONF_PATH). It is the firmware's include/lv_conf.h with one host difference: the tick
// comes from a C function (LVGL's C sources cannot include the C++ fake Arduino.h).
#include "../../include/lv_conf.h"

#undef LV_TICK_CUSTOM_INCLUDE
#undef LV_TICK_CUSTOM_SYS_TIME_EXPR
#define LV_TICK_CUSTOM_INCLUDE "host_tick.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (host_millis())
