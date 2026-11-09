// Request guard of the web server (ARBEITSPLAN 3.6, 3.7): Host allowlist (W-S4), POST rules
// and no CORS (W-S5), WebSocket upgrade (W-S6), sizes and heap (W-S10), login lock and scan
// rate (W-S11), sessions and broken headers (W-S13), password fields over an open AP (W-S1),
// web password hash; a random-input test with a fixed seed (W-S25).

#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <net/pbkdf2.h>
#include <net/request_guard.h>

static const uint32_t STA = (192u << 24) | (168u << 16) | (1u << 8) | 50u; // 192.168.1.50
static const uint32_t PEER = STA + 7;
static const uint32_t AP = (192u << 24) | (168u << 16) | (4u << 8) | 1u; // 192.168.4.1
static const char TOKEN[] = "0123456789abcdef0123456789abcdef";
static const char NAME[] = "cyd-hf-1a2b";

static uint32_t seed = 1;
static uint32_t rnd() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

void setUp() { seed = 1; }
void tearDown() {}

static GuardConfig config(bool password = false) {
    GuardConfig c;
    c.staIp = STA;
    c.deviceName = NAME;
    c.token = TOKEN;
    c.passwordSet = password;
    return c;
}

static GuardConfig apConfig(bool open) {
    GuardConfig c = config();
    c.staIp = 0;
    c.apIp = AP;
    c.apOpen = open;
    c.captive = true;
    return c;
}

static GuardRequest get(const char* path, const char* host = "192.168.1.50") {
    GuardRequest r;
    r.method = "GET";
    r.path = path;
    r.host = host;
    r.headerBytes = 400;
    r.clientIp = PEER;
    r.localIp = STA;
    r.freeHeap = 60000;
    return r;
}

// What the own page sends for a REST write.
static GuardRequest post(const char* path, const char* host = "192.168.1.50") {
    GuardRequest r = get(path, host);
    r.method = "POST";
    r.contentType = "application/json";
    r.contentLength = "2";
    r.token = TOKEN;
    r.origin = "http://192.168.1.50";
    r.secFetchSite = "same-origin";
    return r;
}

static GuardRequest wsUpgrade(const char* query) {
    GuardRequest r = get("/ws");
    r.query = query;
    r.upgrade = "websocket";
    r.origin = "http://192.168.1.50";
    return r;
}

static RequestGuard guard(const GuardConfig& c) {
    RequestGuard g;
    g.configure(c);
    return g;
}

static uint16_t status(RequestGuard& g, const GuardRequest& r, uint64_t now = 0) { return g.check(r, now).status(); }

static uint16_t hostStatus(RequestGuard& g, const char* host) { return status(g, get("/api/state", host)); }

static void test_host_allowlist() {
    RequestGuard g = guard(config());
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, "192.168.1.50"));
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, "192.168.1.50:80"));
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, " 192.168.1.50:8080 "));
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, "cyd-hf-1a2b"));
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, "cyd-hf-1a2b.local"));
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, "CYD-HF-1A2B.LOCAL:80"));
    // DNS rebinding and everything else: 421.
    const char* const foreign[] = {
        "192.168.1.51", "evil.example", "cyd-hf-1a2b.local.evil.example", "cyd-hf-1a2bx", "x.cyd-hf-1a2b.local",
        "cyd-hf-1a2b.local.", "cyd-hf-1a2b.localx", "[::1]", "[fe80::1]:80", "::1", "", " ", "192.168.1.50:",
        "192.168.1.50:0", "192.168.1.50:65536", "192.168.1.50:80:80", "192.168.1.050", "192.168.1.50.",
        "192.168.001.50", ":80", "cyd hf", "cyd-hf-1a2b\t.local", "localhost", "127.0.0.1", "0.0.0.0",
        "192.168.4.1", "cyd-hf-1a2b.local\x01", "user@192.168.1.50"};
    for (const char* h : foreign) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(421, hostStatus(g, h), h);
    }
    TEST_ASSERT_EQUAL_UINT16(421, status(g, get("/api/state", nullptr)));
    TEST_ASSERT_EQUAL_STRING("Host not allowed", g.check(get("/", "evil.example"), 0).reason);
    // Pages are behind the same allowlist.
    TEST_ASSERT_EQUAL_UINT16(421, status(g, get("/", "evil.example")));
    // The AP address counts while the AP runs; no own address without client connection.
    GuardConfig c = config();
    c.apIp = AP;
    g.configure(c);
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, "192.168.4.1"));
    c.staIp = 0;
    g.configure(c);
    TEST_ASSERT_EQUAL_UINT16(421, hostStatus(g, "192.168.1.50"));
    TEST_ASSERT_EQUAL_UINT16(421, hostStatus(g, "0.0.0.0"));
    // configure() copies the name: the caller's buffer may change.
    char name[16];
    strcpy(name, "dev-a");
    c = config();
    c.deviceName = name;
    g.configure(c);
    strcpy(name, "dev-b");
    TEST_ASSERT_EQUAL_UINT16(200, hostStatus(g, "dev-a.local"));
    TEST_ASSERT_EQUAL_UINT16(421, hostStatus(g, "dev-b.local"));
}

