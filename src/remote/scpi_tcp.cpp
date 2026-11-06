#include "scpi_tcp.h"

#include <stdio.h>
#include <string.h>
#include <constants.h>
#include <utils/logger.h>

namespace {
const char* closeText(ScpiTcpLink::Close why) {
    switch (why) {
        case ScpiTcpLink::Close::Idle: return "closed (idle)";
        case ScpiTcpLink::Close::Http: return "closed (HTTP request)";
        case ScpiTcpLink::Close::Local: return "closed (disconnect)";
        case ScpiTcpLink::Close::NotAllowed: return "closed (WiFi)";
        case ScpiTcpLink::Close::SendFailed: return "closed (send failed)";
        case ScpiTcpLink::Close::Overflow: return "closed (not reading)";
        default: return "closed";
    }
}

void logClient(uint32_t ip, const char* what) {
    char line[64];
    snprintf(line, sizeof(line), "TCP client %u.%u.%u.%u %s", (unsigned)(ip >> 24), (unsigned)((ip >> 16) & 0xFF),
             (unsigned)((ip >> 8) & 0xFF), (unsigned)(ip & 0xFF), what);
    Logger::info("SCPI", line);
}
}

ScpiTcpLink::ScpiTcpLink(ScpiBackend& backend, ScpiTcpSocket& socket, const char* serial)
    : session_(backend, Source::Scpi, serial), socket_(socket) {
    session_.setWriter(write_, this);
}

bool ScpiTcpLink::allowedFor(bool staConnected, bool apActive, bool apOpen) {
    return (staConnected || apActive) && !(apActive && apOpen);
}

void ScpiTcpLink::setAllowed(bool allowed) {
    allowed_ = allowed;
    if (!allowed) close_(Close::NotAllowed);
}

ScpiTcpLink::Accept ScpiTcpLink::offer(uint32_t ip, uint32_t nowMs) {
    if (!allowed_) return Accept::NotAllowed;
    if (active_) {
        logClient(ip, "refused (busy)");
        return Accept::Busy;
    }
    active_ = true;
    closing_ = false;
    ip_ = ip;
    lastLineMs_ = nowMs;
    outLen_ = 0;
    framer_.reset();
    session_.clearStatus(); // nothing of the previous connection remains
    logClient(ip, "connected");
    return Accept::Accepted;
}

void ScpiTcpLink::receive(const char* data, size_t len, uint32_t nowMs) {
    for (size_t i = 0; i < len && active_; ++i) {
        const LineFramer::Event e = framer_.feed(data[i]);
        if (e == LineFramer::Event::None) continue;
        lastLineMs_ = nowMs;
        if (e == LineFramer::Event::Line) {
            // W-S18: neither this line nor anything after it is executed.
            if (looksLikeHttp(framer_.line(), framer_.length())) {
                close_(Close::Http);
                return;
            }
            session_.execute(framer_.line(), framer_.length());
            flush_(); // the answers of one line in one segment
        } else {
            session_.pushError(framerErrorCode(e));
        }
        if (closing_) {
            close_(closeWhy_);
            return;
        }
    }
}

void ScpiTcpLink::update(uint32_t nowMs) {
    if (!active_) return;
    flush_();
    if (closing_) close_(closeWhy_);
    else if (nowMs - lastLineMs_ >= SCPI_IDLE_TIMEOUT_MS) close_(Close::Idle);
}

void ScpiTcpLink::peerClosed() { close_(Close::Peer); }

void ScpiTcpLink::disconnect() { close_(Close::Local); }

void ScpiTcpLink::write_(const char* data, size_t len, void* ctx) { static_cast<ScpiTcpLink*>(ctx)->send_(data, len); }

// Collects the answers of a line (the library writes them in pieces); flushed after the line.
void ScpiTcpLink::send_(const char* data, size_t len) {
    if (!active_ || closing_) return;
    if (len > OUT_BUFFER - outLen_) flush_(); // a long compound answer: make room
    if (closing_) return;
    if (len > OUT_BUFFER - outLen_) {
        closing_ = true;
        closeWhy_ = Close::Overflow;
        return;
    }
    memcpy(out_ + outLen_, data, len);
    outLen_ += len;
}

// A failure only marks the connection; it is closed once the current line is done.
void ScpiTcpLink::flush_() {
    if (outLen_ == 0 || closing_) return;
    const int32_t n = socket_.trySend(out_, outLen_);
    if (n < 0) {
        closing_ = true;
        closeWhy_ = Close::SendFailed;
        return;
    }
    const size_t sent = (size_t)n < outLen_ ? (size_t)n : outLen_;
    if (sent == 0) return;
    memmove(out_, out_ + sent, outLen_ - sent);
    outLen_ -= sent;
}

void ScpiTcpLink::close_(Close why) {
    if (!active_) return;
    socket_.closeConnection();
    active_ = false;
    closing_ = false;
    outLen_ = 0;
    framer_.reset();
    lastClose_ = why;
    logClient(ip_, closeText(why));
}
