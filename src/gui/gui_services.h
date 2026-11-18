// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <net/net_settings.h>
#include <net/web_state.h>
#include <utils/touch_cal.h>

class WifiManager;

// What the GUI uses of the rest of the firmware (AF49c, CQ-3): one struct set by main instead of
// a setter per callback. Every entry is optional; without one the GUI shows the feature as off
// (no WiFi: the WiFi rows show Off) or skips it. All of them are called in loop().
struct GuiServices {
    // Touch calibration wizard (U-26): saves a successful calibration.
    bool (*storeTouchCal)(const TouchCalibration&) = nullptr;
    // Factory reset (U-13): erases the NVS and restarts (does not return).
    void (*factoryReset)() = nullptr;
    // WiFi (S53): status for the status bar and the info card, the WIFI pages; saveNet writes
    // the network settings.
    WifiManager* wifi = nullptr;
    bool (*saveNet)(const NetSettings&) = nullptr;
    // Web network change without web password (W-S12, S55): netConfirmNeeded asks whether the
    // touch must confirm now, netConfirmAnswer(true) keeps the new settings, (false) restores
    // the old ones. netPendingS: seconds an open web change still waits (0 = none, AF16);
    // network settings are not changed at the touch screen meanwhile.
    bool (*netConfirmNeeded)() = nullptr;
    void (*netConfirmAnswer)(bool keep) = nullptr;
    uint16_t (*netPendingS)() = nullptr;
    // Web password on the device page (S55b): webPasswordStart hands the typed password to the
    // hashing worker (false: busy or invalid), webPasswordPoll fetches the record once ready.
    bool (*webPasswordStart)(const char* password, size_t len) = nullptr;
    WebPasswordJob::Poll (*webPasswordPoll)(char* record) = nullptr;
    // Remote clients on the info card (S55b): open WebSockets, address of the SCPI TCP client
    // (0 = none), and Reconnect all (served by the next loop()).
    uint8_t (*webClients)() = nullptr;
    uint32_t (*scpiClient)() = nullptr;
    void (*disconnectRemote)() = nullptr;
};