static void test_captive_portal_only_for_pages_over_the_ap() {
    RequestGuard g = guard(apConfig(false));
    GuardRequest r = get("/generate_204", "connectivitycheck.gstatic.com");
    r.localIp = AP;
    r.clientIp = AP + 1;
    GuardDecision d = g.check(r, 0);
    TEST_ASSERT_EQUAL_UINT16(302, d.status());
    char loc[32];
    TEST_ASSERT_TRUE(g.captiveLocation(loc, sizeof(loc)));
    TEST_ASSERT_EQUAL_STRING("http://192.168.4.1/", loc);
    r.host = "captive.apple.com:80";
    TEST_ASSERT_EQUAL_UINT16(302, status(g, r));
    r.method = "HEAD";
    TEST_ASSERT_EQUAL_UINT16(302, status(g, r));
    // Own host over the AP: served normally.
    r.host = "192.168.4.1";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    // Never the API, the WebSocket, a POST or a garbage host.
    GuardRequest api = get("/api/state", "evil.example");
    api.localIp = AP;
    TEST_ASSERT_EQUAL_UINT16(421, status(g, api));
    GuardRequest p = post("/api/config", "evil.example");
    p.localIp = AP;
    TEST_ASSERT_EQUAL_UINT16(421, status(g, p));
    GuardRequest ws = wsUpgrade("t=0123456789abcdef0123456789abcdef");
    ws.host = "evil.example";
    ws.localIp = AP;
    TEST_ASSERT_EQUAL_UINT16(421, status(g, ws));
    GuardRequest junk = get("/", "a_b.example");
    junk.localIp = AP;
    TEST_ASSERT_EQUAL_UINT16(421, status(g, junk));
    // Not over the AP (client network) or without captive DNS: 421.
    GuardConfig c = apConfig(false);
    c.staIp = STA;
    g.configure(c);
    GuardRequest sta = get("/generate_204", "connectivitycheck.gstatic.com");
    TEST_ASSERT_EQUAL_UINT16(421, status(g, sta));
    c.captive = false;
    g.configure(c);
    r.host = "connectivitycheck.gstatic.com";
    TEST_ASSERT_EQUAL_UINT16(421, status(g, r));
    // Without the local address the peer's /24 decides.
    g.configure(apConfig(false));
    GuardRequest byPeer = get("/", "neverssl.com");
    byPeer.localIp = 0;
    byPeer.clientIp = AP + 5;
    TEST_ASSERT_EQUAL_UINT16(302, status(g, byPeer));
    byPeer.clientIp = PEER;
    TEST_ASSERT_EQUAL_UINT16(421, status(g, byPeer));
    RequestGuard none = guard(config());
    TEST_ASSERT_FALSE(none.captiveLocation(loc, sizeof(loc)));
}

static void test_methods_and_no_cors() {
    RequestGuard g = guard(config());
    GuardRequest r = get("/api/state");
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.method = "HEAD";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.method = "OPTIONS";
    TEST_ASSERT_EQUAL_UINT16(405, status(g, r));
    TEST_ASSERT_EQUAL_STRING("OPTIONS not allowed", g.check(r, 0).reason);
    const char* const others[] = {"PUT", "DELETE", "PATCH", "TRACE", "CONNECT", "get", "", "GET "};
    for (const char* m : others) {
        r.method = m;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(405, status(g, r), m);
    }
    // POST only for the API.
    TEST_ASSERT_EQUAL_UINT16(405, status(g, post("/")));
    TEST_ASSERT_EQUAL_UINT16(405, status(g, post("/index.html")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, post("/api/config")));
    // Malformed request line.
    r = get("/api/state");
    r.method = nullptr;
    TEST_ASSERT_EQUAL_UINT16(400, status(g, r));
    r = get("relative");
    TEST_ASSERT_EQUAL_UINT16(400, status(g, r));
}

static void test_post_needs_json_token_and_own_origin() {
    RequestGuard g = guard(config());
    GuardRequest r = post("/api/channel/1");
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    // Content-Type (W-S5): 415.
    const char* const goodTypes[] = {"application/json", "application/json; charset=utf-8",
                                     "Application/JSON;charset=UTF-8", " application/json ",
                                     "application/json ; charset=\"utf-8\""};
    for (const char* t : goodTypes) {
        r.contentType = t;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(200, status(g, r), t);
    }
    const char* const badTypes[] = {"text/plain", "application/x-www-form-urlencoded", "multipart/form-data; boundary=x",
                                    "application/json; charset=latin1", "application/jsonx", "application/json;",
                                    "application/json, text/plain", "application/json-patch+json", ""};
    for (const char* t : badTypes) {
        r.contentType = t;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(415, status(g, r), t);
    }
    r.contentType = nullptr;
    TEST_ASSERT_EQUAL_UINT16(415, status(g, r));
    // Token (W-S5): 403.
    r = post("/api/channel/1");
    const char* const badTokens[] = {"", "0123456789abcdef0123456789abcdee", "0123456789abcdef0123456789abcde",
                                     "0123456789abcdef0123456789abcdef0", "0123456789ABCDEF0123456789ABCDEF"};
    for (const char* t : badTokens) {
        r.token = t;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(403, status(g, r), t);
    }
    r.token = nullptr;
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
    TEST_ASSERT_EQUAL_STRING("Bad token", g.check(r, 0).reason);
    // Without a configured token nothing passes.
    GuardConfig c = config();
    c.token = nullptr;
    RequestGuard noToken = guard(c);
    r = post("/api/channel/1");
    r.token = "";
    TEST_ASSERT_EQUAL_UINT16(403, status(noToken, r));
    // Origin empty or own host; Sec-Fetch-Site not cross-site.
    r = post("/api/channel/1");
    const char* const goodOrigins[] = {"http://192.168.1.50", "http://192.168.1.50:80", "http://cyd-hf-1a2b.local",
                                       "HTTP://CYD-HF-1A2B.local", ""};
    for (const char* o : goodOrigins) {
        r.origin = o;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(200, status(g, r), o);
    }
    r.origin = nullptr;
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    const char* const badOrigins[] = {"http://evil.example", "null", "https://192.168.1.50", "http://192.168.1.50/",
                                      "http://192.168.1.51", "http://", "file://", "http://cyd-hf-1a2b.local.evil.example"};
    for (const char* o : badOrigins) {
        r.origin = o;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(403, status(g, r), o);
    }
    r.origin = "http://192.168.1.50";
    r.secFetchSite = "cross-site";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
    r.secFetchSite = " Cross-Site ";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
    r.secFetchSite = "same-site";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.secFetchSite = nullptr; // scripts
    r.origin = nullptr;
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    // GET on the API: the same origin rules, no token needed.
    GuardRequest q = get("/api/state");
    q.origin = "http://evil.example";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, q));
    q.origin = nullptr;
    q.secFetchSite = "cross-site";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, q));
    // A page may be linked from anywhere.
    GuardRequest page = get("/");
    page.secFetchSite = "cross-site";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, page));
}

