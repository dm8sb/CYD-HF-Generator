// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>
#include <constants.h>
#include "onc_rpc.h"
#include "vxi11.h"

// VXI-11 on the device (A-78, E-37; not host-compiled, the logic is in onc_rpc.* and
// vxi11.*): the portmapper on UDP and TCP 111 and the core channel on TCP VXI11_CORE_PORT.
// Polled in loop() like ScpiTcpServer, on non-blocking lwIP sockets: per call at most a few
// datagrams, reads and accepts, so a flood cannot starve the GUI. Created on the heap (E-31).
class Vxi11Server {
public:
    Vxi11Server(ScpiBackend& backend, const char* serial);
    // In loop(); allowed: the same gate as SCPI over TCP. Opens and closes the sockets.
    void update(bool allowed);
    // The core channel's client (info card, Disconnect).
    RpcTcpLink& core() { return coreLink_; }

private:
    // An accepted TCP connection.
    struct Conn : ScpiTcpSocket {
        int fd = -1;
        int32_t trySend(const char* data, size_t len) override;
        void closeConnection() override;
    };

    // Portmapper TCP connections at a time (RpcTcpLink::offerPool): libtirpc keeps one open
    // for rpcbind v4/v3 and asks GETPORT on a second one.
    static constexpr size_t PORTMAP_LINKS = 2;
    struct PortmapSlot {
        Conn conn;
        uint8_t in[512];
        uint8_t out[128];
        RpcTcpLink link;
        PortmapSlot(PortmapProgram& program); // NOLINT: list-initialized in the array
        PortmapSlot(const PortmapSlot&) = delete;
        PortmapSlot& operator=(const PortmapSlot&) = delete;
    };

    bool open_();
    void close_();
    void accept_(int listenFd, Conn& conn, RpcTcpLink& link, uint32_t nowMs);
    void acceptPortmap_(uint32_t nowMs);
    void receive_(Conn& conn, RpcTcpLink& link, uint32_t nowMs);
    void pollUdp_(uint32_t nowMs);

    Vxi11Core coreProgram_;
    PortmapProgram portmapProgram_;
    Conn coreConn_;
    uint8_t coreIn_[Vxi11Core::MAX_RECORD];
    uint8_t coreOut_[Vxi11Core::MAX_REPLY];
    uint8_t udpIn_[512];
    uint8_t udpOut_[64];
    RpcTcpLink coreLink_;
    PortmapSlot portmap_[PORTMAP_LINKS];
    RpcTcpLink* portmapLinks_[PORTMAP_LINKS];
    Portmap::UdpBudget udpBudget_;
    int coreListen_ = -1;
    int portmapListen_ = -1;
    int udp_ = -1;
    bool running_ = false;
    bool failReported_ = false;
    uint32_t lastTryMs_ = 0;
};
