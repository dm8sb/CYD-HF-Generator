// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "request_guard.h"

// Byte and connection limits of the web server (S55, A-706, A-710). Pure and host-tested; the
// device glue (net/web_server.*) feeds them from the async_tcp task. Not thread-safe.

// Raw bytes of one HTTP connection before ESPAsyncWebServer parses them: the library keeps
// header lines and values without any limit, so the limits are checked on the bytes as they
// arrive and an oversized request is answered (414, 431, 413) and closed before it can use
// the heap. Also the 10 s deadline for the whole request (slow senders, A-710).
class HttpPreFilter {
public:
    static constexpr size_t MAX_URL = 256;          // request target incl. query (A-710)
    static constexpr size_t MAX_LINE = MAX_URL + 32; // request line: method, target, version
    static constexpr size_t MAX_HEADERS = 2048;      // header lines incl. CRLF (A-710)
    static constexpr size_t MAX_BODY = 1024;         // Content-Length (A-710)
    // Bytes after the header block at most (chunked framing, bytes beyond Content-Length).
    static constexpr size_t MAX_AFTER_HEAD = 2 * MAX_BODY;
    static constexpr uint32_t DEADLINE_MS = 10000;   // whole request (A-710)

    enum class Verdict : uint8_t {
        Pass,
        UriTooLong,     // 414
        HeadersTooLarge, // 431
        PayloadTooLarge, // 413: Content-Length above MAX_BODY (decided at the end of the headers)
        TooMuchData,    // more bytes than any allowed request: close without an answer
        Timeout         // DEADLINE_MS passed: close without an answer
    };

    void begin(uint32_t nowMs);
    // Bytes as received; once not Pass, every further call returns the same verdict.
    Verdict feed(const uint8_t* data, size_t len, uint32_t nowMs);
    // The deadline alone (for a connection that sends nothing more).
    Verdict check(uint32_t nowMs);
    bool headComplete() const { return state_ == State::Body; }
    // HTTP status for the verdicts that get an answer, 0 for the others.
    static uint16_t status(Verdict v);

private:
    enum class State : uint8_t { RequestLine, Headers, Body };
    void endOfLine_();

    State state_ = State::RequestLine;
    Verdict verdict_ = Verdict::Pass;
    uint32_t startMs_ = 0;
    size_t lineBytes_ = 0;    // current line, without CR LF
    size_t targetBytes_ = 0;  // request target so far
    uint8_t spaces_ = 0;      // spaces in the request line so far
    size_t headerBytes_ = 0;
    size_t afterHead_ = 0;
    char prefix_[24] {};      // start of the current header line, lower case
    uint8_t prefixLen_ = 0;
    bool lengthOverflow_ = false;
    uint64_t lineValue_ = 0;     // Content-Length value of the current line
    uint64_t contentLength_ = 0; // largest value seen
};

// Live HTTP connections (<= MAX, A-710) with their pre-filter; a WebSocket leaves the table
// when it is upgraded (it counts against the WebSocket limit instead).
class HttpConnections {
public:
    static constexpr uint8_t MAX = 4;
    // A free slot index, -1 when MAX connections are open.
    int acquire(uint32_t nowMs);
    void release(int slot);
    bool used(int slot) const;
    uint8_t count() const;
    HttpPreFilter& filter(int slot) { return slots_[slot].filter; }

private:
    struct Slot {
        bool used;
        HttpPreFilter filter;
    };
    Slot slots_[MAX] {};
};

// WebSocket clients (E-619, A-706): at most MAX, push only, ping every PING_MS, closed after
// PONG_TIMEOUT_MS without an answer; needsState marks a client that has not got the current state.
// AF18: only a PONG that answers our PING counts: it must echo the PING's random payload
// (PING_PAYLOAD_LEN bytes from duePing), so unsolicited PONGs cannot keep a client that reads
// nothing (a hidden tab, a dead peer, a slot hog) in its slot, and more than PING_BUDGET PINGs from a client within PING_WINDOW_MS close it (the
// web UI never pings; the library queues a PONG for every PING without a limit).
class WsClients {
public:
    static constexpr uint8_t MAX = 3;
    static constexpr uint32_t PING_MS = 15000;
    static constexpr uint32_t PONG_TIMEOUT_MS = 30000;
    static constexpr size_t MAX_FRAME = 512; // inbound frames (A-706); the state goes out at ~800 B
    static constexpr uint8_t PING_BUDGET = 3;
    static constexpr size_t PING_PAYLOAD_LEN = 4;
    static constexpr uint32_t PING_WINDOW_MS = 10000;