static void test_token_endpoint_is_same_origin_only() {
    RequestGuard g = guard(config());
    GuardRequest r = get("/api/token");
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.secFetchSite = "same-origin";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.secFetchSite = "none";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.secFetchSite = "same-site";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
    r.secFetchSite = "cross-site";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
}

static void test_size_and_heap_limits() {
    RequestGuard g = guard(config());
    // URL <= 256 B (path and query): 414.
    std::string path = "/" + std::string(255, 'a');
    GuardRequest r = get(path.c_str());
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    path += "a";
    r = get(path.c_str());
    TEST_ASSERT_EQUAL_UINT16(414, status(g, r));
    std::string query(254 - 4, 'q');
    r = get("/ws");
    r.query = query.c_str(); // "/ws" + "?" + 250 = 254
    r.upgrade = "websocket";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r)); // within the limit (token missing)
    query += "qqq";
    r.query = query.c_str();
    TEST_ASSERT_EQUAL_UINT16(414, status(g, r));
    // Headers <= 2 KB: 431.
    r = get("/api/state");
    r.headerBytes = 2048;
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.headerBytes = 2049;
    TEST_ASSERT_EQUAL_UINT16(431, status(g, r));
    // Body <= 1 KB: 413; Content-Length a plain number: 400.
    GuardRequest p = post("/api/config");
    p.contentLength = "1024";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, p));
    p.contentLength = " 0 ";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, p));
    p.contentLength = "1025";
    TEST_ASSERT_EQUAL_UINT16(413, status(g, p));
    p.contentLength = "9999999999";
    TEST_ASSERT_EQUAL_UINT16(413, status(g, p));
    const char* const badLengths[] = {"", "abc", "-1", "+5", "5, 5", "1 0", "0x10", "12345678901", "1e3"};
    for (const char* l : badLengths) {
        p.contentLength = l;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(400, status(g, p), l);
    }
    p.contentLength = nullptr;
    TEST_ASSERT_EQUAL_UINT16(411, status(g, p));
    // Chunked (or any Transfer-Encoding) and multipart: 415.
    p = post("/api/config");
    p.transferEncoding = "chunked";
    TEST_ASSERT_EQUAL_UINT16(415, status(g, p));
    r = get("/api/state");
    r.transferEncoding = "chunked";
    TEST_ASSERT_EQUAL_UINT16(415, status(g, r));
    p = post("/api/config");
    p.contentType = "multipart/form-data; boundary=----x";
    TEST_ASSERT_EQUAL_UINT16(415, status(g, p));
    // Free heap < 24 KB: 503 before anything else.
    r = get("/", "evil.example");
    r.freeHeap = 24 * 1024 - 1;
    TEST_ASSERT_EQUAL_UINT16(503, status(g, r));
    r = get("/");
    r.freeHeap = 24 * 1024;
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
}

static void test_websocket_upgrade() {
    RequestGuard g = guard(config());
    TEST_ASSERT_EQUAL_UINT16(200, status(g, wsUpgrade("t=0123456789abcdef0123456789abcdef")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, wsUpgrade("x=1&t=0123456789abcdef0123456789abcdef")));
    GuardRequest r = wsUpgrade("t=0123456789abcdef0123456789abcdef");
    r.upgrade = "WebSocket";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    // Token: exactly once and right.
    const char* const badQueries[] = {"", "t=", "t=0123456789abcdef0123456789abcdee", "T=0123456789abcdef0123456789abcdef",
                                      "t=0123456789abcdef0123456789abcdef&t=0123456789abcdef0123456789abcdef",
                                      "tt=0123456789abcdef0123456789abcdef", "t=0123456789abcdef0123456789abcdef0"};
    for (const char* q : badQueries) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(403, status(g, wsUpgrade(q)), q);
    }
    r = wsUpgrade(nullptr);
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
    // Origin and Host are checked like for the API.
    r = wsUpgrade("t=0123456789abcdef0123456789abcdef");
    r.origin = "http://evil.example";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
    r.origin = nullptr;
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.secFetchSite = "cross-site";
    TEST_ASSERT_EQUAL_UINT16(403, status(g, r));
    r = wsUpgrade("t=0123456789abcdef0123456789abcdef");
    r.host = "evil.example";
    TEST_ASSERT_EQUAL_UINT16(421, status(g, r));
    // Upgrade only as GET /ws.
    r = wsUpgrade("t=0123456789abcdef0123456789abcdef");
    r.upgrade = nullptr;
    TEST_ASSERT_EQUAL_UINT16(400, status(g, r));
    r.upgrade = "h2c";
    TEST_ASSERT_EQUAL_UINT16(400, status(g, r));
    GuardRequest api = get("/api/state");
    api.upgrade = "websocket";
    TEST_ASSERT_EQUAL_UINT16(400, status(g, api));
    GuardRequest p = post("/api/config");
    p.upgrade = "websocket";
    TEST_ASSERT_EQUAL_UINT16(400, status(g, p));
}

