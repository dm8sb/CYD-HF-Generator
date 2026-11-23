// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "web_routes.h"

#include <stdlib.h>
#include <string.h>

namespace WebRoutes {

namespace {

// data/www/README.md lists the same paths (the host test compares them).
const Route ROUTES[] = {
    {false, "/", Kind::Page},
    {false, "/index.html", Kind::Page},
    {false, "/api/state", Kind::State},
    {false, "/lxi/identification", Kind::Lxi},
    {false, "/api/token", Kind::Token},
    {false, "/api/wifi", Kind::WifiSettings},
    {false, "/api/wifi/scan", Kind::Scan},
    {false, "/ws", Kind::WebSocket},
    {true, "/api/channel/1", Kind::Channel1},
    {true, "/api/channel/2", Kind::Channel2},
    {true, "/api/ch3", Kind::Ch3},
    {true, "/api/config", Kind::Config},
    {true, "/api/save", Kind::Save},
    {true, "/api/identify", Kind::Identify},
    {true, "/api/login", Kind::Login},
    {true, "/api/logout", Kind::Logout},
    {true, "/api/wifi", Kind::WifiChange},
    {true, "/api/wifi/confirm", Kind::WifiConfirm},
};

bool startsWith(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

} // namespace

const Route* table(size_t* count) {
    if (count) *count = sizeof(ROUTES) / sizeof(ROUTES[0]);
    return ROUTES;
}

Kind find(const char* method, const char* path) {
    if (!method || !path) return Kind::NotFound;
    const bool post = strcmp(method, "POST") == 0;
    for (const Route& r : ROUTES) {
        if (r.post == post && strcmp(r.path, path) == 0) return r.kind;
    }
    return Kind::NotFound;
}

uint8_t channelOf(Kind k) { return k == Kind::Channel1 ? 1 : k == Kind::Channel2 ? 2 : 0; }

bool isApi(const char* path) { return startsWith(path, "/api/"); }
bool isWs(const char* path) { return strcmp(path, "/ws") == 0; }
bool isWifi(const char* path) { return strcmp(path, "/api/wifi") == 0 || startsWith(path, "/api/wifi/"); }
bool sessionFree(bool post, const char* path) {
    return post && (strcmp(path, "/api/login") == 0 || strcmp(path, "/api/logout") == 0);
}

} // namespace WebRoutes

namespace WebBody {

namespace {

// One calloc block: the header, then total + 1 bytes of text.
struct Body {
    size_t total;
    size_t received;
};

char* textOf(Body* b) { return reinterpret_cast<char*>(b + 1); }

} // namespace

bool chunk(void** slot, const uint8_t* data, size_t len, size_t index, size_t total, size_t maxTotal) {
    if (!slot || total > maxTotal || index > total || len > total - index) return false;
    if (index == 0) {
        if (*slot) free(*slot);
        *slot = calloc(1, sizeof(Body) + total + 1);
        if (!*slot) return false;
        static_cast<Body*>(*slot)->total = total;
    }
    Body* b = static_cast<Body*>(*slot);
    if (!b || b->total != total || index != b->received) return false;
    if (len) memcpy(textOf(b) + index, data, len);
    b->received += len;
    return true;
}

const char* text(const void* slot) {
    if (!slot) return nullptr;
    return reinterpret_cast<const char*>(static_cast<const Body*>(slot) + 1);
}

size_t length(const void* slot) { return slot ? static_cast<const Body*>(slot)->received : 0; }

void wipe(void* slot) {
    if (!slot) return;
    Body* b = static_cast<Body*>(slot);
    volatile char* t = textOf(b);
    for (size_t i = 0; i < b->total; ++i) t[i] = 0;
}

} // namespace WebBody
