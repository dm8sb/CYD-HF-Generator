// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>
#include <remote/command.h>
#include "net_settings.h"
#include "wifi_manager.h"

// Web server of the WEB-HMI (S55, ARBEITSPLAN 3.3, 3.6, 3.7) on port 80: the embedded web UI,
// the REST API and the push-only WebSocket /ws. Device glue, not host-compiled; the decisions
// are in RequestGuard, RestJson, RestWifi, NetChange, ScanSession, LoginQueue, HttpPreFilter,
// HttpConnections and WsClients (all host-tested).
//
// Tasks: requests are handled in the async_tcp task. They never call the AppController, LVGL,
// I2C, NVS or the WiFi manager: writes go through the CommandQueue (the handler waits at most
// REPLY_WAIT_MS for its commands to show in the snapshot, then answers with the state), WiFi
// changes and scans through a one-slot job mailbox that loop() serves, the password check
// through LoginQueue to a low-priority worker task (W-S23: PBKDF2 takes ~0.2-0.3 s). Its answer
// is sent from the connection's poll callback, again in the async_tcp task. loop() runs the
// two-stage network change, the WebSocket push and the guard configuration. Shared data is
// guarded by one mutex. Created on the heap (E-31); the server starts when WiFi first comes up,
// stops listening when the WiFi mode becomes Off (its WebSockets closed, sessions ended) and
// listens again when WiFi is back.
class WebHmiServer {
public:
    static constexpr uint32_t REPLY_WAIT_MS = 80;   // < 100 ms per W-S23
    static constexpr uint16_t PORT = 80;

    // save writes the network settings (nullptr: RAM only, test builds).
    WebHmiServer(CommandQueue& commands, StateSnapshot& snapshot, WifiManager& wifi, bool (*save)(const NetSettings&));
    // In loop() after the snapshot was published and CommandQueue::markVisible().
    void loop();
    // Touch confirmation of a web network change without web password (W-S12), for the GUI.
    bool touchConfirmNeeded() const;
    void touchConfirm(bool keep);
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
