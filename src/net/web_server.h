// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>
#include <remote/command.h>
#include "net_settings.h"
#include "wifi_manager.h"

// Web server of the WEB-HMI (S55, ARBEITSPLAN 3.3, 3.6, 3.7) on port 80: the embedded web UI,
// the REST API and the push-only WebSocket /ws. Device glue, not host-compiled; the decisions
// are in RequestGuard, RestJson, RestWifi, NetChange, ScanSession, LoginQueue, JobSlot,
// HttpPreFilter, HttpConnections and WsClients (all host-tested).
//
// Tasks: requests are handled in the async_tcp task. They never call the AppController, LVGL,
// I2C, NVS or the WiFi manager: writes go through the CommandQueue, WiFi changes and scans
// through a one-slot job mailbox that loop() serves. The handler waits at most 20 ms (woken by
// loop(), no polling) for the commands to show in the snapshot or the job to finish; a slower
// loop() gets the request paused and answered from the connection's poll callback, a write after
// at most REPLY_WAIT_MS with the state as it is (AF54, E-50). The password check goes through
// LoginQueue to a low-priority worker task (W-S23: PBKDF2 takes about 1.3 s); its answer is also
// sent from the poll callback. Every answer leaves in the async_tcp task. loop() runs the
// two-stage network change, the WebSocket push and the guard configuration. Shared data is
// guarded by one mutex (AF50: web_server.cpp groups the code by task into ConnectionGate,
// LoginService and Pusher; Shared holds the data under the mutex). Created on the heap (E-31);
// the server starts when WiFi first comes up, stops listening when the WiFi mode becomes Off
// (its WebSockets closed, sessions ended) and listens again when WiFi is back.
class WebHmiServer {
public:
    static constexpr uint32_t REPLY_WAIT_MS = 80;   // a paused write answers with the state after this
    static constexpr uint16_t PORT = 80;

    // save writes the network settings (nullptr: RAM only, test builds).
    WebHmiServer(CommandQueue& commands, StateSnapshot& snapshot, WifiManager& wifi, bool (*save)(const NetSettings&));
    // In loop() after the snapshot was published and CommandQueue::markVisible().
    void loop();
    // Touch confirmation of a web network change without web password (W-S12), for the GUI;
    // touchConfirm(false) reverts the change also with a web password (AF16).
    bool touchConfirmNeeded() const;
    void touchConfirm(bool keep);
    // Seconds an open web network change still waits, 0 = none (AF16: the touch screen does not
    // change network settings meanwhile, the WiFi manager does not save).
    uint16_t netChangePendingS() const;
    // Serial number of the LXI identification (GET /lxi/identification, A-78): the *IDN? one.
    void setSerial(const char* serial);
    // WebSocket clients now (info card).
    uint8_t webClients() const;
    // Disconnect at the device (info card, W-S20 for the web): closes every WebSocket and ends
    // every login session; served by the next loop(). A browser without web password connects
    // again by itself, with one it needs a new login.
    void disconnectAll();

    // Callbacks of the async_tcp task (public for the C-style trampolines in web_server.cpp).
    struct Impl;

private:
    Impl* impl_;
};
