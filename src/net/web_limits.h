#pragma once

#include <stddef.h>
#include <stdint.h>

// Byte and connection limits of the web server (S55, W-S6, W-S10). Pure and host-tested; the
// device glue (net/web_server.*) feeds them from the async_tcp task. Not thread-safe.

// Raw bytes of one HTTP connection before ESPAsyncWebServer parses them: the library keeps
// header lines and values without any limit, so the limits are checked on the bytes as they
// arrive and an oversized request is answered (414, 431, 413) and closed before it can use
// the heap. Also the 10 s deadline for the whole request (slow senders, W-S10).
class HttpPreFilter {
public:
    static constexpr size_t MAX_URL = 256;          // request target incl. query (W-S10)
    static constexpr size_t MAX_LINE = MAX_URL + 32; // request line: method, target, version
    static constexpr size_t MAX_HEADERS = 2048;      // header lines incl. CRLF (W-S10)
    static constexpr size_t MAX_BODY = 1024;         // Content-Length (W-S10)
    // Bytes after the header block at most (chunked framing, bytes beyond Content-Length).
    static constexpr size_t MAX_AFTER_HEAD = 2 * MAX_BODY;
    static constexpr uint32_t DEADLINE_MS = 10000;   // whole request (W-S10)

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

// Live HTTP connections (<= MAX, W-S10) with their pre-filter; a WebSocket leaves the table
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

// WebSocket clients (W-E10, W-S6): at most MAX, push only, ping every PING_MS, closed after
// PONG_TIMEOUT_MS without an answer; needsState marks a client that has not got the current state.
class WsClients {
public:
    static constexpr uint8_t MAX = 3;
    static constexpr uint32_t PING_MS = 15000;
    static constexpr uint32_t PONG_TIMEOUT_MS = 30000;
    static constexpr size_t MAX_FRAME = 512; // inbound frames (W-S6); the state goes out at ~800 B

    // false when MAX clients are registered (the caller closes the new one).
    bool add(uint32_t id, uint32_t nowMs);
    void remove(uint32_t id);
    void pong(uint32_t id, uint32_t nowMs);
    uint8_t count() const;
    // Every client gets the next state.
    void markAll();
    // The client in slot index (0..MAX-1) if it waits for the state, else 0; sent() once it
    // got it.
    uint32_t needingState(uint8_t index) const;
    void sent(uint32_t id);
    // A client due for a ping (0 = none); marks it pinged.
    uint32_t duePing(uint32_t nowMs);
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
    };
    Client clients_[MAX] {};
};
