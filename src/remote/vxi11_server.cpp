// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vxi11_server.h"

#include <Arduino.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <lwip/sockets.h>
#include <utils/logger.h>

namespace {
// Per loop(): at most this many reads of CHUNK bytes per connection, new connections per
// listener and datagrams.
constexpr uint8_t READS_PER_UPDATE = 4;
constexpr size_t CHUNK = 128;
constexpr uint8_t ACCEPTS_PER_UPDATE = 2;
constexpr uint8_t DATAGRAMS_PER_UPDATE = 4;
// A socket that could not be opened (port taken) is tried again after this.
constexpr uint32_t RETRY_MS = 10000;

bool wouldBlock(int err) { return err == EWOULDBLOCK || err == EAGAIN; }

uint32_t ipv4(const sockaddr_in& a) { return ntohl(a.sin_addr.s_addr); }

int openSocket(int type, uint16_t port) {
    const int fd = lwip_socket(AF_INET, type, type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
    if (fd < 0) return -1;
    int on = 1;
    lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (type == SOCK_DGRAM) lwip_setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (lwip_bind(fd, (sockaddr*)&a, sizeof(a)) < 0 || (type == SOCK_STREAM && lwip_listen(fd, 2) < 0) ||
        lwip_fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        lwip_close(fd);
        return -1;
    }
    return fd;
}

void closeSocket(int& fd) {
    if (fd >= 0) lwip_close(fd);
    fd = -1;
}
}

int32_t Vxi11Server::Conn::trySend(const char* data, size_t len) {
    if (fd < 0) return -1;
    const int n = lwip_send(fd, data, len, MSG_DONTWAIT);
    if (n >= 0) return n;
    return wouldBlock(errno) ? 0 : -1;
}

void Vxi11Server::Conn::closeConnection() { closeSocket(fd); }

Vxi11Server::PortmapSlot::PortmapSlot(PortmapProgram& program)
    : link(program, conn, in, sizeof(in), out, sizeof(out), PORTMAP_TCP_IDLE_MS, nullptr) {}

Vxi11Server::Vxi11Server(ScpiBackend& backend, const char* serial)
    : coreProgram_(backend, serial), portmapProgram_(VXI11_CORE_PORT),
      coreLink_(coreProgram_, coreConn_, coreIn_, sizeof(coreIn_), coreOut_, sizeof(coreOut_), SCPI_IDLE_TIMEOUT_MS,
                "VXI-11"),
      portmap_ {{portmapProgram_}, {portmapProgram_}} {
    static_assert(PORTMAP_LINKS == 2, "initializer list above");
    for (size_t i = 0; i < PORTMAP_LINKS; ++i) portmapLinks_[i] = &portmap_[i].link;
}

bool Vxi11Server::open_() {
    coreListen_ = openSocket(SOCK_STREAM, VXI11_CORE_PORT);
    portmapListen_ = openSocket(SOCK_STREAM, Portmap::PORT);
    udp_ = openSocket(SOCK_DGRAM, Portmap::PORT);
    if (coreListen_ < 0 || portmapListen_ < 0 || udp_ < 0) {
        close_();
        return false;
    }
    return true;
}

void Vxi11Server::close_() {
    closeSocket(coreListen_);
    closeSocket(portmapListen_);
    closeSocket(udp_);
}

void Vxi11Server::update(bool allowed) {
    const uint32_t now = millis();
    if (allowed != coreLink_.allowed()) {
        coreLink_.setAllowed(allowed); // false closes the clients through closeConnection()
        for (RpcTcpLink* l : portmapLinks_) l->setAllowed(allowed);
        if (!allowed && running_) {
            close_();
            running_ = false;
            Logger::info("SCPI", "VXI-11 stopped");
        }
        lastTryMs_ = now - RETRY_MS; // try at once
    }
    if (!allowed) return;
    if (!running_) {
        if (now - lastTryMs_ < RETRY_MS) return;
        lastTryMs_ = now;
        running_ = open_();
        if (!running_) {
            if (!failReported_) Logger::error("SCPI", "VXI-11 ports not available"); // retried every 10 s
            failReported_ = true;
            return;
        }
        failReported_ = false;
        char line[64];
        snprintf(line, sizeof(line), "VXI-11 on port %u, portmapper on port %u", (unsigned)VXI11_CORE_PORT,
                 (unsigned)Portmap::PORT);
        Logger::info("SCPI", line);
    }
    receive_(coreConn_, coreLink_, now);
    accept_(coreListen_, coreConn_, coreLink_, now);
    coreLink_.update(now);
    for (PortmapSlot& s : portmap_) receive_(s.conn, s.link, now);
    acceptPortmap_(now);
    for (PortmapSlot& s : portmap_) s.link.update(now);
    pollUdp_(now);
}

void Vxi11Server::receive_(Conn& conn, RpcTcpLink& link, uint32_t nowMs) {
    uint8_t buf[CHUNK];
    for (uint8_t i = 0; i < READS_PER_UPDATE && link.wantsInput(); ++i) {
        if (conn.fd < 0) {
            link.peerClosed();
            return;
        }
        // Never past the end of a record: one reply waits at most (RpcTcpLink).
        size_t want = link.wantBytes();
        if (want > sizeof(buf)) want = sizeof(buf);
        const int n = lwip_recv(conn.fd, buf, want, MSG_DONTWAIT);
        if (n > 0) {
            link.receive(buf, (size_t)n, nowMs);
            continue;
        }
        // 0: the client closed the connection (FIN); < 0 other than "no data": it failed.
        if (n == 0 || !wouldBlock(errno)) link.peerClosed();
        return;
    }
}

// Portmapper over TCP: a pool of stateless links; a new connection replaces the one idle the
// longest when all are busy (clients ask once and close; PORTMAP_TCP_IDLE_MS closes the rest).
void Vxi11Server::acceptPortmap_(uint32_t nowMs) {
    for (uint8_t i = 0; i < ACCEPTS_PER_UPDATE; ++i) {
        sockaddr_in peer {};
        socklen_t len = sizeof(peer);
        const int fd = lwip_accept(portmapListen_, (sockaddr*)&peer, &len);
        if (fd < 0) return;
        const size_t k = RpcTcpLink::offerPool(portmapLinks_, PORTMAP_LINKS, ipv4(peer), nowMs);
        if (k >= PORTMAP_LINKS) {
            lwip_close(fd);
            continue;
        }
        portmap_[k].conn.fd = fd;
        int on = 1;
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    }
}

void Vxi11Server::accept_(int listenFd, Conn& conn, RpcTcpLink& link, uint32_t nowMs) {
    for (uint8_t i = 0; i < ACCEPTS_PER_UPDATE; ++i) {
        sockaddr_in peer {};
        socklen_t len = sizeof(peer);
        const int fd = lwip_accept(listenFd, (sockaddr*)&peer, &len);
        if (fd < 0) return;
        if (link.offer(ipv4(peer), nowMs) != RpcTcpLink::Accept::Accepted) {
            lwip_close(fd); // one connection; refused without an answer
            continue;
        }
        conn.fd = fd;
        int on = 1;
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)); // one reply per call
        // W-S20: a vanished client is detected by keepalive probes, as for SCPI over TCP.
        int idle = SCPI_TCP_KEEPALIVE_IDLE_S;
        int interval = SCPI_TCP_KEEPALIVE_INTERVAL_S;
        int count = SCPI_TCP_KEEPALIVE_COUNT;
        const bool ok = lwip_setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) == 0 &&
                        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == 0 &&
                        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) == 0 &&
                        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) == 0;
        if (!ok) Logger::error("SCPI", "VXI-11 keepalive not set");
    }
}

// Portmapper datagrams (lxi discover broadcasts GETPORT to UDP 111).
void Vxi11Server::pollUdp_(uint32_t nowMs) {
    for (uint8_t i = 0; i < DATAGRAMS_PER_UPDATE; ++i) {
        sockaddr_in from {};
        socklen_t len = sizeof(from);
        const int n = lwip_recvfrom(udp_, udpIn_, sizeof(udpIn_), MSG_DONTWAIT, (sockaddr*)&from, &len);
        if (n <= 0) return; // nothing waiting (a datagram larger than the buffer is cut and fails)
        if (from.sin_port == 0 || !udpBudget_.take(nowMs)) continue;
        const size_t r = Portmap::handle(udpIn_, (size_t)n, Portmap::Transport::Udp, VXI11_CORE_PORT, udpOut_,
                                         sizeof(udpOut_));
        if (r) lwip_sendto(udp_, udpOut_, r, MSG_DONTWAIT, (sockaddr*)&from, len);
    }
}
