// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "heap_watch.h"

bool HeapWatch::update(uint32_t freeBytes) {
    if (isLow_) {
        if (freeBytes > recover_) isLow_ = false;
        return false;
    }
    if (freeBytes >= low_) return false;
    isLow_ = true;
    return true;
}
