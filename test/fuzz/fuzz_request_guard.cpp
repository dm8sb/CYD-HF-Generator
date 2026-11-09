// Fuzz target request_guard: RequestGuard (net/request_guard.*) as the web server calls it:
// check() for every request, the login lock, sessions, the scan rate, checkSecrets() and the
// web password rules (W-S1, W-S4, W-S5, W-S6, W-S10, W-S11, W-S13).
// Input: 6 configuration bytes, then a script (text, lines end with LF, CR before it ignored):
//   byte 0: bit 0 web password set, bit 1 loginForReads, bits 2-3 own addresses (0 STA, 1 AP,
//           2 both, 3 none), bit 4 open AP, bit 5 captive DNS, bit 6 the connection arrives
//           over the AP, bit 7 local address unknown (the guard decides by the peer's subnet)
//   bytes 1-4: start of the clock (ms, big endian); the guard gets it as 64-bit monotonic
//           time (S55), so it may pass 2^32 during the script without wrapping
//   byte 5: bits 0-5 seed of the token and the session ids, bit 6 no token configured,
//           bit 7 no device name
// A script line starting with '!' is an operation:
//   !ok [x]      correct password: loginSucceeded() (id from the seed, or 16 bytes x)
//   !bad         wrong password: loginFailed() (also while locked: a check already running)
//   !try         loginAllowed() / loginRetryAfterS() only
//   !wait <ms>   advance the clock (at most 10^8 ms per step and 2^31 ms per input)
//   !logout <c>  logout() with the Cookie header c
//   !endall      endAllSessions()
//   !scan        scanAllowed()
//   !pw <text>   WebPassword::valid()
// Any other line starts a request: "<method> <target>[ <version>]", header lines up to an empty
// line, then as many body bytes as a plain Content-Length says. Header names are matched
// case-insensitively (the server does that), the value starts after ':' and one optional blank.
// The last of repeated headers counts. X-Heap, X-Pad, X-Peer and X-Local set the free heap,
// extra header bytes, the peer and the local address; they are not counted as header bytes. In
// every line "$T" becomes the token and "$S<k>" the id of the k-th session issued.
//
// Invariants:
// - every decision equals an independent reference of the documented order (request line,
//   503 heap, 414 URL > 256 B, 431 headers > 2 KB, 421/302 Host allowlist without IPv6 and
//   captive redirect only for page GET/HEAD over the AP, 405, 415 Transfer-Encoding,
//   400/413 Content-Length, 400 broken Cookie/Authorization, 400 upgrade, 403 Origin and
//   Sec-Fetch-Site, 415/411/403 for POST, 403 WebSocket token, 401 session), with a non-empty
//   reason; the decision does not change with upper-case values or blanks around them where
//   HTTP says so (Host, Origin, Sec-Fetch-Site, Content-Type, Content-Length, Upgrade, token);
// - a session is accepted only with the cookie of a session the guard issued, still live
//   (12 h), not logged out, not replaced (at most 4, the least recently used goes);
// - Set-Cookie is exactly "cyd_sid=<32 hex>; Path=/; Max-Age=43200; HttpOnly; SameSite=Strict"
//   with the id from the random function;
// - the login lock follows a model of W-S11 (5 failures, 30 s doubling up to 960 s, forgotten
//   after 1 h without failure or after a correct login); within a lock the retry time never
//   grows and the lock does not end early;
// - scanAllowed at most every 10 s; checkSecrets 403 exactly for a secret over an open AP;
// - captiveLocation never writes past its buffer; WebPassword::valid agrees with an
//   independent check (8..64 bytes of strict UTF-8 without control characters);
//   constantTimeEqual agrees with memcmp.

#include "fuzz_common.h"

#include <string>
#include <vector>
#include <net/request_guard.h>

namespace fuzz_request_guard {

const uint32_t STA = (192u << 24) | (168u << 16) | (1u << 8) | 50u; // 192.168.1.50
const uint32_t AP = (192u << 24) | (168u << 16) | (4u << 8) | 1u;   // 192.168.4.1
const char NAME[] = "cyd-hf-1a2b";
const uint64_t MAX_ELAPSED = 1ULL << 31;
const uint64_t MAX_WAIT = 100000000ULL;

// --- small independent helpers ------------------------------------------------------------

std::string cstr(const std::string& s) { return std::string(s.c_str()); } // as a C string sees it

char lowerc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
std::string lower(const std::string& s) {
    std::string r(s);
    for (size_t i = 0; i < r.size(); ++i) r[i] = lowerc(r[i]);
    return r;
}
std::string upper(const std::string& s) {
    std::string r(s);
    for (size_t i = 0; i < r.size(); ++i) {
        if (r[i] >= 'a' && r[i] <= 'z') r[i] = (char)(r[i] - 'a' + 'A');
    }
    return r;
}
bool isOws(char c) { return c == ' ' || c == '\t'; }
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isOws(s[a])) ++a;
    while (b > a && isOws(s[b - 1])) --b;
    return s.substr(a, b - a);
}
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool allDigits(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (!isDigit(s[i])) return false;
    }
    return !s.empty();
}
bool startsWith(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }

// Canonical dotted quad: 4 parts of 1..3 digits, no leading zero, each <= 255.
bool ipv4(const std::string& s, uint32_t* out) {
    uint32_t v = 0;
    size_t part = 0, i = 0;
    while (part < 4) {
        size_t j = i;
        while (j < s.size() && isDigit(s[j])) ++j;
        const std::string d = s.substr(i, j - i);
        if (d.empty() || d.size() > 3 || (d.size() > 1 && d[0] == '0')) return false;
        const uint32_t x = (uint32_t)atoi(d.c_str());
        if (x > 255) return false;
        v = v << 8 | x;
        ++part;
        if (part < 4) {
            if (j >= s.size() || s[j] != '.') return false;
            ++j;
        }
        i = j;
    }
    if (i != s.size()) return false;
    *out = v;
    return true;
}

std::string fmtIp(uint32_t a) {
    char b[20];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", (unsigned)(a >> 24), (unsigned)(a >> 16 & 255), (unsigned)(a >> 8 & 255),
             (unsigned)(a & 255));
    return b;
}

std::string hex(const uint8_t* d, size_t n) {
    static const char H[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += H[d[i] >> 4];
        s += H[d[i] & 15];
    }
    return s;
}

bool lowerHex(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (!(isDigit(c) || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

// --- the scenario -------------------------------------------------------------------------

struct Config {
    bool password, loginForReads, apOpen, captive, sideAp, localUnknown, noToken, noName;
    uint32_t staIp, apIp;
    std::string token, name;
};

enum class HostClass { Own, Foreign, Invalid };

HostClass classifyHost(const Config& c, const std::string& s) {
    if (s.empty() || s[0] == '[') return HostClass::Invalid; // IPv6 literals are off (W-S4)
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char ch = (unsigned char)s[i];
        if (ch <= 0x20 || ch >= 0x7F) return HostClass::Invalid;
    }
    std::string host = s;
    const size_t colon = s.find(':');
    if (colon != std::string::npos) {
        if (s.find(':', colon + 1) != std::string::npos) return HostClass::Invalid;
        const std::string port = s.substr(colon + 1);
        if (!allDigits(port) || port.size() > 5) return HostClass::Invalid;
        const long p = atol(port.c_str());
        if (p < 1 || p > 65535) return HostClass::Invalid;
        host = s.substr(0, colon);
    }
    if (host.empty()) return HostClass::Invalid;
    uint32_t ip = 0;
    if (ipv4(host, &ip)) return ip != 0 && (ip == c.staIp || ip == c.apIp) ? HostClass::Own : HostClass::Foreign;
    if (host.size() > 253) return HostClass::Invalid;
    for (size_t i = 0; i < host.size(); ++i) {
        const char ch = lowerc(host[i]);
        if (!((ch >= 'a' && ch <= 'z') || isDigit(ch) || ch == '-' || ch == '.')) return HostClass::Invalid;
    }
    if (!c.name.empty()) {
        const std::string h = lower(host);
        if (h == c.name || h == c.name + ".local") return HostClass::Own;
    }
    return HostClass::Foreign;
}

bool originOk(const Config& c, const std::string& v) {
    const std::string o = trim(v);
    if (o.empty()) return true;
    if (o.size() <= 7 || lower(o.substr(0, 7)) != "http://") return false;
    return classifyHost(c, o.substr(7)) == HostClass::Own;
}

bool plainLength(const std::string& v, uint64_t* out) {
    const std::string t = trim(v);
    if (t.size() > 10 || !allDigits(t)) return false;
    uint64_t x = 0;
    for (size_t i = 0; i < t.size(); ++i) x = x * 10 + (uint64_t)(t[i] - '0');
    *out = x;
    return true;
}

bool jsonType(const std::string& v) {
    const std::string t = lower(trim(v));
    if (!startsWith(t, "application/json")) return false;
    std::string rest = t.substr(16);
    size_t i = 0;
    while (i < rest.size() && isOws(rest[i])) ++i;
    if (i == rest.size()) return true;
    if (rest[i] != ';') return false;
    ++i;
    while (i < rest.size() && isOws(rest[i])) ++i;
    rest = rest.substr(i);
    return rest == "charset=utf-8" || rest == "charset=\"utf-8\"";
}

// RFC 9110 token, then blanks, then credentials; printable, 1..1024 bytes (W-S13).
bool authorizationOk(const std::string& v) {
    if (v.empty() || v.size() > 1024) return false;
    for (size_t i = 0; i < v.size(); ++i) {
        const unsigned char ch = (unsigned char)v[i];
        if (!(ch == ' ' || ch == '\t' || (ch > 0x20 && ch < 0x7F))) return false;
    }
    size_t i = 0;
    while (i < v.size()) {
        const char ch = v[i];
        const bool t = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || isDigit(ch) ||
                       (ch != '\0' && strchr("!#$%&'*+-.^_`|~", ch) != nullptr);
        if (!t) break;
        ++i;
    }
    if (i == 0 || i > 32 || i == v.size() || v[i] != ' ') return false;
    while (i < v.size() && v[i] == ' ') ++i;
    return i < v.size();
}

enum class CookieKind { None, Id, Broken };

// The Cookie header as W-S13 describes it: printable, <= 2 KB, <= 32 pairs, our cookie at most
// once with exactly 32 lower-case hex digits (optionally quoted); other cookies are ignored.
CookieKind parseCookie(const std::string& h, std::string* id) {
    if (h.size() > RequestGuard::MAX_HEADERS) return CookieKind::Broken;
    for (size_t i = 0; i < h.size(); ++i) {
        const unsigned char ch = (unsigned char)h[i];
        if (!(ch == ' ' || ch == '\t' || (ch > 0x20 && ch < 0x7F))) return CookieKind::Broken;
    }
    bool found = false;
    unsigned pairs = 0;
    size_t start = 0;
    while (start <= h.size()) {
        size_t end = h.find(';', start);
        if (end == std::string::npos) end = h.size();
        const std::string seg = trim(h.substr(start, end - start));
        start = end + 1;
        if (seg.empty()) continue;
        if (++pairs > 32) return CookieKind::Broken;
        const size_t eq = seg.find('=');
        if (eq == std::string::npos || seg.substr(0, eq) != RequestGuard::COOKIE_NAME) continue;
        if (found) return CookieKind::Broken;
        std::string v = seg.substr(eq + 1);
        if (v.size() >= 2 && v[0] == '"' && v[v.size() - 1] == '"') v = v.substr(1, v.size() - 2);
        if (v.size() != RequestGuard::SESSION_ID_LEN || !lowerHex(v)) return CookieKind::Broken;
        *id = v;
        found = true;
    }
    return found ? CookieKind::Id : CookieKind::None;
}

// Sessions and login lock as W-S11 and W-S13 describe them, in elapsed time (no wrap).
struct Slot {
    bool used;
    std::string id;
    uint64_t created, lastUse;
};

struct Model {
    Slot slots[RequestGuard::MAX_SESSIONS];
    unsigned failures = 0, level = 0;
    bool locked = false;
    uint64_t lockStart = 0, lockMs = 0, lastFailure = 0;
    bool scanned = false;
    uint64_t lastScan = 0;

    Model() {
        for (Slot& s : slots) s = Slot {false, "", 0, 0};
    }
    bool live(const Slot& s, uint64_t now) const { return s.used && now - s.created < RequestGuard::SESSION_MS; }
    int find(const std::string& id, uint64_t now) const {
        int hit = -1;
        for (int i = 0; i < RequestGuard::MAX_SESSIONS; ++i) {
            if (live(slots[i], now) && slots[i].id == id) hit = i; // the guard keeps the last match
        }
        return hit;
    }
    unsigned count(uint64_t now) const {
        unsigned n = 0;
        for (const Slot& s : slots) n += live(s, now) ? 1 : 0;
        return n;
    }
    bool lockActive(uint64_t now) const { return locked && now - lockStart < lockMs; }
    uint32_t retryS(uint64_t now) const {
        if (!lockActive(now)) return 0;
        return (uint32_t)((lockMs - (now - lockStart) + 999) / 1000);
    }
    void failed(uint64_t now) {
        if ((failures > 0 || level > 0) && now - lastFailure >= RequestGuard::LOGIN_FORGET_MS) {
            failures = 0;
            level = 0;
        }
        lastFailure = now;
        if (lockActive(now)) return; // a check that was running during the lock does not count
        if (++failures < RequestGuard::LOGIN_FAILURES) return;
        failures = 0;
        locked = true;
        lockStart = now;
        lockMs = (uint64_t)RequestGuard::LOGIN_LOCK_MS << level;
        if (level < RequestGuard::LOGIN_MAX_DOUBLINGS) ++level;
    }
    void succeeded(uint64_t now, const std::string& id) {
        failures = 0;
        level = 0;
        locked = false;
        int slot = -1;
        for (int i = 0; i < RequestGuard::MAX_SESSIONS && slot < 0; ++i) {
            if (!live(slots[i], now)) slot = i;
        }
        if (slot < 0) { // all live: the least recently used one goes
            slot = 0;
            for (int i = 1; i < RequestGuard::MAX_SESSIONS; ++i) {
                if (now - slots[i].lastUse > now - slots[slot].lastUse) slot = i;
            }
        }
        slots[slot] = Slot {true, id, now, now};
    }
    void logout(const std::string& id) {
        for (Slot& s : slots) {
            if (s.used && s.id == id) s = Slot {false, "", 0, 0};
        }
    }
    void endAll() {
        for (Slot& s : slots) s = Slot {false, "", 0, 0};
    }
};

// Random function for the guard: returns the prepared words in turn.
uint32_t g_words[4];
unsigned g_next = 0;
uint32_t words() { return g_words[g_next++ & 3]; }

struct Rng {
    uint32_t s;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

// One request as the server would hand it over (C strings; absent headers are null).
struct Req {
    std::string method, path, query;
    bool hasQuery = false;
    std::string h[11];
    bool has[11] = {};
    size_t headerBytes = 0;
    uint32_t clientIp = 0, localIp = 0, freeHeap = 60000;
    std::string body;
};
enum H { HOST, ORIGIN, SFS, CTYPE, CLEN, TE, TOKEN, COOKIE, AUTH, UPGRADE, PSEUDO };
const char* const HEADER_NAMES[] = {"host",         "origin",      "sec-fetch-site", "content-type",
                                    "content-length", "transfer-encoding", "x-cyd-token", "cookie",
                                    "authorization", "upgrade"};

GuardRequest toGuard(const Req& q) {
    GuardRequest r;
    r.method = q.method.c_str();
    r.path = q.path.c_str();
    r.query = q.hasQuery ? q.query.c_str() : nullptr;
    const char** fields[] = {&r.host,          &r.origin, &r.secFetchSite, &r.contentType, &r.contentLength,
                             &r.transferEncoding, &r.token, &r.cookie,     &r.authorization, &r.upgrade};
    for (int i = 0; i < PSEUDO; ++i) *fields[i] = q.has[i] ? q.h[i].c_str() : nullptr;
    r.headerBytes = q.headerBytes;
    r.clientIp = q.clientIp;
    r.localIp = q.localIp;
    r.freeHeap = q.freeHeap;
    return r;
}

bool overAp(const Config& c, const Req& q) {
    if (c.apIp == 0) return false;
    if (q.localIp != 0) return q.localIp == c.apIp;
    return q.clientIp != 0 && (q.clientIp & 0xFFFFFF00u) == (c.apIp & 0xFFFFFF00u);
}

// The documented decision (request_guard.h, ARBEITSPLAN 3.7), written independently.
uint16_t expected(const Config& c, const Model& m, const Req& q, uint64_t now, int* sessionSlot) {
    *sessionSlot = -1;
    if (q.path.empty() || q.path[0] != '/') return 400;
    if (q.freeHeap < RequestGuard::MIN_FREE_HEAP) return 503;
    const size_t url = q.path.size() + (q.hasQuery && !q.query.empty() ? 1 + q.query.size() : 0);
    if (url > RequestGuard::MAX_URL) return 414;
    if (q.headerBytes > RequestGuard::MAX_HEADERS) return 431;
    const bool api = startsWith(q.path, "/api/");
    const bool ws = q.path == "/ws";
    const bool get = q.method == "GET" || q.method == "HEAD";
    const bool post = q.method == "POST";
    const HostClass host = classifyHost(c, trim(q.has[HOST] ? q.h[HOST] : std::string()));
    if (host != HostClass::Own) {
        const bool page = get && !api && !ws && !q.has[UPGRADE];
        return c.captive && overAp(c, q) && host == HostClass::Foreign && page ? 302 : 421;
    }
    if (!get && !post) return 405; // also OPTIONS: no CORS
    if (post && !api) return 405;
    if (q.has[TE]) return 415;
    if (q.has[CLEN]) {
        uint64_t len = 0;
        if (!plainLength(q.h[CLEN], &len)) return 400;
        if (len > RequestGuard::MAX_BODY) return 413;
    }
    std::string id;
    const CookieKind cookie = q.has[COOKIE] ? parseCookie(q.h[COOKIE], &id) : CookieKind::None;
    if (cookie == CookieKind::Broken) return 400;
    if (q.has[AUTH] && !authorizationOk(q.h[AUTH])) return 400;
    if (q.has[UPGRADE] && !(ws && get && lower(trim(q.h[UPGRADE])) == "websocket")) return 400;
    if (ws && !q.has[UPGRADE]) return 400;
    if (api || ws) {
        if (q.has[ORIGIN] && !originOk(c, q.h[ORIGIN])) return 403;
        const std::string site = lower(trim(q.has[SFS] ? q.h[SFS] : std::string()));
        if (q.has[SFS] && site == "cross-site") return 403;
        if (q.path == "/api/token" && q.has[SFS] && site != "same-origin" && site != "none") return 403;
    }
    if (post) {
        if (!q.has[CTYPE] || !jsonType(q.h[CTYPE])) return 415;
        if (!q.has[CLEN]) return 411;
        if (c.token.empty() || !q.has[TOKEN] || trim(q.h[TOKEN]) != c.token) return 403;
    }
    if (ws) {
        int count = 0;
        std::string t;
        const std::string query = q.hasQuery ? q.query : std::string();
        size_t start = 0;
        while (start < query.size()) {
            size_t end = query.find('&', start);
            if (end == std::string::npos) end = query.size();
            const std::string p = query.substr(start, end - start);
            if (startsWith(p, "t=")) {
                t = p.substr(2);
                ++count;
            }
            start = end + 1;
        }
        if (count != 1 || c.token.empty() || t != c.token) return 403;
    }
    const bool login = post && q.path == "/api/login";
    const bool wifi = q.path == "/api/wifi" || startsWith(q.path, "/api/wifi/");
    const bool needed = post || wifi || (c.loginForReads && (api || ws));
    if (c.password && needed && !login) {
        const int s = cookie == CookieKind::Id ? m.find(id, now) : -1;
        if (s < 0) return 401;
        *sessionSlot = s;
    }
    return 200;
}

bool known(uint16_t s) {
    static const uint16_t K[] = {200, 302, 400, 401, 403, 405, 411, 413, 414, 415, 421, 431, 503};
    for (uint16_t k : K) {
        if (k == s) return true;
    }
    return false;
}

class Runner {
public:
    Runner(const uint8_t* d) {
        c_.password = d[0] & 1;
        c_.loginForReads = d[0] & 2;
        const unsigned net = (d[0] >> 2) & 3;
        c_.staIp = (net == 0 || net == 2) ? STA : 0;
        c_.apIp = (net == 1 || net == 2) ? AP : 0;
        c_.apOpen = d[0] & 0x10;
        c_.captive = d[0] & 0x20;
        c_.sideAp = d[0] & 0x40;
        c_.localUnknown = d[0] & 0x80;
        t0_ = (uint32_t)d[1] << 24 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 8 | d[4];
        rng_.s = 0x9E3779B9u ^ (uint32_t)(d[5] & 63) * 0x01000193u;
        c_.noToken = d[5] & 0x40;
        c_.noName = d[5] & 0x80;

        // Per-boot token from makeToken(): 32 lower-case hex digits.
        for (uint32_t& w : g_words) w = rng_.next();
        g_next = 0;
        char token[RequestGuard::TOKEN_LEN + 1];
        RequestGuard::makeToken(words, token);
        FUZZ_CHECK(strlen(token) == RequestGuard::TOKEN_LEN && lowerHex(token));
        uint8_t b[16];
        for (int i = 0; i < 16; ++i) b[i] = (uint8_t)(g_words[i / 4] >> (8 * (i % 4)));
        FUZZ_CHECK(hex(b, 16) == token);
        c_.token = c_.noToken ? "" : token;
        c_.name = c_.noName ? "" : NAME;

        GuardConfig gc;
        gc.staIp = c_.staIp;
        gc.apIp = c_.apIp;
        gc.apOpen = c_.apOpen;
        gc.captive = c_.captive;
        // The guard copies both: the buffers given here are overwritten right after configure().
        char name[sizeof(NAME)];
        char tok[sizeof(token)];
        snprintf(name, sizeof(name), "%s", NAME);
        snprintf(tok, sizeof(tok), "%s", token);
        gc.deviceName = c_.noName ? nullptr : name;
        gc.token = c_.noToken ? nullptr : tok;
        gc.passwordSet = c_.password;
        gc.loginForReads = c_.loginForReads;
        g_.configure(gc);
        memset(name, 'x', sizeof(name) - 1);
        memset(tok, 'x', sizeof(tok) - 1);
        checkState();
    }

    void run(const uint8_t* d, size_t n) {
        size_t i = 0;
        while (i < n) {
            std::string line = nextLine(d, n, &i);
            if (line.empty()) continue;
            if (line[0] == '!') {
                op(line);
            } else {
                request(line, d, n, &i);
            }
            checkState();
        }
    }

private:
    uint64_t nowRaw() const { return (uint64_t)t0_ + elapsed_; }

    static std::string nextLine(const uint8_t* d, size_t n, size_t* i) {
        const size_t start = *i;
        while (*i < n && d[*i] != '\n') ++*i;
        std::string line(reinterpret_cast<const char*>(d + start), *i - start);
        if (*i < n) ++*i; // LF
        if (!line.empty() && line[line.size() - 1] == '\r') line.resize(line.size() - 1);
        return line;
    }

    std::string expand(const std::string& in) const {
        std::string out;
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '$' && i + 1 < in.size() && in[i + 1] == 'T') {
                out += c_.token;
                ++i;
            } else if (in[i] == '$' && i + 2 < in.size() && in[i + 1] == 'S' && isDigit(in[i + 2])) {
                const size_t k = (size_t)(in[i + 2] - '0');
                if (k < issued_.size()) out += issued_[k];
                i += 2;
            } else {
                out += in[i];
            }
        }
        return out;
    }

    void checkState() {
        const uint64_t now = nowRaw();
        const GuardDecision l = g_.loginAllowed(now);
        const uint32_t retry = g_.loginRetryAfterS(now);
        FUZZ_CHECK(retry == m_.retryS(elapsed_));
        FUZZ_CHECK((l.status() == 429) == (retry > 0));
        FUZZ_CHECK(l.status() == 200 || l.status() == 429);
        FUZZ_CHECK(l.reason && l.reason[0]);
        FUZZ_CHECK(retry <= (RequestGuard::LOGIN_LOCK_MS << RequestGuard::LOGIN_MAX_DOUBLINGS) / 1000);
        // Within a lock the retry time never grows and the lock does not end early.
        if (lastRetry_ > 0 && !succeededSince_) {
            const uint64_t d = elapsed_ - lastRetryAt_;
            if (d < (uint64_t)(lastRetry_ - 1) * 1000) FUZZ_CHECK(retry > 0 && retry <= lastRetry_);
        }
        lastRetry_ = retry;
        lastRetryAt_ = elapsed_;
        succeededSince_ = false;
        FUZZ_CHECK(g_.sessionCount(now) == m_.count(elapsed_));
        FUZZ_CHECK(g_.sessionCount(now) <= RequestGuard::MAX_SESSIONS);
    }

    void op(const std::string& raw) {
        const std::string line = expand(raw);
        const uint64_t now = nowRaw();
        if (startsWith(line, "!ok")) {
            if (line.size() > 4) {
                const uint32_t b = (uint8_t)line[4];
                for (uint32_t& w : g_words) w = b * 0x01010101u;
            } else {
                for (uint32_t& w : g_words) w = rng_.next();
            }
            uint8_t idb[16];
            for (int i = 0; i < 16; ++i) idb[i] = (uint8_t)(g_words[i / 4] >> (8 * (i % 4)));
            const std::string id = hex(idb, 16);
            char setCookie[RequestGuard::SET_COOKIE_SIZE + 8];
            memset(setCookie, '#', sizeof(setCookie));
            // Refused without a random function or with a short buffer, nothing changes.
            FUZZ_CHECK(!g_.loginSucceeded(now, nullptr, setCookie, RequestGuard::SET_COOKIE_SIZE));
            FUZZ_CHECK(!g_.loginSucceeded(now, words, setCookie, RequestGuard::SET_COOKIE_SIZE - 1));
            FUZZ_CHECK(!g_.loginSucceeded(now, words, nullptr, RequestGuard::SET_COOKIE_SIZE));
            FUZZ_CHECK(g_.sessionCount(now) == m_.count(elapsed_));
            g_next = 0;
            FUZZ_CHECK(g_.loginSucceeded(now, words, setCookie, RequestGuard::SET_COOKIE_SIZE));
            for (size_t k = RequestGuard::SET_COOKIE_SIZE; k < sizeof(setCookie); ++k) FUZZ_CHECK(setCookie[k] == '#');
            const std::string want =
                std::string(RequestGuard::COOKIE_NAME) + "=" + id + "; Path=/; Max-Age=43200; HttpOnly; SameSite=Strict";
            FUZZ_CHECK(want == setCookie);
            m_.succeeded(elapsed_, id);
            succeededSince_ = true;
            issued_.push_back(id);
            FUZZ_CHECK(g_.sessionValid(("a=b; " + std::string(RequestGuard::COOKIE_NAME) + "=" + id).c_str(), now));
        } else if (line == "!bad") {
            g_.loginFailed(now);
            m_.failed(elapsed_);
        } else if (line == "!try") {
            FUZZ_CHECK((g_.loginAllowed(now).status() == 429) == m_.lockActive(elapsed_));
        } else if (startsWith(line, "!wait ")) {
            const std::string n = line.substr(6, 9);
            uint64_t ms = allDigits(n) ? (uint64_t)atol(n.c_str()) : 0;
            if (ms > MAX_WAIT) ms = MAX_WAIT;
            if (elapsed_ + ms > MAX_ELAPSED) ms = MAX_ELAPSED - elapsed_;
            elapsed_ += ms;
        } else if (startsWith(line, "!logout ")) {
            const std::string h = cstr(line.substr(8));
            g_.logout(h.c_str());
            std::string id;
            if (parseCookie(h, &id) == CookieKind::Id) m_.logout(id);
        } else if (line == "!endall") {
            g_.endAllSessions();
            m_.endAll();
        } else if (line == "!scan") {
            const GuardDecision s = g_.scanAllowed(now);
            const bool refuse = m_.scanned && elapsed_ - m_.lastScan < RequestGuard::SCAN_INTERVAL_MS;
            FUZZ_CHECK(s.status() == (refuse ? 429 : 200));
            if (!refuse) {
                m_.scanned = true;
                m_.lastScan = elapsed_;
            }
        } else if (startsWith(line, "!pw ")) {
            const std::string pw = raw.substr(4); // raw bytes, NUL included
            const bool ok = pw.size() >= WebPassword::MIN_LEN && pw.size() <= WebPassword::MAX_LEN &&
                            fuzzValidUtf8(pw.data(), pw.size(), true);
            FUZZ_CHECK(WebPassword::valid(pw.data(), pw.size()) == ok);
        }
    }

    void request(const std::string& first, const uint8_t* d, size_t n, size_t* i) {
        Req q;
        const std::string line = cstr(expand(first));
        const size_t sp = line.find(' ');
        q.method = line.substr(0, sp);
        std::string target;
        if (sp != std::string::npos) {
            const size_t sp2 = line.find(' ', sp + 1);
            target = line.substr(sp + 1, sp2 == std::string::npos ? std::string::npos : sp2 - sp - 1);
        }
        const size_t qm = target.find('?');
        q.path = target.substr(0, qm);
        if (qm != std::string::npos) {
            q.hasQuery = true;
            q.query = target.substr(qm + 1);
        }
        q.clientIp = c_.sideAp ? AP + 1 : STA + 7;
        q.localIp = c_.localUnknown ? 0 : (c_.sideAp ? AP : STA);
        size_t pad = 0;
        for (;;) {
            if (*i >= n) break;
            const std::string raw = nextLine(d, n, i);
            if (raw.empty()) break;
            const std::string h = expand(raw);
            const size_t colon = h.find(':');
            if (colon == std::string::npos) {
                q.headerBytes += h.size() + 2;
                continue;
            }
            const std::string name = lower(h.substr(0, colon));
            std::string value = h.substr(colon + 1);
            if (!value.empty() && value[0] == ' ') value = value.substr(1);
            value = cstr(value);
            if (name == "x-heap") {
                const std::string v = value.substr(0, 9);
                q.freeHeap = allDigits(v) ? (uint32_t)atol(v.c_str()) : 0;
                continue;
            }
            if (name == "x-pad") {
                const std::string v = value.substr(0, 5);
                pad = allDigits(v) ? (size_t)atol(v.c_str()) : 0;
                continue;
            }
            if (name == "x-peer" || name == "x-local") {
                uint32_t ip = 0;
                if (!ipv4(value, &ip)) ip = 0;
                (name == "x-peer" ? q.clientIp : q.localIp) = ip;
                continue;
            }
            q.headerBytes += h.size() + 2;
            for (int k = 0; k < PSEUDO; ++k) {
                if (name == HEADER_NAMES[k]) {
                    q.h[k] = value;
                    q.has[k] = true;
                }
            }
        }
        q.headerBytes += pad;
        uint64_t len = 0;
        if (q.has[CLEN] && plainLength(q.h[CLEN], &len)) {
            const size_t take = (size_t)(len < (uint64_t)(n - *i) ? len : (uint64_t)(n - *i));
            q.body.assign(reinterpret_cast<const char*>(d + *i), take);
            *i += take;
        }
        decide(q);
    }

    void decide(const Req& q) {
        const uint64_t now = nowRaw();
        const GuardRequest r = toGuard(q);
        // Variants that must give the same decision: case of the values HTTP compares
        // case-insensitively, blanks around values the guard trims. On copies of the guard
        // (check() notes the session use).
        Req upperQ = q;
        Req blankQ = q;
        const int caseless[] = {HOST, ORIGIN, SFS, CTYPE, UPGRADE};
        for (int k : caseless) upperQ.h[k] = upper(q.h[k]);
        const int trimmed[] = {HOST, ORIGIN, SFS, CTYPE, CLEN, TOKEN, UPGRADE};
        for (int k : trimmed) blankQ.h[k] = " \t" + q.h[k] + "\t ";
        RequestGuard g1 = g_;
        RequestGuard g2 = g_;

        int slot = -1;
        const uint16_t want = expected(c_, m_, q, elapsed_, &slot);
        FUZZ_CHECK(g_.overAp(r) == overAp(c_, q));
        const GuardDecision dec = g_.check(r, now);
        FUZZ_CHECK(known(dec.status()));
        FUZZ_CHECK(dec.reason && dec.reason[0]);
        FUZZ_CHECK(dec.allowed() == (dec.status() == 200));
        FUZZ_CHECK(dec.status() == want);
        if (slot >= 0) m_.slots[slot].lastUse = elapsed_;
        FUZZ_CHECK(g1.check(toGuard(upperQ), now).status() == want);
        FUZZ_CHECK(g2.check(toGuard(blankQ), now).status() == want);

        // A given token of the right length: constant-time compare agrees with memcmp.
        if (q.has[TOKEN] && q.h[TOKEN].size() == c_.token.size()) {
            const bool same = memcmp(q.h[TOKEN].data(), c_.token.data(), c_.token.size()) == 0;
            FUZZ_CHECK(WebPassword::constantTimeEqual(q.h[TOKEN].data(), c_.token.data(), c_.token.size()) == same);
        }

        if (dec.status() == 302) {
            char loc[32];
            memset(loc, '#', sizeof(loc));
            FUZZ_CHECK(g_.captiveLocation(loc, sizeof(loc)));
            FUZZ_CHECK("http://" + fmtIp(c_.apIp) + "/" == loc);
            char small[12];
            memset(small, '#', sizeof(small));
            FUZZ_CHECK(!g_.captiveLocation(small, 8));
            FUZZ_CHECK(memchr(small, '\0', 8) != nullptr && small[8] == '#');
        }
        // After the body: W-S1, no password fields over an open access point.
        if (dec.allowed() && q.method == "POST") {
            const std::string b = lower(q.body);
            const bool secret = b.find("pass") != std::string::npos || b.find("psk") != std::string::npos ||
                                b.find("secret") != std::string::npos || b.find("key") != std::string::npos;
            const GuardDecision s = g_.checkSecrets(r, secret);
            FUZZ_CHECK(s.status() == (secret && c_.apOpen && overAp(c_, q) ? 403 : 200));
            FUZZ_CHECK(s.reason && s.reason[0]);
        }
    }

    Config c_;
    RequestGuard g_;
    Model m_;
    Rng rng_ {1};
    uint32_t t0_ = 0;
    uint64_t elapsed_ = 0;
    std::vector<std::string> issued_;
    uint32_t lastRetry_ = 0;
    uint64_t lastRetryAt_ = 0;
    bool succeededSince_ = false;
};

int run(const uint8_t* data, size_t size) {
    if (size < 6) return 0;
    Runner r(data);
    r.run(data + 6, size - 6);
    return 0;
}

} // namespace fuzz_request_guard

FUZZ_TARGET(request_guard, fuzz_request_guard::run)
