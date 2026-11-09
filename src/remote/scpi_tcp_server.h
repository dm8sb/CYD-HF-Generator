#pragma once

#include <WiFi.h>
#include "scpi_tcp.h"

// SCPI over TCP 5025 on the device (not host-compiled; the logic is in ScpiTcpLink). Polled in
// loop() like the USB link: non-blocking accept, recv and send on the lwIP socket, at most a few
// small chunks per call so a flood cannot starve the GUI. Created on the heap (static DRAM is
// full, E-31).
class ScpiTcpServer : public ScpiTcpSocket {
public:
    ScpiTcpServer(ScpiBackend& backend, const char* serial);
    // In loop(); allowed = ScpiTcpLink::allowedFor(WiFi status). Starts and stops listening.
    void update(bool allowed);
    ScpiTcpLink& link() { return link_; }

    int32_t trySend(const char* data, size_t len) override;
    void closeConnection() override;

private:
    void accept_(uint32_t nowMs);
    void receive_(uint32_t nowMs);

    WiFiServer server_;
    WiFiClient client_;
    ScpiTcpLink link_;
};