static void test_broken_cookie_and_authorization_headers() {
    RequestGuard g = guard(config());
    GuardRequest r = get("/api/state");
    // Other cookies of the same host are tolerated (a former device at this address).
    const char* const tolerated[] = {"a=b", "a=b; c=d", "a=b;c=d", "theme=dark mode; x=", "flag", "a=b;", "a=1;; b=2",
                                     "cyd_sid=\"0123456789abcdef0123456789abcdef\"",
                                     "x=1; cyd_sid=0123456789abcdef0123456789abcdef; y=2"};
    for (const char* c : tolerated) {
        r.cookie = c;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(200, status(g, r), c);
    }
    std::string many;
    for (int i = 0; i < 33; ++i) {
        char pair[16];
        snprintf(pair, sizeof(pair), "k%d=v; ", i);
        many += pair;
    }
    const char* const broken[] = {"a=\x01", "a=\xC3\xA4", "a=b\r\nX-Injected: 1", "cyd_sid=0123",
                                  "cyd_sid=0123456789ABCDEF0123456789ABCDEF", "cyd_sid=0123456789abcdef0123456789abcdeg",
                                  "cyd_sid=", "cyd_sid=0123456789abcdef0123456789abcdef; cyd_sid=0123456789abcdef0123456789abcdef",
                                  many.c_str()};
    for (const char* c : broken) {
        r.cookie = c;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(400, status(g, r), c);
    }
    TEST_ASSERT_EQUAL_STRING("Bad Cookie header", g.check(r, 0).reason);
    // Authorization: no Basic-Auth (W-E8); well-formed is ignored, broken refused.
    r = get("/api/state");
    r.authorization = "Basic dXNlcjpwYXNzd29yZA==";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    r.authorization = "Digest username=\"x\", realm=\"y\"";
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    std::string longAuth = "Bearer " + std::string(1100, 'a');
    const char* const badAuth[] = {"", "Basic", "Basic ", " Basic x", "Bas\x01ic x", "Basic \xC3\xA4", "B(asic x",
                                   longAuth.c_str()};
    for (const char* a : badAuth) {
        r.authorization = a;
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(400, status(g, r), a);
    }
    // With a password a Basic header grants nothing.
    RequestGuard locked = guard(config(true));
    GuardRequest p = post("/api/config");
    p.authorization = "Basic dXNlcjpwYXNzd29yZA==";
    TEST_ASSERT_EQUAL_UINT16(401, status(locked, p));
}

static std::string cookieOf(const char* setCookie) {
    const char* semi = strchr(setCookie, ';');
    return semi ? std::string(setCookie, (size_t)(semi - setCookie)) : std::string(setCookie);
}

static void test_login_scope() {
    // Default with a password: writes and the WiFi settings need a session; pages, state,
    // token and WebSocket do not (the state holds no secrets, ARBEITSPLAN 3.6).
    RequestGuard g = guard(config(true));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, get("/")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, get("/api/state")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, get("/api/token")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, wsUpgrade("t=0123456789abcdef0123456789abcdef")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, get("/api/wifi")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, get("/api/wifi/scan")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, get("/api/wifix")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, post("/api/channel/1")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, post("/api/save")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, post("/api/wifi/confirm")));
    TEST_ASSERT_EQUAL_STRING("Login required", g.check(post("/api/config"), 0).reason);
    TEST_ASSERT_EQUAL_UINT16(200, status(g, post("/api/login")));
    // Without a password nothing needs a session, loginForReads alone changes nothing.
    GuardConfig c = config(false);
    c.loginForReads = true;
    g.configure(c);
    TEST_ASSERT_EQUAL_UINT16(200, status(g, post("/api/channel/1")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, get("/api/wifi")));
}

static void test_sessions() {
    GuardConfig all = config(true);
    all.loginForReads = true;
    RequestGuard g = guard(all);
    // Without login: pages yes, API and WebSocket no, the login itself yes.
    TEST_ASSERT_EQUAL_UINT16(200, status(g, get("/")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, get("/app.js")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, get("/api/state")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, get("/api/token")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, post("/api/channel/1")));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, wsUpgrade("t=0123456789abcdef0123456789abcdef")));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, post("/api/login")));
    // The login still needs token and JSON (CSRF).
    GuardRequest login = post("/api/login");
    login.token = nullptr;
    TEST_ASSERT_EQUAL_UINT16(403, status(g, login));
    // Login: random 128-bit id, HttpOnly, SameSite=Strict, 12 h.
    char setCookie[RequestGuard::SET_COOKIE_SIZE];
    TEST_ASSERT_FALSE(g.loginSucceeded(1000, rnd, setCookie, 10));
    TEST_ASSERT_FALSE(g.loginSucceeded(1000, nullptr, setCookie, sizeof(setCookie)));
    TEST_ASSERT_TRUE(g.loginSucceeded(1000, rnd, setCookie, sizeof(setCookie)));
    TEST_ASSERT_EQUAL_STRING_LEN("cyd_sid=", setCookie, 8);
    TEST_ASSERT_NOT_NULL(strstr(setCookie, "; Path=/"));
    TEST_ASSERT_NOT_NULL(strstr(setCookie, "; Max-Age=43200"));
    TEST_ASSERT_NOT_NULL(strstr(setCookie, "; HttpOnly"));
    TEST_ASSERT_NOT_NULL(strstr(setCookie, "; SameSite=Strict"));
    const std::string c1 = cookieOf(setCookie);
    TEST_ASSERT_EQUAL(8 + 32, (int)c1.size());
    for (size_t i = 8; i < c1.size(); ++i) TEST_ASSERT_NOT_NULL(strchr("0123456789abcdef", c1[i]));
    GuardRequest r = get("/api/state");
    r.cookie = c1.c_str();
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r, 2000));
    TEST_ASSERT_TRUE(g.sessionValid(c1.c_str(), 2000));
    const std::string withOthers = "theme=dark; " + c1 + "; lang=en";
    r.cookie = withOthers.c_str();
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r, 2000));
    GuardRequest ws = wsUpgrade("t=0123456789abcdef0123456789abcdef");
    ws.cookie = c1.c_str();
    TEST_ASSERT_EQUAL_UINT16(200, status(g, ws, 2000));
    GuardRequest p = post("/api/config");
    p.cookie = c1.c_str();
    TEST_ASSERT_EQUAL_UINT16(200, status(g, p, 2000));
    // A guessed id: 401.
    std::string guessed = c1;
    guessed[guessed.size() - 1] = guessed[guessed.size() - 1] == '0' ? '1' : '0';
    r.cookie = guessed.c_str();
    TEST_ASSERT_EQUAL_UINT16(401, status(g, r, 2000));
    // 12 h after the login (not after the last use) the session ends.
    r.cookie = c1.c_str();
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r, 1000 + RequestGuard::SESSION_MS - 1));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, r, 1000 + RequestGuard::SESSION_MS));
    TEST_ASSERT_EQUAL_UINT8(0, g.sessionCount(1000 + RequestGuard::SESSION_MS));
    // At most MAX_SESSIONS: the least recently used one gives way.
    g.endAllSessions();
    std::string ids[5];
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_TRUE(g.loginSucceeded(10000 + (uint32_t)i, rnd, setCookie, sizeof(setCookie)));
        ids[i] = cookieOf(setCookie);
        if (i == 3) {
            // Session 0 is used again, so session 1 is now the least recently used one.
            r.cookie = ids[0].c_str();
            TEST_ASSERT_EQUAL_UINT16(200, status(g, r, 10003));
        }
    }
    TEST_ASSERT_EQUAL_UINT8(RequestGuard::MAX_SESSIONS, g.sessionCount(10010));
    TEST_ASSERT_TRUE(g.sessionValid(ids[0].c_str(), 10010));
    TEST_ASSERT_FALSE(g.sessionValid(ids[1].c_str(), 10010));
    for (int i = 2; i < 5; ++i) TEST_ASSERT_TRUE(g.sessionValid(ids[i].c_str(), 10010));
    for (int i = 0; i < 5; ++i) {
        for (int k = i + 1; k < 5; ++k) TEST_ASSERT_TRUE(ids[i] != ids[k]);
    }
    // Logout ends one, endAllSessions() (password changed) all.
    g.logout(ids[2].c_str());
    TEST_ASSERT_FALSE(g.sessionValid(ids[2].c_str(), 10010));
    TEST_ASSERT_TRUE(g.sessionValid(ids[3].c_str(), 10010));
    char clear[RequestGuard::SET_COOKIE_SIZE];
    RequestGuard::clearCookie(clear, sizeof(clear));
    TEST_ASSERT_EQUAL_STRING("cyd_sid=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict", clear);
    g.endAllSessions();
    TEST_ASSERT_EQUAL_UINT8(0, g.sessionCount(10010));
    // Without a password no session is needed.
    RequestGuard open = guard(config(false));
    TEST_ASSERT_EQUAL_UINT16(200, status(open, get("/api/state")));
}

