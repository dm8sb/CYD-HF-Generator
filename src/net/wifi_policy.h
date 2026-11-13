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

}
