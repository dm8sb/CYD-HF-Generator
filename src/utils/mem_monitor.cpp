// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mem_monitor.h"

#include <Arduino.h>
#include <config.h>
#include <constants.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <utils/logger.h>

namespace MemMonitor {

void log(const char* when) {
    if (!LOG_DEBUG) return;
    lv_mem_monitor_t lvgl;
    lv_mem_monitor(&lvgl);
    char line[200];
    snprintf(line, sizeof(line),
             "%s: heap free %u min %u block %u | lvgl free %u of %u block %u max used %u | stack %u",
             when, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), (unsigned)lvgl.free_size,
             (unsigned)lvgl.total_size, (unsigned)lvgl.free_biggest_size, (unsigned)lvgl.max_used,
             (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    Logger::info("MEM", line);
}

void update() {
    if (!LOG_DEBUG) return;
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last < MEM_LOG_INTERVAL_MS) return;
    last = now;
    log("periodic");
}

}
