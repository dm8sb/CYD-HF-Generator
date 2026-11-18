// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wifi_policy.h"

#include <string.h>

namespace WifiPolicy {

Targets decide(const Inputs& in) {
    Targets t {false, false, false, false, false};
    switch (in.mode) {
        case WifiMode::Off:
            break;
        case WifiMode::AccessPoint:
            // An open AP goes off after 15 min without a station and stays off (W-S2).
            t.ap = !(in.apOpen && in.openApTimedOut);
            t.apOpen = in.apOpen;
            break;
        case WifiMode::Client:
            t.sta = true;
            // W-S3 / D14: never an AP while the client has an address.
            if (in.staHasIp || in.msWithoutIp < FALLBACK_AFTER_MS) break;
            if (in.apOpen && !in.openFallbackConfirmed) {
                t.needsOpenConfirmation = true;
                break;
            }
            if (in.apOpen && in.openApTimedOut) break;
            t.ap = true;
            t.apOpen = in.apOpen;
            t.fallback = true;
            break;
    }
    return t;
}

int chooseSlot(const bool stored[2], const int rssi[2], int lastFailed, uint8_t next) {
    if (!stored[0] && !stored[1]) return -1;
    const bool seen0 = stored[0] && rssi[0] > NOT_SEEN;
    const bool seen1 = stored[1] && rssi[1] > NOT_SEEN;
    if (seen0 && seen1) {
        if (lastFailed == 0) return 1;
        if (lastFailed == 1) return 0;
        return rssi[1] > rssi[0] ? 1 : 0;
    }
    if (seen0) return 0;
    if (seen1) return 1;
    // None seen: the stored ones in turn.
    const uint8_t first = next % 2;
    return stored[first] ? first : 1 - first;
}

Failure classifyFailure(uint16_t reason) {
    switch (reason) {
        case Reason::NO_AP_FOUND: return Failure::NotFound;
        case Reason::AUTH_EXPIRE:
        case Reason::FOURWAY_HANDSHAKE_TIMEOUT:
        case Reason::AUTH_FAIL:
        case Reason::HANDSHAKE_TIMEOUT: return Failure::WrongPassword;
        default: return Failure::Other;
    }
}

bool pollDue(uint32_t nowMs, uint32_t lastMs, bool polled) { return !polled || nowMs - lastMs >= STATUS_POLL_MS; }

uint8_t mergeSeen(const char* const stored[2], const char* seen, int seenRssi, int rssi[2]) {
    uint8_t raised = 0;
    if (!seen) return 0;
    for (uint8_t k = 0; k < 2; ++k) {
        if (!stored[k] || !stored[k][0] || strcmp(stored[k], seen) != 0) continue;
        if (seenRssi > rssi[k]) {
            rssi[k] = seenRssi;
            raised |= (uint8_t)(1u << k);
        }
    }
    return raised;
}

bool openApIdleExpired(uint32_t nowMs, uint32_t idleSinceMs) { return nowMs - idleSinceMs >= OPEN_AP_IDLE_OFF_MS; }

void Connector::reset() { *this = Connector {}; }

Connector::Step Connector::poll(uint32_t nowMs) {
    if (!attempting_ || nowMs - attemptMs_ < CONNECT_TIMEOUT_MS) return Step::None;
    attempting_ = false;
    lastFailed_ = attemptSlot_;
    return Step::TimedOut;
}

uint32_t Connector::scanIntervalMs(bool apStations) const {
    return apStations && intervalMs_ < SCAN_WITH_STATIONS_MS ? SCAN_WITH_STATIONS_MS : intervalMs_;
}

bool Connector::scanDue(uint32_t nowMs, const bool stored[2], bool apStations) const {
    if (attempting_ || !(stored[0] || stored[1])) return false;
    return !scanned_ || nowMs - lastScanMs_ >= scanIntervalMs(apStations);
}

void Connector::scanResult(bool storedSeen) {
    if (storedSeen) intervalMs_ = SCAN_INTERVAL_MS;
    else intervalMs_ = intervalMs_ >= SCAN_BACKOFF_MAX_MS / 2 ? SCAN_BACKOFF_MAX_MS : 2 * intervalMs_;
}

void Connector::scanStarted(uint32_t nowMs) {
    scanned_ = true;
    lastScanMs_ = nowMs;
}

int Connector::choose(const bool stored[2], const int rssi[2]) {
    const int slot = chooseSlot(stored, rssi, lastFailed_, next_);
    if (slot >= 0) next_ = (uint8_t)((slot + 1) % 2);
    return slot;
}

void Connector::attemptStarted(int slot, uint32_t nowMs) {
    attempting_ = true;
    attemptMs_ = nowMs;
    attemptSlot_ = (int8_t)slot;
}

void Connector::connected() {
    attempting_ = false;
    lastFailed_ = -1;
    intervalMs_ = SCAN_INTERVAL_MS;
}

}