static void test_login_lock_doubles() {
    RequestGuard g = guard(config(true));
    uint32_t t = 5000;
    TEST_ASSERT_TRUE(g.loginAllowed(t).allowed());
    for (int i = 0; i < 4; ++i) g.loginFailed(t);
    TEST_ASSERT_TRUE(g.loginAllowed(t).allowed());
    g.loginFailed(t); // 5th: locked for 30 s
    TEST_ASSERT_EQUAL_UINT16(429, g.loginAllowed(t).status());
    TEST_ASSERT_EQUAL_STRING("Login locked", g.loginAllowed(t).reason);
    TEST_ASSERT_EQUAL_UINT32(30, g.loginRetryAfterS(t));
    TEST_ASSERT_EQUAL_UINT32(1, g.loginRetryAfterS(t + 29001));
    TEST_ASSERT_EQUAL_UINT16(429, g.loginAllowed(t + 29999).status());
    // Failures during the lock (a check that was already running) do not count.
    for (int i = 0; i < 10; ++i) g.loginFailed(t + 1000);
    TEST_ASSERT_TRUE(g.loginAllowed(t + 30000).allowed());
    TEST_ASSERT_EQUAL_UINT32(0, g.loginRetryAfterS(t + 30000));
    // Every further lock doubles: 60, 120, 240, 480, 960 s, then it stays at 960 s.
    const uint32_t expected[] = {60, 120, 240, 480, 960, 960, 960};
    t += 30000;
    for (uint32_t lockS : expected) {
        for (int i = 0; i < 5; ++i) g.loginFailed(t);
        TEST_ASSERT_EQUAL_UINT32(lockS, g.loginRetryAfterS(t));
        TEST_ASSERT_EQUAL_UINT16(429, g.loginAllowed(t + lockS * 1000 - 1).status());
        t += lockS * 1000;
        TEST_ASSERT_TRUE(g.loginAllowed(t).allowed());
    }
    // A correct password resets the level.
    char setCookie[RequestGuard::SET_COOKIE_SIZE];
    TEST_ASSERT_TRUE(g.loginSucceeded(t, rnd, setCookie, sizeof(setCookie)));
    for (int i = 0; i < 5; ++i) g.loginFailed(t);
    TEST_ASSERT_EQUAL_UINT32(30, g.loginRetryAfterS(t));
    // So does an hour without failure.
    t += 30000;
    for (int i = 0; i < 5; ++i) g.loginFailed(t);
    TEST_ASSERT_EQUAL_UINT32(60, g.loginRetryAfterS(t));
    t += RequestGuard::LOGIN_FORGET_MS;
    for (int i = 0; i < 4; ++i) g.loginFailed(t);
    TEST_ASSERT_TRUE(g.loginAllowed(t).allowed());
    g.loginFailed(t);
    TEST_ASSERT_EQUAL_UINT32(30, g.loginRetryAfterS(t));
    // Across 2^32 ms (64-bit time).
    RequestGuard w = guard(config(true));
    const uint64_t nearWrap = 0xFFFFFFFFull - 10000;
    for (int i = 0; i < 5; ++i) w.loginFailed(nearWrap);
    TEST_ASSERT_EQUAL_UINT16(429, w.loginAllowed(nearWrap + 25000).status());
    TEST_ASSERT_TRUE(w.loginAllowed(nearWrap + 30000).allowed());
}

