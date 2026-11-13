// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "request_guard.h"

#include <stdio.h>
#include <string.h>
#include <net/pbkdf2.h>
#include <utils/sanitize.h>

namespace {

const GuardDecision ALLOW {Verdict::Allow, "OK"};

GuardDecision refuse(Verdict v, const char* reason) { return GuardDecision {v, reason}; }

char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool ows(char c) { return c == ' ' || c == '\t'; }

// Case-insensitive (ASCII) comparison of s[0..n) with the NUL-terminated b.
bool ieqn(const char* s, size_t n, const char* b) {
    for (size_t i = 0; i < n; ++i) {
        if (!b[i] || lower(s[i]) != lower(b[i])) return false;
    }
    return b[n] == '\0';
}

// Header value without leading and trailing blanks (OWS).
void trim(const char* v, const char** s, size_t* n) {
    size_t len = strlen(v);
    while (len > 0 && ows(*v)) {
        ++v;
        --len;
    }
    while (len > 0 && ows(v[len - 1])) --len;
    *s = v;
    *n = len;
}

bool ieqValue(const char* v, const char* b) {
    if (!v) return false;
    const char* s;
    size_t n;
    trim(v, &s, &n);
    return ieqn(s, n, b);
}

bool startsWith(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

// RFC 9110 token character.
bool tchar(char c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (digit(c)) return true;
    return strchr("!#$%&'*+-.^_`|~", c) != nullptr && c != '\0';
}

// Visible ASCII or blank (header values we accept at all).
bool printable(char c) { return c == ' ' || c == '\t' || (c > 0x20 && c < 0x7F); }

bool parseIpv4(const char* s, size_t n, uint32_t* out) {
    uint32_t v = 0;
    size_t i = 0;
    for (int part = 0; part < 4; ++part) {
        if (i >= n || !digit(s[i])) return false;
        const size_t start = i;
        uint32_t x = 0;
        while (i < n && digit(s[i])) {
            if (i - start == 3) return false;
            x = x * 10 + (uint32_t)(s[i] - '0');
            ++i;
        }
        if (x > 255 || (i - start > 1 && s[start] == '0')) return false; // no leading zeros (octal)
        v = (v << 8) | x;
        if (part < 3) {
            if (i >= n || s[i] != '.') return false;
            ++i;
        }
    }
    if (i != n) return false;
    *out = v;
    return true;
}

bool validPort(const char* s, size_t n) {
    if (n == 0 || n > 5) return false;
    uint32_t p = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!digit(s[i])) return false;
        p = p * 10 + (uint32_t)(s[i] - '0');
    }
    return p >= 1 && p <= 65535;
}

// Host name characters (a reg-name we would ever redirect): letters, digits, '-', '.'.
bool hostChars(const char* s, size_t n) {
    if (n == 0 || n > 253) return false;
    for (size_t i = 0; i < n; ++i) {
        const char c = lower(s[i]);
        if (!((c >= 'a' && c <= 'z') || digit(c) || c == '-' || c == '.')) return false;
    }
    return true;
}

// Splits "host[:port]"; false if the port is malformed.
bool splitPort(const char* s, size_t n, size_t* hostLen) {
    const char* colon = nullptr;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == ':') {
            if (colon) return false; // more than one: IPv6 without brackets or garbage
            colon = s + i;
        }
    }
    if (!colon) {
        *hostLen = n;
        return true;
    }
    const size_t h = (size_t)(colon - s);
    if (!validPort(colon + 1, n - h - 1)) return false;
    *hostLen = h;
    return true;
}

bool isApi(const char* path) { return startsWith(path, "/api/"); }
bool isWs(const char* path) { return strcmp(path, "/ws") == 0; }

// Plain decimal number (1..10 digits) after trimming; false for signs, lists, garbage.
bool parseLength(const char* v, uint64_t* out) {
    const char* s;
    size_t n;
    trim(v, &s, &n);
    if (n == 0 || n > 10) return false;
    uint64_t x = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!digit(s[i])) return false;
        x = x * 10 + (uint64_t)(s[i] - '0');
    }
    *out = x;
    return true;
}

// application/json, optionally followed by "; charset=utf-8" (case-insensitive, blanks).
bool jsonContentType(const char* v) {
    const char* s;
    size_t n;
    trim(v, &s, &n);
    static const char TYPE[] = "application/json";
    const size_t t = sizeof(TYPE) - 1;
    if (n < t || !ieqn(s, t, TYPE)) return false;
    size_t i = t;
    while (i < n && ows(s[i])) ++i;
    if (i == n) return true;
    if (s[i++] != ';') return false;
    while (i < n && ows(s[i])) ++i;
    return ieqn(s + i, n - i, "charset=utf-8") || ieqn(s + i, n - i, "charset=\"utf-8\"");
}

