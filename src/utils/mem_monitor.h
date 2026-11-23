// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Heap and LVGL memory figures in the debug log (LOG_DEBUG 1): the baseline for the
// migration and the WEB-HMI heap limits (audit V-8). Compiled out in release builds.
// The `Heap low` warning (A-726, FW-08) runs in every build; its decision is HeapWatch.
// Not part of the host tests (ESP-IDF heap and LVGL APIs).
namespace MemMonitor {
// One log line with the current figures; `when` names the moment (e.g. "boot").
void log(const char* when);
// Call from loop(): checks the free heap every HEAP_CHECK_INTERVAL_MS (warning below
// HEAP_LOW_BYTES) and, with LOG_DEBUG, logs the figures every MEM_LOG_INTERVAL_MS.
void update();
}
