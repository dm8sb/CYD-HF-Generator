// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Heap and LVGL memory figures in the debug log (LOG_DEBUG 1): the baseline for the
// migration and the WEB-HMI heap limits (audit V-8). Compiled out in release builds.
// Not part of the host tests (ESP-IDF heap and LVGL APIs).
namespace MemMonitor {
// One log line with the current figures; `when` names the moment (e.g. "boot").
void log(const char* when);
// Call from loop(): logs every MEM_LOG_INTERVAL_MS.
void update();
}
