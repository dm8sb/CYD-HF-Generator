// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include <app_fatal.h>

#include <Arduino.h>
#include <esp_system.h>

extern "C" void app_fatal_restart(const char* reason) {
    Serial.printf("[ERROR][FATAL] %s; restarting\n", reason ? reason : "unknown");
    Serial.flush();
    esp_restart();
}
