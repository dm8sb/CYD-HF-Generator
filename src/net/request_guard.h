#pragma once

#include <stddef.h>
#include <stdint.h>

// Request guard of the web server (ARBEITSPLAN 3.3, 3.7): decides about every HTTP request and
// WebSocket upgrade before the server reads a body or queues a command. Pure and deterministic:
// the time comes in as nowMs, randomness through a function pointer, nothing is executed here.
// nowMs is a 64-bit monotonic millisecond count (the server passes esp_timer_get_time() / 1000):
// with 32-bit millis() an expired session, login lock or scan stamp came back after the 49.7-day
// wrap (fuzz finding S57c).
// No Arduino or web server types; the server copies what it needs into GuardRequest. Not
// thread-safe: the server calls it from one task (async_tcp) or under its own lock.
//
// Order of the checks in check() (the first failing one answers):
//   503 free heap < MIN_FREE_HEAP (W-S10)
//   414 URL > MAX_URL, 431 headers > MAX_HEADERS (W-S10)
//   421 Host not the own IPv4 (optional port), <name> or <name>.local; IPv6 literals never
//       (W-S4). Over the AP with captive DNS a GET/HEAD of a page (not /api/, not /ws) with a
//       well-formed foreign host gets 302 to the AP address instead (captive portal).
//   405 OPTIONS (no CORS) and every method except GET, HEAD, POST; POST only on /api/ (W-S5)
//   415 Transfer-Encoding (chunked) on any request (W-S10)
//   400 Content-Length not a plain decimal number; 413 above MAX_BODY (W-S10)
//   400 broken Cookie or Authorization header (W-S13); a well-formed Authorization is ignored
//       (no Basic-Auth, W-E8), it never grants access
//   400 Upgrade other than a WebSocket upgrade of GET /ws, or GET /ws without upgrade
//   403 /api/ and /ws: Origin present and not http://<allowed host>, Sec-Fetch-Site cross-site;
//       GET /api/token only same-origin (Sec-Fetch-Site absent, same-origin or none) (W-S5)
//   415 POST without Content-Type application/json (optional ;charset=utf-8), 411 without
//       Content-Length, 403 without the valid X-CYD-Token (W-S5)
//   403 WebSocket upgrade without the valid token in the query (t=<token>) (W-S6)
//   401 a web password is set and the request has no valid session: every POST except
//       /api/login and /api/logout, and GET /api/wifi, /api/wifi/*; with loginForReads also all other /api/ and
//       /ws (like the mock's --login-all). Pages, their files and by default the state load
//       without login (ARBEITSPLAN 3.6: the state never holds secrets, MAC or commit)
// After parsing a body the server calls checkSecrets() (W-S1), on /api/login loginAllowed()
// (W-S11), on /api/wifi/scan scanAllowed() (W-S11). The write rate limit (20/s per source) is
// done by CommandQueue::push() (W-S11) and not repeated here.

enum class Verdict : uint16_t {
    Allow = 200,
    Redirect = 302,             // captive portal: Location from captiveLocation()
    BadRequest = 400,
    Unauthorized = 401,         // login needed
    Forbidden = 403,
    MethodNotAllowed = 405,
    LengthRequired = 411,
    PayloadTooLarge = 413,
    UriTooLong = 414,
    UnsupportedMediaType = 415,
    MisdirectedRequest = 421,   // Host not allowed
    TooManyRequests = 429,
    HeaderFieldsTooLarge = 431,
    ServiceUnavailable = 503
};

struct GuardDecision {
    Verdict verdict;
    const char* reason; // short English text for the log or an error body, never nullptr
    bool allowed() const { return verdict == Verdict::Allow; }
    uint16_t status() const { return (uint16_t)verdict; }
};

// Addresses are IPv4 as (a << 24) | (b << 16) | (c << 8) | d, 0 = none.
struct GuardConfig {
    uint32_t staIp = 0;               // own address in the client network
    uint32_t apIp = 0;                // own address while the access point runs
    bool apOpen = false;              // the access point runs without a password (W-S1)
    bool captive = false;             // captive DNS runs (AP mode): page requests are redirected
    const char* deviceName = nullptr; // RFC 1123, e.g. "cyd-hf-1a2b" (copied)
    const char* token = nullptr;      // per-boot token, see makeToken() (copied)
    bool passwordSet = false;         // a web password is set: writes and WiFi need a session
    bool loginForReads = false;       // ... and also the state, the token and the WebSocket
};

