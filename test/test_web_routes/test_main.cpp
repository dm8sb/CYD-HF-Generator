// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Routes and body assembly of the web server (AF32, TEST-1, CQ-6): every path of the REST
// contract (data/www/README.md) has its route, unknown paths and the other method give
// NotFound, RequestGuard sees the same path classes, and a body arrives in chunks as
// ESPAsyncWebServer hands it over.

#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <net/request_guard.h>
#include <net/web_limits.h>
#include <net/web_routes.h>
#include <remote/rest_json.h>

using WebRoutes::Kind;

void setUp() {}
void tearDown() {}

// The REST contract: method, path, route.
struct Expected {
    const char* method;
    const char* path;
    Kind kind;
};
static const Expected CONTRACT[] = {
    {"GET", "/", Kind::Page},
    {"GET", "/index.html", Kind::Page},
    {"HEAD", "/", Kind::Page},
    {"GET", "/api/state", Kind::State},
    {"HEAD", "/api/state", Kind::State},
    {"GET", "/lxi/identification", Kind::Lxi},
    {"GET", "/api/token", Kind::Token},
    {"GET", "/api/wifi", Kind::WifiSettings},
    {"GET", "/api/wifi/scan", Kind::Scan},
    {"GET", "/ws", Kind::WebSocket},
    {"POST", "/api/channel/1", Kind::Channel1},
    {"POST", "/api/channel/2", Kind::Channel2},
    {"POST", "/api/ch3", Kind::Ch3},
    {"POST", "/api/config", Kind::Config},
    {"POST", "/api/save", Kind::Save},
    {"POST", "/api/identify", Kind::Identify},
    {"POST", "/api/login", Kind::Login},
    {"POST", "/api/logout", Kind::Logout},
    {"POST", "/api/wifi", Kind::WifiChange},
    {"POST", "/api/wifi/confirm", Kind::WifiConfirm},
};

static void test_every_contract_path_has_its_route() {
    for (const Expected& e : CONTRACT) {
        TEST_ASSERT_EQUAL_MESSAGE((int)e.kind, (int)WebRoutes::find(e.method, e.path), e.path);
    }
    // The table holds exactly these (HEAD shares the GET entries).
    size_t n = 0;
    const WebRoutes::Route* t = WebRoutes::table(&n);
    TEST_ASSERT_EQUAL_size_t(18, n);
    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_MESSAGE((int)t[i].kind, (int)WebRoutes::find(t[i].post ? "POST" : "GET", t[i].path), t[i].path);
    }
    TEST_ASSERT_EQUAL_UINT8(1, WebRoutes::channelOf(Kind::Channel1));
    TEST_ASSERT_EQUAL_UINT8(2, WebRoutes::channelOf(Kind::Channel2));
    TEST_ASSERT_EQUAL_UINT8(0, WebRoutes::channelOf(Kind::Ch3));
}

// Every "`GET /path`" and "`POST /path`" of the REST contract in data/www/README.md is routed
// (pio test runs in the project directory); "<1\|2>" stands for both channels, a '*' for a
// family of paths.
static void test_contract_document_matches_the_table() {
    FILE* f = fopen("data/www/README.md", "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "data/www/README.md not found");
    std::string doc;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) doc.append(buf, n);
    fclose(f);
    unsigned checked = 0;
    for (size_t pos = doc.find('`'); pos != std::string::npos; pos = doc.find('`', pos + 1)) {
        const size_t end = doc.find('`', pos + 1);
        if (end == std::string::npos) break;
        const std::string code = doc.substr(pos + 1, end - pos - 1);
        pos = end;
        const bool get = code.compare(0, 5, "GET /") == 0;
        const bool post = code.compare(0, 6, "POST /") == 0;
        if ((!get && !post) || code.find('*') != std::string::npos || code.find(' ', get ? 4 : 5) != std::string::npos) continue;
        std::string path = code.substr(get ? 4 : 5);
        const size_t alt = path.find("<1\\|2>");
        const char* method = get ? "GET" : "POST";
        if (alt != std::string::npos) {
            for (const char* ch : {"1", "2"}) {
                const std::string p = path.substr(0, alt) + ch;
                TEST_ASSERT_NOT_EQUAL_MESSAGE((int)Kind::NotFound, (int)WebRoutes::find(method, p.c_str()), p.c_str());
                ++checked;
            }
            continue;
        }
        TEST_ASSERT_NOT_EQUAL_MESSAGE((int)Kind::NotFound, (int)WebRoutes::find(method, path.c_str()), code.c_str());
        ++checked;
    }
    TEST_ASSERT_TRUE(checked >= 16);
}

