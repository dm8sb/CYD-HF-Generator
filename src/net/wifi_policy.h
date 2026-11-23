// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>
#include "net_settings.h"

// What the radio should do (W-E1, W-S2, W-S3, D14). Pure: the WiFi manager feeds in the
// observed state and applies the result; host-tested.
namespace WifiPolicy {

static constexpr uint32_t FALLBACK_AFTER_MS = 60000;      // client without IP -> fallback AP
static constexpr uint32_t OPEN_AP_IDLE_OFF_MS = 15 * 60000; // open AP without station -> off

struct Inputs {
    WifiMode mode;
    bool staHasIp;             // the client has an address (W-S3: the AP must go)
    uint32_t msWithoutIp;      // in Client mode since start or since the address was lost
    bool apOpen;               // settings: AP without password
    bool openFallbackConfirmed; // the touch confirmed an open fallback AP (W-S2)
    bool openApTimedOut;       // an open AP had no station for OPEN_AP_IDLE_OFF_MS
};

struct Targets {
    bool sta;      // client interface on (searching or connected)
    bool ap;       // access point on
    bool apOpen;   // without password
    bool fallback; // AP started because the client found no network
    bool needsOpenConfirmation; // an open fallback AP waits for the touch
};

Targets decide(const Inputs& in);

static constexpr int NOT_SEEN = -1000;
// Which stored network to try next. stored[i]: slot i holds a network; rssi[i]: its strongest
// access point in the last scan or NOT_SEEN; lastFailed: slot of the last failed attempt or -1;
// next: slot to try when none was seen (hidden SSIDs, round robin). The strongest seen network
// wins (the first slot at equal RSSI), except the one that just failed while the other is seen too (a wrong password on the
// stronger network must not lock out the other one). -1: nothing stored.
int chooseSlot(const bool stored[2], const int rssi[2], int lastFailed, uint8_t next);

// Why a connection attempt failed (feedback on the WIFI page).
enum class Failure : uint8_t { None, NotFound, WrongPassword, Other };
// Disconnect reasons of the ESP-IDF WiFi driver used here (wifi_err_reason_t; wifi_manager.cpp
// checks them against esp_wifi_types.h).
namespace Reason {
static constexpr uint16_t AUTH_EXPIRE = 2;
static constexpr uint16_t FOURWAY_HANDSHAKE_TIMEOUT = 15;
static constexpr uint16_t NO_AP_FOUND = 201;
static constexpr uint16_t AUTH_FAIL = 202;
static constexpr uint16_t HANDSHAKE_TIMEOUT = 204;
}
// The last disconnect reason of a timed-out attempt (0 = none came): not found, most often a
// wrong password (auth expired/failed, handshake timeouts) or anything else.
Failure classifyFailure(uint16_t reason);

// Signal strength and AP stations are read from the driver at most this often (AF11): every
// read is a cross-task call into the WiFi task.
static constexpr uint32_t STATUS_POLL_MS = 1000;
// True if a value last read at lastMs (polled: it was read at all) should be read again.
bool pollDue(uint32_t nowMs, uint32_t lastMs, bool polled);


// Strongest access point per stored network: one call per scan entry; stored[i] "" = empty
// slot. Returns the slots whose rssi[] it raised (bit 0, bit 1), so the caller can note the
// security of that entry.
uint8_t mergeSeen(const char* const stored[2], const char* seen, int seenRssi, int rssi[2]);

// An open AP without station since idleSinceMs reached OPEN_AP_IDLE_OFF_MS (W-S2).
bool openApIdleExpired(uint32_t nowMs, uint32_t idleSinceMs);

// Timing of the client while it has no address (AF13, TEST-9): background scans, connection
// attempts with their timeout and the slot that failed last. Pure state; WifiManager starts the
// scans and attempts and reports what happened. All times wrap-safe (millis()).
// Backoff (AF14, D32, PERF-5): no background scan without a stored network; the interval starts
// at SCAN_INTERVAL_MS and doubles after every scan without a stored network up to
// SCAN_BACKOFF_MAX_MS; a seen network or a connection starts again at SCAN_INTERVAL_MS. With
// stations at the fallback AP at least SCAN_WITH_STATIONS_MS (the AP leaves its channel for
// every scan and attempt). An attempt follows every background scan, so WiFi.begin() for an
// unseen (hidden) network runs at the same pace.
class Connector {
public:
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    static constexpr uint32_t SCAN_INTERVAL_MS = 15000;
    static constexpr uint32_t SCAN_BACKOFF_MAX_MS = 5 * 60000;
    static constexpr uint32_t SCAN_WITH_STATIONS_MS = 2 * 60000;
    enum class Step : uint8_t { None, TimedOut };

    // The client (re)starts: no attempt, nothing failed, the first scan is due at once.
    void reset();
    // Called in every loop() while the client has no address. TimedOut once when the running
    // attempt reached CONNECT_TIMEOUT_MS; lastFailed() is its slot then.
    Step poll(uint32_t nowMs);
    // A background scan should start now: a network is stored, no attempt runs and the interval
    // since the last scan has passed. apStations: stations are connected to the fallback AP.
    bool scanDue(uint32_t nowMs, const bool stored[2], bool apStations) const;
    uint32_t scanIntervalMs(bool apStations) const;
    // Result of a background scan: a stored network was in it (back to SCAN_INTERVAL_MS) or not
    // (the interval doubles).
    void scanResult(bool storedSeen);
    void scanStarted(uint32_t nowMs);
    // A scan of the WIFI page ended: the background scan waits SCAN_INTERVAL_MS, the backoff
    // starts again (someone is setting up the network).
    void scanEnded(uint32_t nowMs) {
        scanStarted(nowMs);
        intervalMs_ = SCAN_INTERVAL_MS;
    }
    // The slot to try after a scan (rssi[] from mergeSeen, NOT_SEEN for unseen), -1 = none
    // stored; advances the round robin for hidden networks.
    int choose(const bool stored[2], const int rssi[2]);
    void attemptStarted(int slot, uint32_t nowMs);
    // The client got an address: the attempt ended well, no slot counts as failed any more.
    void connected();
    bool attempting() const { return attempting_; }
    int lastFailed() const { return lastFailed_; }

private:
    bool attempting_ = false;
    uint32_t attemptMs_ = 0;
    int8_t attemptSlot_ = -1;
    int8_t lastFailed_ = -1;
    uint8_t next_ = 0;
    bool scanned_ = false; // a scan ran since reset() (millis() may be 0 at a real scan)
    uint32_t lastScanMs_ = 0;
    uint32_t intervalMs_ = SCAN_INTERVAL_MS;
};

}