// One request as the server sees it. Header values are nullptr when the header is absent.
struct GuardRequest {
    const char* method = nullptr; // "GET", "POST", ... (case-sensitive)
    const char* path = nullptr;   // "/api/state", without the query
    const char* query = nullptr;  // after '?', nullptr or "" without
    const char* host = nullptr;
    const char* origin = nullptr;
    const char* secFetchSite = nullptr;
    const char* contentType = nullptr;
    const char* contentLength = nullptr;
    const char* transferEncoding = nullptr;
    const char* token = nullptr; // X-CYD-Token
    const char* cookie = nullptr;
    const char* authorization = nullptr;
    const char* upgrade = nullptr;
    size_t headerBytes = 0;      // all header lines together (names, values, separators)
    uint32_t clientIp = 0;       // peer address
    uint32_t localIp = 0;        // own address the connection arrived on (0: unknown)
    uint32_t freeHeap = 0;       // free heap now (bytes)
};

class RequestGuard {
public:
    typedef uint32_t (*Random32)();

    static constexpr size_t MAX_URL = 256;             // path and query (W-S10)
    static constexpr size_t MAX_HEADERS = 2048;        // W-S10
    static constexpr size_t MAX_BODY = 1024;           // W-S10
    static constexpr uint32_t MIN_FREE_HEAP = 24 * 1024; // below: 503 (ARBEITSPLAN 3.4)
    static constexpr size_t TOKEN_LEN = 32;            // hex digits = 128 bit
    static constexpr size_t MAX_NAME = 32;             // device name (W-E5)
    // Sessions (W-S13): random 128-bit id as 32 hex digits in the cookie, 12 h from the login,
    // at most MAX_SESSIONS (W-E10: 3 clients plus one re-login); a new login replaces the
    // least recently used one.
    static constexpr const char* COOKIE_NAME = "cyd_sid";
    static constexpr size_t SESSION_ID_LEN = 32;
    static constexpr uint8_t MAX_SESSIONS = 4;
    static constexpr uint32_t SESSION_MS = 12UL * 3600UL * 1000UL;
    // Set-Cookie value incl. NUL: "cyd_sid=<32>; Path=/; Max-Age=43200; HttpOnly; SameSite=Strict".
    static constexpr size_t SET_COOKIE_SIZE = 96;
    // Login (W-S11): after LOGIN_FAILURES wrong passwords LOGIN_LOCK_MS locked, each further lock
    // doubles up to 2^LOGIN_MAX_DOUBLINGS (16 min). One counter for all clients: an attacker
    // cannot spread the attempts over addresses; the price is that he can lock the web login
    // (the touch screen stays usable). The level falls back to 0 after a correct login or
    // LOGIN_FORGET_MS without failure.
    static constexpr uint8_t LOGIN_FAILURES = 5;
    static constexpr uint32_t LOGIN_LOCK_MS = 30000;
    static constexpr uint8_t LOGIN_MAX_DOUBLINGS = 5;
    static constexpr uint32_t LOGIN_FORGET_MS = 3600000;
    static constexpr uint32_t SCAN_INTERVAL_MS = 10000; // WiFi scan (W-S11)

    void configure(const GuardConfig& c);
    const GuardConfig& config() const { return config_; }

    // The decision for one request (see the order above); nowMs for the session expiry.
    GuardDecision check(const GuardRequest& r, uint64_t nowMs);
    // W-S1: over an open access point a body with password fields (RestJson::carriesSecret())
    // is refused with 403, whatever the endpoint.
    GuardDecision checkSecrets(const GuardRequest& r, bool bodyCarriesSecret) const;
    // The connection arrived over the access point (localIp, or else clientIp in its /24).
    bool overAp(const GuardRequest& r) const;
    bool overOpenAp(const GuardRequest& r) const { return config_.apOpen && overAp(r); }
    // "http://192.168.4.1/" for the captive portal redirect; false without AP.
    bool captiveLocation(char* out, size_t outSize) const;

    // Login: loginAllowed() before the password is checked (429 while locked), then
    // loginFailed() or loginSucceeded(); the latter starts a session and writes the
    // Set-Cookie value. The password check itself (WebPassword::verify, slow) is the server's.
    GuardDecision loginAllowed(uint64_t nowMs) const;
    uint32_t loginRetryAfterS(uint64_t nowMs) const; // 0 when not locked
    void loginFailed(uint64_t nowMs);
    bool loginSucceeded(uint64_t nowMs, Random32 random, char* setCookie, size_t setCookieSize);
    // Ends the session of this Cookie header (logout); clearCookie() gives the Set-Cookie value.
    void logout(const char* cookieHeader);
    static void clearCookie(char* out, size_t outSize);
    // Password changed or removed: every session ends.
    void endAllSessions();
    uint8_t sessionCount(uint64_t nowMs) const;
    bool sessionValid(const char* cookieHeader, uint64_t nowMs) const;

