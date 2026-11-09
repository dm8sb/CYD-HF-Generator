#pragma once

#include <stddef.h>
#include <stdint.h>
#include "line_framer.h"
#include "scpi.h"

// The accepted TCP connection as ScpiTcpLink sees it; the device glue (scpi_tcp_server.*) wraps
// the WiFiClient, the host tests a fake. Method names avoid the lwIP socket macros (send, close).
class ScpiTcpSocket {
public:
    virtual ~ScpiTcpSocket() {}
    // Non-blocking: the bytes the stack took (0 while its send buffer is full), < 0 if the
    // connection failed.
    virtual int32_t trySend(const char* data, size_t len) = 0;
    // Closes the connection.
    virtual void closeConnection() = 0;
};

// SCPI over TCP 5025 (W-E12, W-E13, W-S18, W-S20). Pure logic, host-tested; it runs in loop()
// like the USB link, so commands go through LoopScpiBackend with Source::Scpi.
// - One connection: a second one is refused while one is active (the glue closes it at once).
// - A line that looks like HTTP (request or header line) closes the connection without executing
//   it or anything after it (cross-site requests from a browser).
// - SCPI_IDLE_TIMEOUT_MS without a line closes it.
// - Not allowed (WiFi down, or the access point is open): no connection, the glue stops listening.
// - The answers of a line are collected and sent once per line (one segment with TCP_NODELAY).
//   What the stack does not take at once is kept (OUT_BUFFER); while some is waiting no input is
//   read, so a client that does not read is slowed down by TCP; an overflow closes.
// Log lines name the client address and the reason, never command contents.
class ScpiTcpLink {
public:
    enum class Accept : uint8_t { Accepted, Busy, NotAllowed };
    enum class Close : uint8_t { Peer, Idle, Http, Local, NotAllowed, SendFailed, Overflow };

    static constexpr size_t OUT_BUFFER = 1024;

    // serial: *IDN? field 3 as for the USB session.
    ScpiTcpLink(ScpiBackend& backend, ScpiTcpSocket& socket, const char* serial = nullptr);

    // SCPI over TCP runs while WiFi is up, but not while the access point is open (W-S20).
    static bool allowedFor(bool staConnected, bool apActive, bool apOpen);
    // false closes an active connection.
    void setAllowed(bool allowed);
    bool allowed() const { return allowed_; }

    // A new connection from ip (a.b.c.d as in StaticIp). Busy / NotAllowed: the caller closes it.
    Accept offer(uint32_t ip, uint32_t nowMs);
    // Bytes from the active connection.
    void receive(const char* data, size_t len, uint32_t nowMs);
    // In loop(): sends waiting answers, closes an idle connection.
    void update(uint32_t nowMs);
    // The client closed the connection or it failed.
    void peerClosed();
    // Disconnect at the device (W-E13; button on the info card later).
    void disconnect();

    bool active() const { return active_; }
    // Read the socket only when no answer is waiting.
    bool wantsInput() const { return active_ && outLen_ == 0; }
    uint32_t clientIp() const { return active_ ? ip_ : 0; }
    ScpiSession& session() { return session_; }
    // Why the last connection was closed (tests).
    Close lastClose() const { return lastClose_; }

private:
    static void write_(const char* data, size_t len, void* ctx);
    void send_(const char* data, size_t len);
    void flush_();
    void close_(Close why);

    ScpiSession session_;
    ScpiTcpSocket& socket_;
    LineFramer framer_;
    bool allowed_ = false;
    bool active_ = false;
    bool closing_ = false; // a send failed during a line: close once the line is done
    Close closeWhy_ = Close::Peer;
    Close lastClose_ = Close::Peer;
    uint32_t ip_ = 0;
    uint32_t lastLineMs_ = 0;
    char out_[OUT_BUFFER] {};
    size_t outLen_ = 0;
};