// Fuzz finding S57c: with 32-bit time an expired session, login lock or scan stamp came back
// 2^32 ms (49.7 days) later. The guard takes 64-bit monotonic milliseconds.
static void test_nothing_comes_back_after_49_days() {
    const uint64_t WRAP = 0x100000000ull;
    RequestGuard g = guard(config(true));
    char setCookie[RequestGuard::SET_COOKIE_SIZE];
    TEST_ASSERT_TRUE(g.loginSucceeded(1000, rnd, setCookie, sizeof(setCookie)));
    const std::string c = cookieOf(setCookie);
    GuardRequest r = post("/api/config");
    r.cookie = c.c_str();
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r, 2000));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, r, 1000 + RequestGuard::SESSION_MS));
    TEST_ASSERT_EQUAL_UINT16(401, status(g, r, WRAP + 2000));
    TEST_ASSERT_FALSE(g.sessionValid(c.c_str(), WRAP + 2000));
    TEST_ASSERT_EQUAL_UINT8(0, g.sessionCount(WRAP + 2000));
    // A login lock of 30 s is over, also 2^32 ms later.
    for (int i = 0; i < 5; ++i) g.loginFailed(5000);
    TEST_ASSERT_EQUAL_UINT16(429, g.loginAllowed(6000).status());
    TEST_ASSERT_TRUE(g.loginAllowed(WRAP + 6000).allowed());
    TEST_ASSERT_EQUAL_UINT32(0, g.loginRetryAfterS(WRAP + 6000));
    // The scan interval as well.
    TEST_ASSERT_TRUE(g.scanAllowed(100).allowed());
    TEST_ASSERT_TRUE(g.scanAllowed(WRAP + 200).allowed());
}

static void test_scan_at_most_every_10_s() {
    RequestGuard g = guard(config());
    TEST_ASSERT_TRUE(g.scanAllowed(0).allowed());
    TEST_ASSERT_EQUAL_UINT16(429, g.scanAllowed(1).status());
    TEST_ASSERT_EQUAL_UINT16(429, g.scanAllowed(9999).status());
    TEST_ASSERT_TRUE(g.scanAllowed(10000).allowed());
    TEST_ASSERT_EQUAL_UINT16(429, g.scanAllowed(15000).status()); // refused tries do not count
    TEST_ASSERT_TRUE(g.scanAllowed(20000).allowed());
}

static void test_no_passwords_over_an_open_ap() {
    RequestGuard g = guard(apConfig(true));
    GuardRequest r = post("/api/wifi", "192.168.4.1");
    r.origin = nullptr;
    r.localIp = AP;
    r.clientIp = AP + 2;
    TEST_ASSERT_TRUE(g.overOpenAp(r));
    TEST_ASSERT_EQUAL_UINT16(200, status(g, r));
    TEST_ASSERT_EQUAL_UINT16(403, g.checkSecrets(r, true).status());
    TEST_ASSERT_TRUE(g.checkSecrets(r, false).allowed());
    // Secured AP or the client network: allowed.
    g.configure(apConfig(false));
    TEST_ASSERT_FALSE(g.overOpenAp(r));
    TEST_ASSERT_TRUE(g.checkSecrets(r, true).allowed());
    GuardConfig c = apConfig(true);
    c.staIp = STA;
    g.configure(c);
    GuardRequest sta = post("/api/wifi");
    TEST_ASSERT_FALSE(g.overOpenAp(sta));
    TEST_ASSERT_TRUE(g.checkSecrets(sta, true).allowed());
    // AP off: never.
    g.configure(config());
    TEST_ASSERT_FALSE(g.overAp(r));
}

static void test_tokens() {
    char a[RequestGuard::TOKEN_LEN + 1];
    char b[RequestGuard::TOKEN_LEN + 1];
    RequestGuard::makeToken(rnd, a);
    RequestGuard::makeToken(rnd, b);
    TEST_ASSERT_EQUAL(32, (int)strlen(a));
    for (size_t i = 0; i < 32; ++i) TEST_ASSERT_NOT_NULL(strchr("0123456789abcdef", a[i]));
    TEST_ASSERT_TRUE(strcmp(a, b) != 0);
    // The generated token works for POST and WebSocket.
    GuardConfig c = config();
    c.token = a;
    RequestGuard g = guard(c);
    GuardRequest p = post("/api/save");
    p.token = a;
    TEST_ASSERT_EQUAL_UINT16(200, status(g, p));
    p.token = b;
    TEST_ASSERT_EQUAL_UINT16(403, status(g, p));
    std::string q = std::string("t=") + a;
    TEST_ASSERT_EQUAL_UINT16(200, status(g, wsUpgrade(q.c_str())));
}

static std::string hex(const uint8_t* d, size_t n) {
    static const char H[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += H[d[i] >> 4];
        s += H[d[i] & 15];
    }
    return s;
}

static bool verify(const char* pw, const char* record) { return WebPassword::verify(pw, strlen(pw), record); }

