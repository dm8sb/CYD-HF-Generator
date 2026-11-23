// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target prefilter: HttpPreFilter (net/web_limits.*), the byte filter in front of the
// HTTP parser (S55, W-S10), as the web server feeds it from the async_tcp task: the raw bytes of
// one connection in chunks of any size with a clock that only moves forward (and may wrap).
// Input: bytes 0-3 start of the clock (ms, big endian, so the deadline is crossed across the
// 32-bit wrap too), byte 4 seed of the chunk sizes, byte 5 bits 0-1 the time step per chunk
// (0 none, 1 up to 15 ms, 2 up to 300 ms, 3 up to 4 s), bit 2 single bytes; the rest is the
// connection's byte stream.
//
// Invariants:
// - the verdict after every chunk equals an independent reference that reads the whole stream
//   at once (request line up to the first LF without CR: 414 when the target between the first
//   and the second blank exceeds 256 bytes or the line 288; header block: every byte counts,
//   431 at byte 2049; at the empty line 413 when a Content-Length line carries a value above
//   1024, its digits read as one decimal number of any length; after the head: close at byte
//   2049) and decides at the same byte, plus the deadline: a chunk that arrives more than 10 s
//   after begin() gives Timeout (unsigned 32-bit difference);
// - once not Pass, every feed() and check() returns the same verdict; headComplete() exactly
//   when the reference reached the empty line with the bytes consumed; status() 414/431/413 or 0;
// - the same stream in one chunk after begin() decides the same (chunking changes nothing);
// - differential against RequestGuard::check for a complete request line "<method> <target>
//   <version>": when the guard refuses the URL (414) the pre-filter did too, and a
//   Content-Length line the guard refuses with 413 (plain decimal above 1 KB) means the
//   pre-filter answered 413 (or stopped earlier); a head the pre-filter passed never gets 414
//   or 413 from the guard.

#include "fuzz_common.h"

#include <string>
#include <vector>
#include <net/request_guard.h>
#include <net/web_limits.h>

namespace fuzz_prefilter {

typedef HttpPreFilter::Verdict V;

const size_t MAX_TARGET = 256;
const size_t MAX_LINE = 288;
const size_t MAX_HEADER_BYTES = 2048;
const size_t MAX_BODY = 1024;
const size_t MAX_AFTER = 2048;
const uint32_t DEADLINE = 10000;

// --- independent reference ----------------------------------------------------------------

struct Ref {
    bool decided = false;
    V verdict = V::Pass;
    size_t at = 0;         // byte index of the decision
    bool head = false;     // the empty line was reached
    size_t headAt = 0;     // its LF
    bool requestLine = false;
    std::string line;      // the request line without CR
    std::vector<std::string> lengths; // values of the Content-Length lines (after the colon)
    size_t headerBytes = 0;
};

char lowerc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

// Digits of the value as one decimal number: above MAX_BODY?
bool digitsAbove(const std::string& v, size_t limit) {
    std::string d;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] >= '0' && v[i] <= '9' && !(d.empty() && v[i] == '0')) d += v[i];
    }
    if (d.size() > 9) return true;
    return d.size() && (size_t)atol(d.c_str()) > limit;
}

void decide(Ref& r, V v, size_t at) {
    r.decided = true;
    r.verdict = v;
    r.at = at;
}

Ref reference(const uint8_t* d, size_t n) {
    Ref r;
    size_t i = 0;
    // Request line: count everything but CR; the target is the run after the first blank.
    size_t lineBytes = 0, target = 0, blanks = 0;
    for (; i < n; ++i) {
        const char c = (char)d[i];
        if (c == '\n') break;
        if (c == '\r') continue;
        r.line += c;
        ++lineBytes;
        if (c == ' ') ++blanks;
        else if (blanks == 1) ++target;
        if (target > MAX_TARGET || lineBytes > MAX_LINE) {
            decide(r, V::UriTooLong, i);
            return r;
        }
    }
    if (i >= n) return r;
    r.requestLine = true;
    ++i;
    // Header lines up to the empty one; every byte counts.
    std::string cur;
    bool tooLarge = false;
    for (; i < n; ++i) {
        const char c = (char)d[i];
        if (++r.headerBytes > MAX_HEADER_BYTES) {
            decide(r, V::HeadersTooLarge, i);
            return r;
        }
        if (c == '\r') continue;
        if (c != '\n') {
            cur += c;
            continue;
        }
        if (cur.empty()) {
            r.head = true;
            r.headAt = i;
            if (tooLarge) {
                decide(r, V::PayloadTooLarge, i);
                return r;
            }
            break;
        }
        std::string name;
        for (size_t k = 0; k < cur.size() && k < 15; ++k) name += lowerc(cur[k]);
        if (name == "content-length:") {
            const std::string value = cur.substr(15);
            r.lengths.push_back(value);
            if (digitsAbove(value, MAX_BODY)) tooLarge = true;
        }
        cur.clear();
    }
    if (!r.head) return r;
    // After the head: at most MAX_AFTER bytes.
    size_t after = 0;
    for (++i; i < n; ++i) {
        if (++after > MAX_AFTER) {
            decide(r, V::TooMuchData, i);
            return r;
        }
    }
    return r;
}

// --- chunks and time ------------------------------------------------------------------------