    // false when MAX clients are registered (the caller closes the new one).
    bool add(uint32_t id, uint32_t nowMs);
    void remove(uint32_t id);
    // A PONG from the client: counts only while our PING waits for it and with its payload.
    void pong(uint32_t id, uint32_t nowMs, const uint8_t* data, size_t len);
    // A PING from the client; false = flood (or unknown client): the caller closes it.
    bool clientPing(uint32_t id, uint32_t nowMs);
    uint8_t count() const;
    // Every client gets the next state.
    void markAll();
    // The client in slot index (0..MAX-1) if it waits for the state, else 0; sent() once it
    // got it.
    uint32_t needingState(uint8_t index) const;
    void sent(uint32_t id);
    // A client due for a ping (0 = none); marks it pinged with nonce (random) as the payload,
    // written to payload[PING_PAYLOAD_LEN].
    uint32_t duePing(uint32_t nowMs, uint32_t nonce, uint8_t payload[PING_PAYLOAD_LEN]);
    // A client silent for longer than PONG_TIMEOUT_MS (0 = none); removes it.
    uint32_t expired(uint32_t nowMs);
    // An inbound frame of this total length is acceptable (push only: anything else is ignored).
    static bool frameAllowed(uint64_t frameLen) { return frameLen <= MAX_FRAME; }

private:
    struct Client {
        uint32_t id; // 0 = free
        uint32_t lastPongMs;
        uint32_t lastPingMs;
        bool needsState;
        bool awaitingPong;    // our PING is unanswered
        uint32_t nonce;       // its payload
        uint8_t pingsIn;      // PINGs from the client in the current window
        uint32_t pingWindowMs;
    };
    Client clients_[MAX] {};
};

// WebSocket push pacing (AF19, PERF-6): at most one push per MIN_INTERVAL_MS. The first change
// after a quiet interval goes at once; changes within the interval are merged and the state as
// it is then goes out at its end (a SCPI flood changed the state up to ~80 times per second,
// each a JSON build and a send to every client).
class PushPacer {
public:
    static constexpr uint32_t MIN_INTERVAL_MS = 100;
    // changed: the state differs from the last one pushed. True: push the current state now.
    bool due(bool changed, uint32_t nowMs) {
        if (changed) pending_ = true;
        if (!pending_) return false;
        if (sent_ && nowMs - lastMs_ < MIN_INTERVAL_MS) return false;
        pending_ = false;
        sent_ = true;
        lastMs_ = nowMs;
        return true;
    }
    void reset() { *this = PushPacer {}; }

private:
    bool pending_ = false;
    bool sent_ = false;
    uint32_t lastMs_ = 0;
};

// Heap checks before allocations of the web server (AF19, PERF-8): a fragmented heap can have
// enough free bytes but no block for the next allocation; std::make_shared then throws
// bad_alloc, which ends in terminate() and a restart with all outputs off.
namespace HeapLimits {
static constexpr uint32_t MIN_BLOCK_ACCEPT = 8 * 1024; // a new connection: request, parser, answer
static constexpr uint32_t PUSH_RESERVE = 4 * 1024;     // beyond the state text: message objects, lwIP
// A new HTTP connection is taken (else 503): A-710 free heap and the largest block.
inline bool acceptOk(uint32_t freeHeap, uint32_t largestBlock) {
    return freeHeap >= RequestGuard::MIN_FREE_HEAP && largestBlock >= MIN_BLOCK_ACCEPT;
}
// A push of stateBytes is built (else skipped, the clients keep waiting for the state).
inline bool pushOk(size_t stateBytes, uint32_t largestBlock) { return largestBlock >= stateBytes + PUSH_RESERVE; }
}