static void test_web_password_hash() {
    using namespace WebPassword;
    TEST_ASSERT_FALSE(valid("1234567", 7));
    TEST_ASSERT_TRUE(valid("12345678", 8));
    const std::string max(64, 'x');
    TEST_ASSERT_TRUE(valid(max.c_str(), 64));
    TEST_ASSERT_FALSE(valid((max + "x").c_str(), 65));
    TEST_ASSERT_TRUE(valid("p\xC3\xA4ssw\xC3\xB6rd", 10)); // UTF-8 from a phone keyboard
    TEST_ASSERT_FALSE(valid("pass\nword", 9));
    TEST_ASSERT_FALSE(valid("pass\x7Fword", 9));
    TEST_ASSERT_FALSE(valid("passw\xC3rd", 9)); // broken UTF-8
    TEST_ASSERT_FALSE(valid(nullptr, 8));
    char record[RECORD_SIZE];
    TEST_ASSERT_FALSE(hash("short", 5, rnd, record));
    TEST_ASSERT_EQUAL_STRING("", record);
    TEST_ASSERT_FALSE(hash("correct horse", 13, nullptr, record));
    TEST_ASSERT_TRUE(hash("correct horse", 13, rnd, record));
    // "pbkdf2-sha1$10000$<salt 32 hex>$<hash 40 hex>"
    TEST_ASSERT_EQUAL(12 + 5 + 1 + 32 + 1 + 40, (int)strlen(record));
    TEST_ASSERT_EQUAL_STRING_LEN("pbkdf2-sha1$10000$", record, 18);
    TEST_ASSERT_EQUAL_CHAR('$', record[18 + 32]);
    // Checked independently: PBKDF2 over the stored salt gives the stored hash.
    uint8_t salt[16];
    for (int i = 0; i < 16; ++i) {
        unsigned v = 0;
        sscanf(record + 18 + 2 * i, "%2x", &v);
        salt[i] = (uint8_t)v;
    }
    uint8_t dk[20];
    Crypto::pbkdf2HmacSha1((const uint8_t*)"correct horse", 13, salt, 16, 10000, dk, 20);
    TEST_ASSERT_EQUAL_STRING(hex(dk, 20).c_str(), record + 18 + 32 + 1);
    TEST_ASSERT_TRUE(verify("correct horse", record));
    TEST_ASSERT_FALSE(verify("correct horsE", record));
    TEST_ASSERT_FALSE(verify("correct hors", record));
    TEST_ASSERT_FALSE(verify("", record));
    // Random salt: the same password gives another record.
    char again[RECORD_SIZE];
    TEST_ASSERT_TRUE(hash("correct horse", 13, rnd, again));
    TEST_ASSERT_TRUE(strcmp(record, again) != 0);
    TEST_ASSERT_TRUE(verify("correct horse", again));
    // A record with another iteration count (raised later) still verifies.
    char other[RECORD_SIZE];
    Crypto::pbkdf2HmacSha1((const uint8_t*)"correct horse", 13, salt, 16, 2000, dk, 20);
    snprintf(other, sizeof(other), "pbkdf2-sha1$2000$%s$%s", hex(salt, 16).c_str(), hex(dk, 20).c_str());
    TEST_ASSERT_TRUE(verify("correct horse", other));
    // Malformed or out-of-bounds records never verify (and never run for long).
    const std::string s = hex(salt, 16);
    const std::string h = hex(dk, 20);
    const std::string bad[] = {
        "", "pbkdf2-sha1$", "pbkdf2-sha256$2000$" + s + "$" + h, "pbkdf2-sha1$999$" + s + "$" + h,
        "pbkdf2-sha1$100001$" + s + "$" + h, "pbkdf2-sha1$4294967295$" + s + "$" + h, "pbkdf2-sha1$$" + s + "$" + h,
        "pbkdf2-sha1$2000$" + s.substr(2) + "$" + h, "pbkdf2-sha1$2000$" + s + "$" + h + "00",
        "pbkdf2-sha1$2000$" + s + "$" + h.substr(0, 38) + "AB", "pbkdf2-sha1$2000" + s + "$" + h,
        "pbkdf2-sha1$2000$" + s + h};
    for (const std::string& b : bad) TEST_ASSERT_FALSE_MESSAGE(verify("correct horse", b.c_str()), b.c_str());
    TEST_ASSERT_FALSE(WebPassword::verify("correct horse", 13, nullptr));
    TEST_ASSERT_FALSE(WebPassword::verify(max.c_str(), 65, record));
    TEST_ASSERT_TRUE(constantTimeEqual("abc", "abc", 3));
    TEST_ASSERT_FALSE(constantTimeEqual("abc", "abd", 3));
    TEST_ASSERT_TRUE(constantTimeEqual("x", "y", 0));
}

// Random requests with a fixed seed (W-S25; fuzzing proper in S57): every decision is one of
// the documented ones, and an allowed request always passed the rules it is meant to pass.
static const char* pick(const char* const* list, size_t n) { return list[rnd() % n]; }

static std::string randomBytes(size_t maxLen) {
    std::string s;
    const size_t n = rnd() % (maxLen + 1);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t x = rnd();
        char c = (char)(x >> 8);
        if (x % 3) c = (char)(' ' + (x >> 16) % 95);
        if (c == '\0') c = 'x';
        s += c;
    }
    return s;
}