// "scheme credentials": scheme a token, then blanks, then printable ASCII; at most 1024 bytes.
bool wellFormedAuthorization(const char* v) {
    const size_t n = strlen(v);
    if (n == 0 || n > 1024) return false;
    for (size_t i = 0; i < n; ++i) {
        if (!printable(v[i])) return false;
    }
    size_t i = 0;
    while (i < n && tchar(v[i])) ++i;
    if (i == 0 || i > 32 || i == n || v[i] != ' ') return false;
    while (i < n && v[i] == ' ') ++i;
    return i < n;
}

int hexValue(char c) {
    if (digit(c)) return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1; // upper case is not what we hand out
}

bool parseHex(const char* s, size_t n, uint8_t* out) {
    for (size_t i = 0; i < n / 2; ++i) {
        const int hi = hexValue(s[2 * i]);
        const int lo = hexValue(s[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)(hi << 4 | lo);
    }
    return n % 2 == 0;
}

void toHex(const uint8_t* d, size_t n, char* out) {
    static const char H[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = H[d[i] >> 4];
        out[2 * i + 1] = H[d[i] & 15];
    }
    out[2 * n] = '\0';
}

void randomBytes(RequestGuard::Random32 random, uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; i += 4) {
        const uint32_t r = random ? random() : 0;
        for (size_t k = 0; k < 4 && i + k < n; ++k) out[i + k] = (uint8_t)(r >> (8 * k));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Configuration and helpers

void RequestGuard::configure(const GuardConfig& c) {
    config_ = c;
    snprintf(name_, sizeof(name_), "%s", c.deviceName ? c.deviceName : "");
    snprintf(token_, sizeof(token_), "%s", c.token ? c.token : "");
    // Pointers into own copies: the caller's buffers may change.
    config_.deviceName = name_;
    config_.token = token_;
}

bool RequestGuard::overAp(const GuardRequest& r) const {
    if (config_.apIp == 0) return false;
    if (r.localIp != 0) return r.localIp == config_.apIp;
    return r.clientIp != 0 && (r.clientIp & 0xFFFFFF00u) == (config_.apIp & 0xFFFFFF00u);
}

bool RequestGuard::captiveLocation(char* out, size_t outSize) const {
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    if (config_.apIp == 0) return false;
    const uint32_t a = config_.apIp;
    const int w = snprintf(out, outSize, "http://%u.%u.%u.%u/", (unsigned)(a >> 24), (unsigned)((a >> 16) & 255),
                           (unsigned)((a >> 8) & 255), (unsigned)(a & 255));
    return w > 0 && (size_t)w < outSize;
}

RequestGuard::HostKind RequestGuard::classifyHost_(const char* s, size_t n) const {
    for (size_t i = 0; i < n; ++i) {
        if (!printable(s[i]) || s[i] == ' ' || s[i] == '\t') return HostKind::Invalid;
    }
    if (n == 0 || s[0] == '[') return HostKind::Invalid; // IPv6 is off (W-S4)
    size_t h = 0;
    if (!splitPort(s, n, &h) || h == 0) return HostKind::Invalid;
    uint32_t ip = 0;
    if (parseIpv4(s, h, &ip)) {
        const bool own = ip != 0 && (ip == config_.staIp || ip == config_.apIp);
        return own ? HostKind::Own : HostKind::Foreign;
    }
    if (!hostChars(s, h)) return HostKind::Invalid;
    const size_t nameLen = strlen(name_);
    if (nameLen > 0) {
        if (ieqn(s, h, name_)) return HostKind::Own;
        static const char LOCAL[] = ".local";
        const size_t l = sizeof(LOCAL) - 1;
        if (h == nameLen + l && ieqn(s, nameLen, name_) && ieqn(s + nameLen, l, LOCAL)) return HostKind::Own;
    }
    return HostKind::Foreign;
}

bool RequestGuard::originAllowed_(const char* origin) const {
    const char* s;
    size_t n;
    trim(origin, &s, &n);
    if (n == 0) return true; // no Origin: not a cross-origin browser request
    static const char SCHEME[] = "http://";
    const size_t p = sizeof(SCHEME) - 1;
    if (n <= p || !ieqn(s, p, SCHEME)) return false; // also "null" and https
    return classifyHost_(s + p, n - p) == HostKind::Own;
}

bool RequestGuard::tokenMatches_(const char* given, size_t n) const {
    const size_t expected = strlen(token_);
    if (expected == 0 || !given || n != expected) return false; // the length is not secret
    return WebPassword::constantTimeEqual(given, token_, expected);
}

bool RequestGuard::tokenMatches_(const char* given) const {
    if (!given) return false;
    const char* s;
    size_t n;
    trim(given, &s, &n);
    return tokenMatches_(s, n);
}

int RequestGuard::parseCookie_(const char* header, uint8_t id[16]) {
    // Lenient towards other cookies of the same host (a former device at this address may have
    // set some), strict about bytes and about our own cookie.
    const size_t n = strlen(header);
    if (n > MAX_HEADERS) return -1;
    for (size_t i = 0; i < n; ++i) {
        if (!printable(header[i])) return -1;
    }
    const size_t nameLen = strlen(COOKIE_NAME);
    int found = 0;
    unsigned pairs = 0;
    size_t i = 0;
    while (i < n) {
        while (i < n && ows(header[i])) ++i;
        const size_t start = i;
        while (i < n && header[i] != ';') ++i;
        size_t end = i;
        while (end > start && ows(header[end - 1])) --end;
        if (i < n) ++i; // ';'
        if (end == start) continue; // empty segment ("a=1;" or ";;")
        if (++pairs > 32) return -1;
        const char* eq = (const char*)memchr(header + start, '=', end - start);
        if (!eq) continue; // a value without name: not ours
        const size_t keyLen = (size_t)(eq - (header + start));
        if (keyLen != nameLen || strncmp(header + start, COOKIE_NAME, nameLen) != 0) continue;
        if (found) return -1; // twice: refuse instead of guessing
        const char* v = eq + 1;
        size_t vn = (size_t)(header + end - v);
        if (vn >= 2 && v[0] == '"' && v[vn - 1] == '"') {
            ++v;
            vn -= 2;
        }
        if (vn != SESSION_ID_LEN || !parseHex(v, vn, id)) return -1;
        found = 1;
    }
    return found;
}

int RequestGuard::findSession_(const uint8_t id[16], uint64_t nowMs) const {
    // Every slot is compared (no early exit on the id).
    int hit = -1;
    for (uint8_t i = 0; i < MAX_SESSIONS; ++i) {
        const Session& s = sessions_[i];
        const bool live = s.used && nowMs - s.createdMs < SESSION_MS;
        const bool same = WebPassword::constantTimeEqual(s.id, id, sizeof(s.id));
        if (live && same) hit = i;
    }
    return hit;
}

bool RequestGuard::sessionValid(const char* cookieHeader, uint64_t nowMs) const {
    if (!cookieHeader) return false;
    uint8_t id[16];
    if (parseCookie_(cookieHeader, id) != 1) return false;
    return findSession_(id, nowMs) >= 0;
}

uint8_t RequestGuard::sessionCount(uint64_t nowMs) const {
    uint8_t n = 0;
    for (const Session& s : sessions_) {
        if (s.used && nowMs - s.createdMs < SESSION_MS) ++n;
    }
    return n;
}

void RequestGuard::endAllSessions() {
    for (Session& s : sessions_) s = Session {};
}

void RequestGuard::logout(const char* cookieHeader) {
    if (!cookieHeader) return;
    uint8_t id[16];
    if (parseCookie_(cookieHeader, id) != 1) return;
    for (Session& s : sessions_) {
        if (s.used && WebPassword::constantTimeEqual(s.id, id, sizeof(s.id))) s = Session {};
    }
}

void RequestGuard::clearCookie(char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    snprintf(out, outSize, "%s=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict", COOKIE_NAME);
}

void RequestGuard::makeToken(Random32 random, char out[TOKEN_LEN + 1]) {
    uint8_t b[TOKEN_LEN / 2];
    randomBytes(random, b, sizeof(b));
    toHex(b, sizeof(b), out);
}

// ---------------------------------------------------------------------------
// Decisions

GuardDecision RequestGuard::check(const GuardRequest& r, uint64_t nowMs) {
    if (!r.method || !r.path || r.path[0] != '/') return refuse(Verdict::BadRequest, "Bad request line");
    if (r.freeHeap < MIN_FREE_HEAP) return refuse(Verdict::ServiceUnavailable, "Low memory");
    const size_t queryLen = r.query && r.query[0] ? 1 + strlen(r.query) : 0;
    if (strlen(r.path) + queryLen > MAX_URL) return refuse(Verdict::UriTooLong, "URL too long");
    if (r.headerBytes > MAX_HEADERS) return refuse(Verdict::HeaderFieldsTooLarge, "Headers too large");

    const bool api = isApi(r.path);
    const bool ws = isWs(r.path);
    const bool get = strcmp(r.method, "GET") == 0 || strcmp(r.method, "HEAD") == 0;
    const bool post = strcmp(r.method, "POST") == 0;

    // W-S4: Host allowlist (DNS rebinding); captive portal only for page requests over the AP.
    const char* hs = "";
    size_t hn = 0;
    if (r.host) trim(r.host, &hs, &hn);
    const HostKind host = classifyHost_(hs, hn);
    if (host != HostKind::Own) {
        size_t h = 0;
        const bool nameOnly = host == HostKind::Foreign && splitPort(hs, hn, &h) && hostChars(hs, h);
        if (config_.captive && overAp(r) && nameOnly && get && !api && !ws && !r.upgrade) {
            return refuse(Verdict::Redirect, "Captive portal");
        }
        return refuse(Verdict::MisdirectedRequest, "Host not allowed");
    }

    // W-S5: no CORS preflight, no other methods; POST only for the API.
    if (strcmp(r.method, "OPTIONS") == 0) return refuse(Verdict::MethodNotAllowed, "OPTIONS not allowed");
    if (!get && !post) return refuse(Verdict::MethodNotAllowed, "Method not allowed");
    if (post && !api) return refuse(Verdict::MethodNotAllowed, "Method not allowed");

    // W-S10: body framing and size.
    if (r.transferEncoding) return refuse(Verdict::UnsupportedMediaType, "Transfer-Encoding not allowed");
    if (r.contentLength) {
        uint64_t len = 0;
        if (!parseLength(r.contentLength, &len)) return refuse(Verdict::BadRequest, "Bad Content-Length");
        if (len > MAX_BODY) return refuse(Verdict::PayloadTooLarge, "Body too large");
    }

    // W-S13: broken Cookie / Authorization headers are refused.
    uint8_t id[16] {};
    const int cookie = r.cookie ? parseCookie_(r.cookie, id) : 0;
    if (cookie < 0) return refuse(Verdict::BadRequest, "Bad Cookie header");
    if (r.authorization && !wellFormedAuthorization(r.authorization)) {
        return refuse(Verdict::BadRequest, "Bad Authorization header");
    }

    // W-S6: a WebSocket upgrade only as GET /ws.
    const bool upgrade = r.upgrade != nullptr;
    if (upgrade && !(ws && get && ieqValue(r.upgrade, "websocket"))) return refuse(Verdict::BadRequest, "Bad upgrade");
    if (ws && !upgrade) return refuse(Verdict::BadRequest, "WebSocket upgrade expected");

    if (api || ws) {
        // W-S5, W-S6: only the own pages (or scripts without Origin).
        if (r.origin && !originAllowed_(r.origin)) return refuse(Verdict::Forbidden, "Foreign origin");
        if (ieqValue(r.secFetchSite, "cross-site")) return refuse(Verdict::Forbidden, "Cross-site request");
        if (strcmp(r.path, "/api/token") == 0 && r.secFetchSite && !ieqValue(r.secFetchSite, "same-origin") &&
            !ieqValue(r.secFetchSite, "none")) {
            return refuse(Verdict::Forbidden, "Same-origin only");
        }
    }

    if (post) {
        if (!r.contentType || !jsonContentType(r.contentType)) {
            return refuse(Verdict::UnsupportedMediaType, "Content-Type must be application/json");
        }
        if (!r.contentLength) return refuse(Verdict::LengthRequired, "Length required");
        if (!tokenMatches_(r.token)) return refuse(Verdict::Forbidden, "Bad token");
    }

    if (ws) {
        // Query "t=<token>", other parameters ignored, the token exactly once.
        const char* q = r.query ? r.query : "";
        const char* t = nullptr;
        size_t tn = 0;
        int count = 0;
        while (*q) {
            const char* end = strchr(q, '&');
            const size_t len = end ? (size_t)(end - q) : strlen(q);
            if (len >= 2 && q[0] == 't' && q[1] == '=') {
                t = q + 2;
                tn = len - 2;
                ++count;
            }
            q += len;
            if (*q == '&') ++q;
        }
        if (count != 1 || !tokenMatches_(t, tn)) return refuse(Verdict::Forbidden, "Bad token");
    }

    // With a web password: a session for every write and the WiFi settings, with loginForReads
    // also for the state and the WebSocket. The login and the logout never need one (a logout
    // with an expired session must not ask for the password first).
    const bool sessionFree = post && (strcmp(r.path, "/api/login") == 0 || strcmp(r.path, "/api/logout") == 0);
    const bool wifi = strcmp(r.path, "/api/wifi") == 0 || startsWith(r.path, "/api/wifi/");
    const bool needed = post || wifi || (config_.loginForReads && (api || ws));
    if (config_.passwordSet && needed && !sessionFree) {
        const int s = cookie == 1 ? findSession_(id, nowMs) : -1;
        if (s < 0) return refuse(Verdict::Unauthorized, "Login required");
        sessions_[s].lastUseMs = nowMs;
    }
    return ALLOW;
}

GuardDecision RequestGuard::checkSecrets(const GuardRequest& r, bool bodyCarriesSecret) const {
    if (bodyCarriesSecret && overOpenAp(r)) return refuse(Verdict::Forbidden, "No passwords over an open access point");
    return ALLOW;
}

GuardDecision RequestGuard::loginAllowed(uint64_t nowMs) const {
    if (locked_ && nowMs - lockStartMs_ < lockMs_) return refuse(Verdict::TooManyRequests, "Login locked");
    return ALLOW;
}

uint32_t RequestGuard::loginRetryAfterS(uint64_t nowMs) const {
    if (!locked_ || nowMs - lockStartMs_ >= lockMs_) return 0;
    const uint32_t left = (uint32_t)(lockMs_ - (nowMs - lockStartMs_));
    return (left + 999) / 1000;
}

void RequestGuard::loginFailed(uint64_t nowMs) {
    if (failures_ > 0 || lockLevel_ > 0) {
        if (nowMs - lastFailureMs_ >= LOGIN_FORGET_MS) {
            failures_ = 0;
            lockLevel_ = 0;
        }
    }
    lastFailureMs_ = nowMs;
    if (loginAllowed(nowMs).verdict != Verdict::Allow) return; // a check that ran during the lock
    if (++failures_ < LOGIN_FAILURES) return;
    failures_ = 0;
    locked_ = true;
    lockStartMs_ = nowMs;
    lockMs_ = LOGIN_LOCK_MS << lockLevel_;
    if (lockLevel_ < LOGIN_MAX_DOUBLINGS) ++lockLevel_;
}

bool RequestGuard::loginSucceeded(uint64_t nowMs, Random32 random, char* setCookie, size_t setCookieSize) {
    if (!setCookie || setCookieSize < SET_COOKIE_SIZE || !random) return false;
    failures_ = 0;
    lockLevel_ = 0;
    locked_ = false;
    // A free or expired slot, else the least recently used one.
    int slot = -1;
    for (uint8_t i = 0; i < MAX_SESSIONS && slot < 0; ++i) {
        const Session& s = sessions_[i];
        if (!s.used || nowMs - s.createdMs >= SESSION_MS) slot = i;
    }
    if (slot < 0) {
        slot = 0;
        for (uint8_t i = 1; i < MAX_SESSIONS; ++i) {
            if (nowMs - sessions_[i].lastUseMs > nowMs - sessions_[slot].lastUseMs) slot = i;
        }
    }
    Session& s = sessions_[slot];
    randomBytes(random, s.id, sizeof(s.id));
    s.createdMs = nowMs;
    s.lastUseMs = nowMs;
    s.used = true;
    char hex[SESSION_ID_LEN + 1];
    toHex(s.id, sizeof(s.id), hex);
    snprintf(setCookie, setCookieSize, "%s=%s; Path=/; Max-Age=%lu; HttpOnly; SameSite=Strict", COOKIE_NAME, hex,
             (unsigned long)(SESSION_MS / 1000));
    return true;
}

GuardDecision RequestGuard::scanAllowed(uint64_t nowMs) {
    if (scanned_ && nowMs - lastScanMs_ < SCAN_INTERVAL_MS) return refuse(Verdict::TooManyRequests, "Scan at most every 10 s");
    scanned_ = true;
    lastScanMs_ = nowMs;
    return ALLOW;
}

// ---------------------------------------------------------------------------
// Web password

namespace WebPassword {

namespace {
const char PREFIX[] = "pbkdf2-sha1$";

void derive(const char* password, size_t len, const uint8_t* salt, uint32_t iterations, uint8_t out[HASH_LEN]) {
    Crypto::pbkdf2HmacSha1((const uint8_t*)password, len, salt, SALT_LEN, iterations, out, HASH_LEN);
}

void wipe(void* p, size_t n) {
    volatile uint8_t* b = (volatile uint8_t*)p;
    for (size_t i = 0; i < n; ++i) b[i] = 0;
}
}

bool constantTimeEqual(const void* a, const void* b, size_t n) {
    const volatile uint8_t* x = (const volatile uint8_t*)a;
    const volatile uint8_t* y = (const volatile uint8_t*)b;
    uint8_t d = 0;
    for (size_t i = 0; i < n; ++i) d = (uint8_t)(d | (x[i] ^ y[i]));
    return d == 0;
}

bool valid(const char* password, size_t len) {
    if (!password || len < MIN_LEN || len > MAX_LEN) return false;
    // Sanitize turns invalid UTF-8 and control characters into '?': unchanged = valid.
    char copy[MAX_LEN + 1];
    const size_t n = Sanitize::text(password, len, copy, sizeof(copy), MAX_LEN);
    const bool same = n == len && memcmp(copy, password, len) == 0;
    wipe(copy, sizeof(copy));
    return same;
}

bool hash(const char* password, size_t len, RequestGuard::Random32 random, char out[RECORD_SIZE]) {
    if (!out) return false;
    out[0] = '\0';
    if (!random || !valid(password, len)) return false;
    uint8_t salt[SALT_LEN];
    randomBytes(random, salt, sizeof(salt));
    const bool ok = hashWithSalt(password, len, salt, out);
    wipe(salt, sizeof(salt));
    return ok;
}

bool hashWithSalt(const char* password, size_t len, const uint8_t salt[SALT_LEN], char out[RECORD_SIZE]) {
    if (!out) return false;
    out[0] = '\0';
    if (!salt || !valid(password, len)) return false;
    uint8_t dk[HASH_LEN];
    derive(password, len, salt, ITERATIONS, dk);
    char saltHex[2 * SALT_LEN + 1];
    char dkHex[2 * HASH_LEN + 1];
    toHex(salt, SALT_LEN, saltHex); // salt is a pointer here
    toHex(dk, sizeof(dk), dkHex);
    snprintf(out, RECORD_SIZE, "%s%lu$%s$%s", PREFIX, (unsigned long)ITERATIONS, saltHex, dkHex);
    wipe(dk, sizeof(dk));
    wipe(dkHex, sizeof(dkHex));
    return true;
}

namespace {
bool parseRecord(const char* record, uint32_t* iterations, uint8_t salt[SALT_LEN], uint8_t expected[HASH_LEN]) {
    if (!record) return false;
    const size_t p = sizeof(PREFIX) - 1;
    if (strncmp(record, PREFIX, p) != 0) return false;
    const char* s = record + p;
    uint32_t it = 0;
    int digits = 0;
    while (digit(*s)) {
        if (++digits > 6) return false;
        it = it * 10 + (uint32_t)(*s++ - '0');
    }
    if (digits == 0 || *s++ != '$' || it < MIN_ITERATIONS || it > MAX_ITERATIONS) return false;
    if (strlen(s) != 2 * SALT_LEN + 1 + 2 * HASH_LEN || s[2 * SALT_LEN] != '$') return false;
    if (!parseHex(s, 2 * SALT_LEN, salt) || !parseHex(s + 2 * SALT_LEN + 1, 2 * HASH_LEN, expected)) return false;
    *iterations = it;
    return true;
}
}

bool wellFormed(const char* record) {
    uint32_t iterations = 0;
    uint8_t salt[SALT_LEN];
    uint8_t expected[HASH_LEN];
    return parseRecord(record, &iterations, salt, expected);
}

bool verify(const char* password, size_t len, const char* record) {
    if (!password || !record || len > MAX_LEN) return false;
    uint32_t iterations = 0;
    uint8_t salt[SALT_LEN];
    uint8_t expected[HASH_LEN];
    if (!parseRecord(record, &iterations, salt, expected)) return false;
    uint8_t dk[HASH_LEN];
    derive(password, len, salt, iterations, dk);
    const bool same = constantTimeEqual(dk, expected, HASH_LEN);
    wipe(dk, sizeof(dk));
    return same;
}

}
