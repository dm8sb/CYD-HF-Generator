#include "scpi_tcp_server.h"

#include <errno.h>
#include <lwip/sockets.h>
#include <constants.h>
#include <utils/logger.h>

namespace {
// Per loop(): at most this many reads of CHUNK bytes and new connections.
constexpr uint8_t READS_PER_UPDATE = 4;
constexpr size_t CHUNK = 64;
constexpr uint8_t ACCEPTS_PER_UPDATE = 2;

bool wouldBlock(int err) { return err == EWOULDBLOCK || err == EAGAIN; }

uint32_t ipv4(const IPAddress& a) { return (uint32_t)a[0] << 24 | (uint32_t)a[1] << 16 | (uint32_t)a[2] << 8 | a[3]; }
}

ScpiTcpServer::ScpiTcpServer(ScpiBackend& backend, const char* serial)
    : server_(SCPI_TCP_PORT), link_(backend, *this, serial) {}

void ScpiTcpServer::update(bool allowed) {
    const uint32_t now = millis();
    if (allowed != link_.allowed()) {
        link_.setAllowed(allowed); // false closes the client through closeConnection()
        if (allowed) {
            server_.begin();
            server_.setNoDelay(true); // answers are short lines: no Nagle delay
            Logger::value("SCPI", "TCP server on port", SCPI_TCP_PORT);
        } else {
            server_.end();
            Logger::info("SCPI", "TCP server stopped");
        }
    }
    if (!allowed) return;
    // The current client first: a closed one frees the place for a new connection.
    receive_(now);
    accept_(now);
    link_.update(now);
}

void ScpiTcpServer::receive_(uint32_t nowMs) {
    char buf[CHUNK];
    for (uint8_t i = 0; i < READS_PER_UPDATE && link_.wantsInput(); ++i) {
        const int fd = client_.fd();
        if (fd < 0) {
            link_.peerClosed();
            return;
        }
        const int n = lwip_recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n > 0) {
            link_.receive(buf, (size_t)n, nowMs);
            continue;
        }
        // 0: the client closed the connection (FIN); < 0 other than "no data": it failed.
        if (n == 0 || !wouldBlock(errno)) link_.peerClosed();
        return;
    }
}

void ScpiTcpServer::accept_(uint32_t nowMs) {
    for (uint8_t i = 0; i < ACCEPTS_PER_UPDATE; ++i) {
        WiFiClient c = server_.accept();
        if (c.fd() < 0) return;
        if (link_.offer(ipv4(c.remoteIP()), nowMs) != ScpiTcpLink::Accept::Accepted) {
            c.stop(); // W-E13: one connection; refused without an answer
            continue;
        }
        client_ = c;
        // W-S20: a vanished client (cable, sleeping laptop) is detected by keepalive probes.
        int on = 1;
        int idle = SCPI_TCP_KEEPALIVE_IDLE_S;
        int interval = SCPI_TCP_KEEPALIVE_INTERVAL_S;
        int count = SCPI_TCP_KEEPALIVE_COUNT;
        const bool keepalive = client_.setSocketOption(SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) == 0 &&
                               client_.setSocketOption(IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == 0 &&
                               client_.setSocketOption(IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) == 0 &&
                               client_.setSocketOption(IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) == 0;
        if (!keepalive) Logger::error("SCPI", "TCP keepalive not set");
        client_.setNoDelay(true);
    }
}

int32_t ScpiTcpServer::trySend(const char* data, size_t len) {
    const int fd = client_.fd();
    if (fd < 0) return -1;
    const int n = lwip_send(fd, data, len, MSG_DONTWAIT);
    if (n >= 0) return n;
    return wouldBlock(errno) ? 0 : -1;
}

void ScpiTcpServer::closeConnection() { client_.stop(); }
