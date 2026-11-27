// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Warning when the free heap gets short (A-726, FW-08): the decision without Arduino calls, so
// it is host-tested; MemMonitor feeds it the free heap once per HEAP_CHECK_INTERVAL_MS and logs
// `Heap low` when update() returns true. Below `lowBytes` it warns once; it warns again only
// after the heap recovered above `recoverBytes` (hysteresis).
class HeapWatch {
public:
    HeapWatch(uint32_t lowBytes, uint32_t recoverBytes) : low_(lowBytes), recover_(recoverBytes) {}

    // One measurement; true when the warning is due now.
    bool update(uint32_t freeBytes);
    // True from the warning until the recovery above `recoverBytes`.
    bool low() const { return isLow_; }

private:
    uint32_t low_;
    uint32_t recover_;
    bool isLow_ = false;
};
