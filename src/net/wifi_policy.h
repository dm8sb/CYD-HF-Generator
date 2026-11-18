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
// wins, except the one that just failed while the other is seen too (a wrong password on the
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

}