    // GET /api/wifi/scan: Allow (and noted) at most every SCAN_INTERVAL_MS, else 429.
    GuardDecision scanAllowed(uint64_t nowMs);

    // Per-boot token for X-CYD-Token and the WebSocket query: TOKEN_LEN lower-case hex digits.
    static void makeToken(Random32 random, char out[TOKEN_LEN + 1]);

private:
    enum class HostKind : uint8_t { Own, Foreign, Invalid };
    HostKind classifyHost_(const char* s, size_t n) const;
    bool originAllowed_(const char* origin) const;
    bool tokenMatches_(const char* given) const;
    bool tokenMatches_(const char* given, size_t n) const;
    // -1 broken header, 0 no session cookie, 1 id in *id.
    static int parseCookie_(const char* header, uint8_t id[16]);
    int findSession_(const uint8_t id[16], uint64_t nowMs) const;

    struct Session {
        uint8_t id[16];
        uint64_t createdMs;
        uint64_t lastUseMs;
        bool used;
    };
    GuardConfig config_ {};
    char name_[MAX_NAME + 1] {};
    char token_[TOKEN_LEN * 2 + 1] {};
    Session sessions_[MAX_SESSIONS] {};
    uint8_t failures_ = 0;
    uint8_t lockLevel_ = 0;
    bool locked_ = false;
    uint64_t lockStartMs_ = 0;
    uint32_t lockMs_ = 0;
    uint64_t lastFailureMs_ = 0;
    bool scanned_ = false;
    uint64_t lastScanMs_ = 0;
};

// Web password (W-S13): 8..64 bytes of UTF-8 without control characters, stored as
// "pbkdf2-sha1$10000$<salt 32 hex>$<hash 40 hex>" (PBKDF2-HMAC-SHA1, RFC 8018, 128-bit random
// salt). Crypto::pbkdf2HmacSha1 (net/pbkdf2.*, checked against RFC 6070) instead of mbedTLS:
// the same code runs in the host tests and gives the same hash; mbedTLS computes the same
// function, so the server can switch to it later without changing stored records.
// Iterations: 10 000, the minimum asked for (NIST SP 800-132 recommends at least 1 000; the
// OWASP value for SHA-1 is far beyond an ESP32). The implementation hashes both HMAC pads in
// every iteration (4 SHA-1 blocks), i.e. 40 000 blocks per check; not measured, estimated at
// 0.2-0.3 s on the ESP32 (software SHA-1, 240 MHz). verify() therefore must not run in the
// async_tcp task (W-S23: no network task blocks > 100 ms); the login lock bounds it to
// LOGIN_FAILURES checks per lock period. The iteration count is part of the record, so it can
// be raised later; verify() accepts MIN_ITERATIONS..MAX_ITERATIONS only (a damaged record must
// not hang the device).
namespace WebPassword {

static constexpr size_t MIN_LEN = 8;
static constexpr size_t MAX_LEN = 64;
static constexpr uint32_t ITERATIONS = 10000;
static constexpr uint32_t MIN_ITERATIONS = 1000;
static constexpr uint32_t MAX_ITERATIONS = 100000;
static constexpr size_t SALT_LEN = 16;
static constexpr size_t HASH_LEN = 20;
static constexpr size_t RECORD_SIZE = 96; // incl. NUL

bool valid(const char* password, size_t len);
// false if the password is not valid().
bool hash(const char* password, size_t len, RequestGuard::Random32 random, char out[RECORD_SIZE]);
// The same with a given salt (the device draws it in loop() and hashes in a worker task, S55b).
bool hashWithSalt(const char* password, size_t len, const uint8_t salt[SALT_LEN], char out[RECORD_SIZE]);
// Constant-time comparison of the derived key; false for a malformed record.
bool verify(const char* password, size_t len, const char* record);
// The record has the stored form (prefix, iterations in range, hex salt and hash); NVS check.
bool wellFormed(const char* record);
// Compares n bytes without an early exit.
bool constantTimeEqual(const void* a, const void* b, size_t n);

}