static void test_random_requests_keep_the_rules() {
    static const char* const methods[] = {"GET", "HEAD", "POST", "OPTIONS", "PUT", "get", "", "POST "};
    static const char* const paths[] = {"/", "/api/state", "/api/config", "/api/login", "/api/token", "/ws", "/ws/",
                                        "/api/wifi/scan", "/apix", "api/state", "/app.js"};
    static const char* const hosts[] = {"192.168.1.50", "cyd-hf-1a2b.local", "cyd-hf-1a2b:80", "evil.example",
                                        "192.168.4.1", "[::1]", "", "192.168.1.50:99999"};
    static const char* const origins[] = {"http://192.168.1.50", "http://evil.example", "null", "", "http://cyd-hf-1a2b.local"};
    static const char* const sites[] = {"same-origin", "cross-site", "same-site", "none", "CROSS-SITE"};
    static const char* const types[] = {"application/json", "text/plain", "application/json; charset=utf-8",
                                        "multipart/form-data", ""};
    static const char* const lengths[] = {"2", "1024", "1025", "-1", "", "abc", "0"};
    static const char* const tokens[] = {"0123456789abcdef0123456789abcdef", "0123456789abcdef0123456789abcdee", "", "x"};
    static const char* const upgrades[] = {"websocket", "h2c", "WebSocket", ""};
    static const char* const queries[] = {"t=0123456789abcdef0123456789abcdef", "t=bad", "", "x=1&t=0123456789abcdef0123456789abcdef"};
    static const uint16_t known[] = {200, 302, 400, 401, 403, 405, 411, 413, 414, 415, 421, 429, 431, 503};

    unsigned allowed = 0;
    unsigned sessions = 0;
    for (int round = 0; round < 100000; ++round) {
        const bool password = rnd() % 2;
        GuardConfig c = (rnd() % 4 == 0) ? apConfig(rnd() % 2) : config(password);
        c.passwordSet = password;
        c.loginForReads = rnd() % 2;
        RequestGuard g;
        g.configure(c);
        std::string cookie;
        char setCookie[RequestGuard::SET_COOKIE_SIZE];
        if (rnd() % 2 && g.loginSucceeded(0, rnd, setCookie, sizeof(setCookie))) cookie = cookieOf(setCookie);
        std::string junk[6];
        for (std::string& j : junk) j = randomBytes(40);
        // A valid request of a random kind (page, API read, API write, WebSocket), then each
        // field replaced with probability 1/8 by an interesting value, junk or nothing.
        const bool overAp = c.staIp == 0;
        const char* ownHost = overAp ? "192.168.4.1" : "192.168.1.50";
        const uint32_t kind = rnd() % 4;
        GuardRequest r = kind == 0 ? get("/", ownHost)
                       : kind == 1 ? get("/api/state", ownHost)
                       : kind == 2 ? post("/api/config", ownHost)
                                   : wsUpgrade("t=0123456789abcdef0123456789abcdef");
        r.host = ownHost;
        r.origin = nullptr;
        if (overAp) r.localIp = AP;
        if (!cookie.empty()) r.cookie = cookie.c_str();
#define MUTATE(field, value) \
    if (rnd() % 8 == 0) field = (value)
        MUTATE(r.method, rnd() % 8 ? pick(methods, 8) : junk[0].c_str());
        MUTATE(r.path, rnd() % 8 ? pick(paths, 11) : junk[1].c_str());
        MUTATE(r.query, rnd() % 2 ? pick(queries, 4) : nullptr);
        MUTATE(r.host, rnd() % 4 ? pick(hosts, 8) : junk[2].c_str());
        MUTATE(r.origin, rnd() % 4 ? pick(origins, 5) : junk[3].c_str());
        MUTATE(r.secFetchSite, rnd() % 4 ? pick(sites, 5) : nullptr);
        MUTATE(r.contentType, rnd() % 4 ? pick(types, 5) : nullptr);
        MUTATE(r.contentLength, rnd() % 4 ? pick(lengths, 7) : nullptr);
        MUTATE(r.transferEncoding, "chunked");
        MUTATE(r.token, rnd() % 4 ? pick(tokens, 4) : nullptr);
        MUTATE(r.cookie, rnd() % 2 ? junk[4].c_str() : nullptr);
        MUTATE(r.authorization, junk[5].c_str());
        MUTATE(r.upgrade, rnd() % 4 ? pick(upgrades, 4) : nullptr);
        MUTATE(r.headerBytes, rnd() % 2200);
        MUTATE(r.clientIp, rnd() % 2 ? PEER : AP + 3);
        MUTATE(r.localIp, rnd() % 2 ? STA : AP);
        MUTATE(r.freeHeap, 20000 + rnd() % 20000);
#undef MUTATE

        const GuardDecision d = g.check(r, 1000);
        bool isKnown = false;
        for (uint16_t k : known) isKnown = isKnown || d.status() == k;
        TEST_ASSERT_TRUE(isKnown);
        TEST_ASSERT_NOT_NULL(d.reason);
        TEST_ASSERT_TRUE(d.reason[0] != '\0');
        if (!d.allowed()) continue;
        ++allowed;
        const bool post = strcmp(r.method, "POST") == 0;
        const bool api = strncmp(r.path, "/api/", 5) == 0;
        const bool ws = strcmp(r.path, "/ws") == 0;
        TEST_ASSERT_TRUE(post || strcmp(r.method, "GET") == 0 || strcmp(r.method, "HEAD") == 0);
        TEST_ASSERT_TRUE(r.freeHeap >= RequestGuard::MIN_FREE_HEAP);
        TEST_ASSERT_TRUE(r.headerBytes <= RequestGuard::MAX_HEADERS);
        TEST_ASSERT_NULL(r.transferEncoding);
        TEST_ASSERT_TRUE(strcmp(r.host, "evil.example") != 0 && strcmp(r.host, "[::1]") != 0);
        if (post) {
            TEST_ASSERT_TRUE(api);
            TEST_ASSERT_EQUAL_STRING(TOKEN, r.token);
            TEST_ASSERT_NOT_NULL(r.contentLength);
        }
        if (api || ws) {
            TEST_ASSERT_TRUE(!r.secFetchSite || strcmp(r.secFetchSite, "cross-site") != 0);
            TEST_ASSERT_TRUE(!r.origin || strcmp(r.origin, "http://evil.example") != 0);
        }
        if (ws) TEST_ASSERT_TRUE(r.query && strstr(r.query, "t=0123456789abcdef0123456789abcdef"));
        const bool wifi = strncmp(r.path, "/api/wifi", 9) == 0 && (r.path[9] == '\0' || r.path[9] == '/');
        const bool needed = post || wifi || (c.loginForReads && (api || ws));
        if (password && needed && !(post && strcmp(r.path, "/api/login") == 0)) {
            TEST_ASSERT_TRUE(g.sessionValid(r.cookie, 1000));
            ++sessions;
        }
    }
    // The generator reaches the interesting cases.
    TEST_ASSERT_TRUE(allowed > 2000);
    TEST_ASSERT_TRUE(sessions > 100);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_host_allowlist);
    RUN_TEST(test_captive_portal_only_for_pages_over_the_ap);
    RUN_TEST(test_methods_and_no_cors);
    RUN_TEST(test_post_needs_json_token_and_own_origin);
    RUN_TEST(test_token_endpoint_is_same_origin_only);
    RUN_TEST(test_size_and_heap_limits);
    RUN_TEST(test_websocket_upgrade);
    RUN_TEST(test_broken_cookie_and_authorization_headers);
    RUN_TEST(test_login_scope);
    RUN_TEST(test_sessions);
    RUN_TEST(test_login_lock_doubles);
    RUN_TEST(test_scan_at_most_every_10_s);
    RUN_TEST(test_nothing_comes_back_after_49_days);
    RUN_TEST(test_no_passwords_over_an_open_ap);
    RUN_TEST(test_tokens);
    RUN_TEST(test_web_password_hash);
    RUN_TEST(test_random_requests_keep_the_rules);
    return UNITY_END();
}
