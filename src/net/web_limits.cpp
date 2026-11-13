// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "web_limits.h"

#include <string.h>

namespace {
const char CONTENT_LENGTH[] = "content-length:";
constexpr size_t CONTENT_LENGTH_LEN = sizeof(CONTENT_LENGTH) - 1;
constexpr uint64_t LENGTH_CAP = 1000000000ULL; // anything larger only needs to be "too large"
}

// ---------------------------------------------------------------------------
// HttpPreFilter

void HttpPreFilter::begin(uint32_t nowMs) {
    *this = HttpPreFilter {};
    startMs_ = nowMs;
}

uint16_t HttpPreFilter::status(Verdict v) {
    switch (v) {
        case Verdict::UriTooLong: return 414;
        case Verdict::HeadersTooLarge: return 431;
        case Verdict::PayloadTooLarge: return 413;
        case Verdict::Pass:
        case Verdict::TooMuchData:
        case Verdict::Timeout: break;
    }
    return 0;
}

HttpPreFilter::Verdict HttpPreFilter::check(uint32_t nowMs) {
    if (verdict_ == Verdict::Pass && nowMs - startMs_ > DEADLINE_MS) verdict_ = Verdict::Timeout;
    return verdict_;
}

void HttpPreFilter::endOfLine_() {
    lineBytes_ = 0;
    prefixLen_ = 0;
    lineValue_ = 0;
}

HttpPreFilter::Verdict HttpPreFilter::feed(const uint8_t* data, size_t len, uint32_t nowMs) {
    if (check(nowMs) != Verdict::Pass) return verdict_;
    for (size_t i = 0; i < len && verdict_ == Verdict::Pass; ++i) {
        const char c = (char)data[i];
        switch (state_) {
            case State::RequestLine:
                if (c == '\r') break;
                if (c == '\n') {
                    endOfLine_();
                    state_ = State::Headers;
                    break;
                }
                ++lineBytes_;
                if (c == ' ') ++spaces_;
                else if (spaces_ == 1) ++targetBytes_;
                if (targetBytes_ > MAX_URL || lineBytes_ > MAX_LINE) verdict_ = Verdict::UriTooLong;
                break;
            case State::Headers:
                if (++headerBytes_ > MAX_HEADERS) {
                    verdict_ = Verdict::HeadersTooLarge;
                    break;
                }
                if (c == '\r') break;
                if (c == '\n') {
                    if (lineBytes_ == 0) {
                        state_ = State::Body;
                        if (lengthOverflow_ || contentLength_ > MAX_BODY) verdict_ = Verdict::PayloadTooLarge;
                    }
                    endOfLine_();
                    break;
                }
                ++lineBytes_;
                if (prefixLen_ < CONTENT_LENGTH_LEN) {
                    prefix_[prefixLen_++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
                    // A header name that does not match stops collecting (prefixLen_ past the end).
                    if (memcmp(prefix_, CONTENT_LENGTH, prefixLen_) != 0) prefixLen_ = sizeof(prefix_);
                } else if (prefixLen_ == CONTENT_LENGTH_LEN) {
                    // Value of Content-Length: digits after optional blanks; the largest value of
                    // repeated headers counts (the guard refuses repeated or malformed ones).
                    if (c >= '0' && c <= '9') {
                        lineValue_ = lineValue_ * 10 + (uint64_t)(c - '0');
                        if (lineValue_ > LENGTH_CAP) {
                            lengthOverflow_ = true;
                            lineValue_ = LENGTH_CAP;
                        }
                        if (lineValue_ > contentLength_) contentLength_ = lineValue_;
                    }
                }
                break;
            case State::Body:
                if (++afterHead_ > MAX_AFTER_HEAD) verdict_ = Verdict::TooMuchData;
                break;
        }
    }
    return verdict_;
}

// ---------------------------------------------------------------------------
// HttpConnections

int HttpConnections::acquire(uint32_t nowMs) {
    for (uint8_t i = 0; i < MAX; ++i) {
        if (!slots_[i].used) {
            slots_[i].used = true;
            slots_[i].filter.begin(nowMs);
            return i;
        }
    }
    return -1;
}

void HttpConnections::release(int slot) {
    if (slot >= 0 && slot < MAX) slots_[slot].used = false;
}

bool HttpConnections::used(int slot) const { return slot >= 0 && slot < MAX && slots_[slot].used; }

uint8_t HttpConnections::count() const {
    uint8_t n = 0;
    for (const Slot& s : slots_) n = (uint8_t)(n + (s.used ? 1 : 0));
    return n;
}

// ---------------------------------------------------------------------------
// WsClients

bool WsClients::add(uint32_t id, uint32_t nowMs) {
    if (id == 0) return false;
    Client* free = nullptr;
    for (Client& c : clients_) {
        if (c.id == id) return true;
        if (c.id == 0 && !free) free = &c;
    }
    if (!free) return false;
    *free = Client {id, nowMs, nowMs, true};
    return true;
}

void WsClients::remove(uint32_t id) {
    for (Client& c : clients_) {
        if (id != 0 && c.id == id) c = Client {};
    }
}

void WsClients::pong(uint32_t id, uint32_t nowMs) {
    for (Client& c : clients_) {
        if (id != 0 && c.id == id) c.lastPongMs = nowMs;
    }
}

uint8_t WsClients::count() const {
    uint8_t n = 0;
    for (const Client& c : clients_) n = (uint8_t)(n + (c.id ? 1 : 0));
    return n;
}

void WsClients::markAll() {
    for (Client& c : clients_) {
        if (c.id) c.needsState = true;
    }
}

uint32_t WsClients::needingState(uint8_t index) const {
    if (index >= MAX) return 0;
    const Client& c = clients_[index];
    return c.id && c.needsState ? c.id : 0;
}

void WsClients::sent(uint32_t id) {
    for (Client& c : clients_) {
        if (id != 0 && c.id == id) c.needsState = false;
    }
}

uint32_t WsClients::duePing(uint32_t nowMs) {
    for (Client& c : clients_) {
        if (c.id && nowMs - c.lastPingMs >= PING_MS) {
            c.lastPingMs = nowMs;
            return c.id;
        }
    }
    return 0;
}

uint32_t WsClients::expired(uint32_t nowMs) {
    for (Client& c : clients_) {
        if (c.id && nowMs - c.lastPongMs > PONG_TIMEOUT_MS) {
            const uint32_t id = c.id;
            c = Client {};
            return id;
        }
    }
    return 0;
}