// 404: unknown paths, the other method, near misses (case, trailing slash, query in the path).
static void test_unknown_paths_and_methods() {
    const Expected notFound[] = {
        {"GET", "/api/config", Kind::NotFound},     // a POST path
        {"GET", "/api/channel/1", Kind::NotFound},
        {"GET", "/api/login", Kind::NotFound},
        {"POST", "/api/state", Kind::NotFound},     // a GET path
        {"POST", "/api/wifi/scan", Kind::NotFound},
        {"POST", "/", Kind::NotFound},
        {"POST", "/ws", Kind::NotFound},
        {"GET", "/api/channel/3", Kind::NotFound},
        {"POST", "/api/channel/3", Kind::NotFound},
        {"POST", "/api/channel/01", Kind::NotFound},
        {"GET", "/API/STATE", Kind::NotFound},
        {"GET", "/api/state/", Kind::NotFound},
        {"GET", "/index.htm", Kind::NotFound},
        {"GET", "/api/state?x=1", Kind::NotFound},
        {"GET", "", Kind::NotFound},
        {"PUT", "/api/config", Kind::NotFound},
        {"OPTIONS", "/api/config", Kind::NotFound},
    };
    for (const Expected& e : notFound) {
        TEST_ASSERT_EQUAL_MESSAGE((int)Kind::NotFound, (int)WebRoutes::find(e.method, e.path), e.path);
    }
    TEST_ASSERT_EQUAL((int)Kind::NotFound, (int)WebRoutes::find(nullptr, "/"));
    TEST_ASSERT_EQUAL((int)Kind::NotFound, (int)WebRoutes::find("GET", nullptr));
}

// The guard's path classes come from the same module.
static void test_path_classes() {
    TEST_ASSERT_TRUE(WebRoutes::isApi("/api/state"));
    TEST_ASSERT_TRUE(WebRoutes::isApi("/api/"));
    TEST_ASSERT_FALSE(WebRoutes::isApi("/api"));
    TEST_ASSERT_FALSE(WebRoutes::isApi("/apix/state"));
    TEST_ASSERT_TRUE(WebRoutes::isWs("/ws"));
    TEST_ASSERT_FALSE(WebRoutes::isWs("/ws/"));
    TEST_ASSERT_TRUE(WebRoutes::isWifi("/api/wifi"));
    TEST_ASSERT_TRUE(WebRoutes::isWifi("/api/wifi/scan"));
    TEST_ASSERT_TRUE(WebRoutes::isWifi("/api/wifi/confirm"));
    TEST_ASSERT_FALSE(WebRoutes::isWifi("/api/wifix"));
    TEST_ASSERT_TRUE(WebRoutes::sessionFree(true, "/api/login"));
    TEST_ASSERT_TRUE(WebRoutes::sessionFree(true, "/api/logout"));
    TEST_ASSERT_FALSE(WebRoutes::sessionFree(false, "/api/login"));
    TEST_ASSERT_FALSE(WebRoutes::sessionFree(true, "/api/wifi"));
    // Every POST route is under /api/ (the guard allows POST only there), every route needing
    // the WiFi session is a WiFi path.
    size_t n = 0;
    const WebRoutes::Route* t = WebRoutes::table(&n);
    for (size_t i = 0; i < n; ++i) {
        if (t[i].post) TEST_ASSERT_TRUE_MESSAGE(WebRoutes::isApi(t[i].path), t[i].path);
        const bool wifi = t[i].kind == Kind::WifiSettings || t[i].kind == Kind::Scan || t[i].kind == Kind::WifiChange ||
                          t[i].kind == Kind::WifiConfirm;
        TEST_ASSERT_EQUAL_MESSAGE(wifi, WebRoutes::isWifi(t[i].path), t[i].path);
    }
}

// The fourth WebSocket gets no place: the server answers 503 from WsClients::add.
static void test_fourth_websocket_is_refused() {
    TEST_ASSERT_EQUAL((int)Kind::WebSocket, (int)WebRoutes::find("GET", "/ws"));
    WsClients c;
    for (uint32_t id = 1; id <= WsClients::MAX; ++id) TEST_ASSERT_TRUE(c.add(id, 0));
    TEST_ASSERT_EQUAL_UINT8(3, c.count());
    TEST_ASSERT_FALSE(c.add(4, 0));
    c.remove(2);
    TEST_ASSERT_TRUE(c.add(4, 0));
}

// --- body assembly --------------------------------------------------------------------------

static bool feed(void** slot, const char* data, size_t len, size_t index, size_t total) {
    return WebBody::chunk(slot, (const uint8_t*)data, len, index, total, RequestGuard::MAX_BODY);
}

