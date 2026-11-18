// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// What a page shows besides the controller state (AF12, CQ-14): the GUI fills one StatusView
// per refresh check with exactly the fields of the current page (the others stay 0) and redraws
// only when it differs from the last one. Replaces the XOR hash, which mixed every page's
// fields (RSSI on all pages, the heap in 1 KB steps) and could miss changes by collision.
// Pure and host-tested.
struct StatusView {
    uint8_t remote;        // REMOTE indicator: 0 = off, else Source + 1
    uint8_t wifiMode;      // WifiMode
    bool staConnected;
    bool apActive;
    bool apOpen;
    bool fallback;
    bool needsOpenConfirmation;
    bool connecting;
    uint8_t apStations;
    uint8_t lastFailure;   // WifiPolicy::Failure
    uint8_t scanState;     // WifiManager::ScanState
    uint8_t scanCount;
    int8_t rssi;           // as shown (StatusViewRules::rssiShown), CONFIG only
    uint32_t staIp;
    uint32_t heapStep;     // free heap in HEAP_STEP_BYTES, CONFIG info tab only
    uint8_t webClients;
    uint32_t scpiClient;

    bool operator==(const StatusView& o) const {
        return remote == o.remote && wifiMode == o.wifiMode && staConnected == o.staConnected &&
               apActive == o.apActive && apOpen == o.apOpen && fallback == o.fallback &&
               needsOpenConfirmation == o.needsOpenConfirmation && connecting == o.connecting &&
               apStations == o.apStations && lastFailure == o.lastFailure && scanState == o.scanState &&
               scanCount == o.scanCount && rssi == o.rssi && staIp == o.staIp && heapStep == o.heapStep &&
               webClients == o.webClients && scpiClient == o.scpiClient;
    }
    bool operator!=(const StatusView& o) const { return !(*this == o); }
};

namespace StatusViewRules {

static constexpr int RSSI_HYSTERESIS_DB = 3;
static constexpr uint32_t HEAP_STEP_BYTES = 4096;
static constexpr uint32_t HEAP_SAMPLE_MS = 5000;

// The RSSI to show: the shown value follows only a change of RSSI_HYSTERESIS_DB or more, so a
// signal that flickers by 1-2 dB redraws nothing. 0 = not connected (shown as such at once).
inline int8_t rssiShown(int8_t shown, int8_t now) {
    if (now == 0 || shown == 0) return now;
    const int d = (int)now - (int)shown;
    return (d >= RSSI_HYSTERESIS_DB || d <= -RSSI_HYSTERESIS_DB) ? now : shown;
}

// Free heap in steps of HEAP_STEP_BYTES.
inline uint32_t heapStep(uint32_t freeBytes) { return freeBytes / HEAP_STEP_BYTES; }

// The heap is sampled (a walk of the heap under its lock) at most every HEAP_SAMPLE_MS.
inline bool heapDue(uint32_t nowMs, uint32_t lastMs, bool sampled) {
    return !sampled || nowMs - lastMs >= HEAP_SAMPLE_MS;
}

}