struct Rng {
    uint32_t s;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

uint16_t expectedStatus(V v) {
    switch (v) {
        case V::UriTooLong: return 414;
        case V::HeadersTooLarge: return 431;
        case V::PayloadTooLarge: return 413;
        default: return 0;
    }
}

// --- differential against RequestGuard ------------------------------------------------------

const uint32_t OWN = (192u << 24) | (168u << 16) | (1u << 8) | 50u;

uint16_t guardStatus(const std::string& method, const std::string& path, const std::string& query,
                     const char* contentLength) {
    GuardConfig cfg;
    cfg.staIp = OWN;
    cfg.deviceName = "cyd-hf-1a2b";
    cfg.token = "0123456789abcdef0123456789abcdef";
    RequestGuard g;
    g.configure(cfg);
    GuardRequest q;
    q.method = method.c_str();
    q.path = path.c_str();
    q.query = query.c_str();
    q.host = "192.168.1.50";
    q.contentLength = contentLength;
    q.contentType = "application/json";
    q.headerBytes = 0;
    q.clientIp = OWN + 1;
    q.localIp = OWN;
    q.freeHeap = 100000;
    return g.check(q, 0).status();
}

void differential(const Ref& ref, V final) {
    if (!ref.requestLine) return;
    // "<method> <target> <version>" with a target that starts with '/'.
    const size_t a = ref.line.find(' ');
    if (a == std::string::npos) return;
    const size_t b = ref.line.find(' ', a + 1);
    if (b == std::string::npos || ref.line.find(' ', b + 1) != std::string::npos) return;
    const std::string method = ref.line.substr(0, a);
    const std::string target = ref.line.substr(a + 1, b - a - 1);
    if (method.empty() || target.empty() || target[0] != '/') return;
    const size_t qm = target.find('?');
    const std::string path = target.substr(0, qm);
    const std::string query = qm == std::string::npos ? std::string() : target.substr(qm + 1);
    // POST to the API, so 405 does not hide 413.
    const uint16_t url = guardStatus("POST", path, query, nullptr);
    if (url == 414) FUZZ_CHECK(final == V::UriTooLong);
    if (final == V::Pass && ref.head) FUZZ_CHECK(url != 414);
    if (url == 414 || !ref.head) return;
    for (size_t k = 0; k < ref.lengths.size(); ++k) {
        const std::string value = std::string(ref.lengths[k].c_str()); // as a C string sees it
        const uint16_t s = guardStatus("POST", "/api/config", "", value.c_str());
        if (s == 413) FUZZ_CHECK(final == V::PayloadTooLarge);
        if (final == V::Pass) FUZZ_CHECK(s != 413);
    }
}

// --- the harness ----------------------------------------------------------------------------

int run(const uint8_t* data, size_t size) {
    if (size < 6) return 0;
    const uint32_t start = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3];
    Rng rng {0x9E3779B9u ^ ((uint32_t)data[4] * 0x01000193u) ^ (uint32_t)size};
    const uint8_t mode = data[5];
    const uint8_t* s = data + 6;
    const size_t n = size - 6;
    static const uint32_t STEP[4] = {0, 16, 301, 4001};
    const uint32_t step = STEP[mode & 3];
    const bool single = (mode & 4) != 0;

    const Ref ref = reference(s, n);

    HttpPreFilter f;
    f.begin(start);
    FUZZ_CHECK(f.check(start) == V::Pass);
    FUZZ_CHECK(!f.headComplete());
    uint32_t now = start;
    size_t pos = 0;
    bool decided = false;
    V verdict = V::Pass;
    size_t consumed = 0; // bytes the filter has looked at
    while (pos < n) {
        size_t len = single ? 1 : 1 + rng.next() % 64;
        if (!single && rng.next() % 8 == 0) len = 1 + rng.next() % 700;
        if (len > n - pos) len = n - pos;
        if (step) now += rng.next() % step;
        const V got = f.feed(s + pos, len, now);
        if (!decided) {
            if ((uint32_t)(now - start) > DEADLINE) {
                decided = true;
                verdict = V::Timeout;
            } else if (ref.decided && ref.at < pos + len) {
                decided = true;
                verdict = ref.verdict;
                consumed = ref.at + 1;
            } else {
                consumed = pos + len;
            }
        }
        FUZZ_CHECK(got == verdict);
        FUZZ_CHECK(f.check(now) == verdict);
        const bool head = ref.head && ref.headAt < consumed &&
                          !(ref.decided && (ref.at < ref.headAt || (ref.at == ref.headAt && ref.verdict != V::PayloadTooLarge)));
        FUZZ_CHECK(f.headComplete() == head);
        FUZZ_CHECK(HttpPreFilter::status(got) == expectedStatus(got));
        pos += len;
    }
    // The deadline alone: nothing more arrives.
    const uint32_t later = now + DEADLINE + 1;
    if (!decided && (uint32_t)(later - start) > DEADLINE) FUZZ_CHECK(f.check(later) == V::Timeout);
    else FUZZ_CHECK(f.check(later) == verdict);
    FUZZ_CHECK(f.feed(s, n, later) == f.check(later)); // stays

    // The whole stream in one chunk, in time.
    HttpPreFilter whole;
    whole.begin(start);
    const V once = whole.feed(s, n, start + DEADLINE);
    FUZZ_CHECK(once == (ref.decided ? ref.verdict : V::Pass));
    FUZZ_CHECK(whole.headComplete() == (ref.head && !(ref.decided && (ref.at < ref.headAt ||
                                                                       (ref.at == ref.headAt && ref.verdict != V::PayloadTooLarge)))));
    whole.begin(start); // begin() starts over
    FUZZ_CHECK(whole.check(start) == V::Pass && !whole.headComplete());

    differential(ref, once);
    return 0;
}

}

FUZZ_TARGET(prefilter, fuzz_prefilter::run)