static void test_body_in_three_parts() {
    void* slot = nullptr;
    const char* body = "{\"frequency\":7100000}";
    const size_t total = strlen(body);
    TEST_ASSERT_TRUE(feed(&slot, body, 5, 0, total));
    TEST_ASSERT_TRUE(feed(&slot, body + 5, 10, 5, total));
    TEST_ASSERT_EQUAL_size_t(15, WebBody::length(slot));
    TEST_ASSERT_TRUE(feed(&slot, body + 15, total - 15, 15, total));
    TEST_ASSERT_EQUAL_STRING(body, WebBody::text(slot));
    TEST_ASSERT_EQUAL_size_t(total, WebBody::length(slot));
    // A new body (index 0) replaces the old one.
    TEST_ASSERT_TRUE(feed(&slot, "{}", 2, 0, 2));
    TEST_ASSERT_EQUAL_STRING("{}", WebBody::text(slot));
    TEST_ASSERT_EQUAL_size_t(2, WebBody::length(slot));
    free(slot); // the library frees _tempObject with free()
    TEST_ASSERT_NULL(WebBody::text(nullptr));
    TEST_ASSERT_EQUAL_size_t(0, WebBody::length(nullptr));
}

static void test_body_limits() {
    void* slot = nullptr;
    char big[RequestGuard::MAX_BODY + 1];
    memset(big, 'a', sizeof(big));
    // Above MAX_BODY: not kept (the guard answers 413).
    TEST_ASSERT_FALSE(feed(&slot, big, sizeof(big), 0, sizeof(big)));
    TEST_ASSERT_NULL(slot);
    TEST_ASSERT_TRUE(feed(&slot, big, RequestGuard::MAX_BODY, 0, RequestGuard::MAX_BODY));
    TEST_ASSERT_EQUAL_size_t(RequestGuard::MAX_BODY, WebBody::length(slot));
    // index + len > total, a gap, another total: refused, the kept bytes stay.
    TEST_ASSERT_TRUE(feed(&slot, "abc", 3, 0, 6));
    TEST_ASSERT_FALSE(feed(&slot, "defg", 4, 3, 6));
    TEST_ASSERT_FALSE(feed(&slot, "e", 1, 4, 6));
    TEST_ASSERT_FALSE(feed(&slot, "d", 1, 3, 7));
    TEST_ASSERT_FALSE(feed(&slot, "d", 1, (size_t)-1, 6));
    TEST_ASSERT_EQUAL_size_t(3, WebBody::length(slot));
    TEST_ASSERT_EQUAL_STRING("abc", WebBody::text(slot));
    TEST_ASSERT_TRUE(feed(&slot, "def", 3, 3, 6));
    TEST_ASSERT_EQUAL_STRING("abcdef", WebBody::text(slot));
    free(slot);
    // A chunk without the first one is not kept.
    slot = nullptr;
    TEST_ASSERT_FALSE(feed(&slot, "def", 3, 3, 6));
    TEST_ASSERT_NULL(slot);
    TEST_ASSERT_FALSE(WebBody::chunk(nullptr, (const uint8_t*)"x", 1, 0, 1, 16));
}

// Embedded NUL: the bytes after it are kept and counted, and wipe() clears all of them.
static void test_body_with_embedded_nul() {
    void* slot = nullptr;
    const char body[] = "{\"password\":\"a\0secret\"}";
    const size_t total = sizeof(body) - 1;
    TEST_ASSERT_TRUE(feed(&slot, body, total, 0, total));
    TEST_ASSERT_EQUAL_size_t(total, WebBody::length(slot));
    TEST_ASSERT_EQUAL_MEMORY(body, WebBody::text(slot), total);
    TEST_ASSERT_EQUAL_CHAR('\0', WebBody::text(slot)[total]);
    WebBody::wipe(slot);
    for (size_t i = 0; i < total; ++i) TEST_ASSERT_EQUAL_CHAR('\0', WebBody::text(slot)[i]);
    free(slot);
    // With its full length the parser refuses it (-102); cut at the NUL by strlen it was a
    // valid body before AF32.
    const char config[] = "{\"backlight\":50}\0{\"x\":1}";
    slot = nullptr;
    TEST_ASSERT_TRUE(feed(&slot, config, sizeof(config) - 1, 0, sizeof(config) - 1));
    RestJson::Commands cmds {};
    const RestJson::Reply r = RestJson::parseConfig(WebBody::text(slot), WebBody::length(slot), &cmds);
    TEST_ASSERT_EQUAL_UINT16(400, r.status);
    TEST_ASSERT_EQUAL_INT16(-102, r.code);
    TEST_ASSERT_TRUE(RestJson::parseConfig(WebBody::text(slot), strlen(WebBody::text(slot)), &cmds).ok());
    free(slot);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_contract_path_has_its_route);
    RUN_TEST(test_contract_document_matches_the_table);
    RUN_TEST(test_unknown_paths_and_methods);
    RUN_TEST(test_path_classes);
    RUN_TEST(test_fourth_websocket_is_refused);
    RUN_TEST(test_body_in_three_parts);
    RUN_TEST(test_body_limits);
    RUN_TEST(test_body_with_embedded_nul);
    return UNITY_END();
}
