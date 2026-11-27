// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

// Routes of the web server (AF32, TEST-1, CQ-6) and the assembly of a request body. Pure and
// host-tested; net/web_server.cpp dispatches with find() and keeps the bodies with chunk(),
// RequestGuard classifies paths with the predicates below, so the paths live in one place.
// The embedded files of data/www (tools/embed_www.py) are looked up by the server itself
// between the page and the other GET routes; they are not in this table.
namespace WebRoutes {

enum class Kind : uint8_t {
    NotFound,
    Page,         // GET / and /index.html
    State,        // GET /api/state
    Lxi,          // GET /lxi/identification
    Token,        // GET /api/token
    WifiSettings, // GET /api/wifi
    Scan,         // GET /api/wifi/scan
    WebSocket,    // GET /ws (upgrade)
    Channel1,     // POST /api/channel/1
    Channel2,     // POST /api/channel/2
    Ch3,          // POST /api/ch3
    Config,       // POST /api/config
    Save,         // POST /api/save
    Identify,     // POST /api/identify
    Login,        // POST /api/login
    Logout,       // POST /api/logout
    WifiChange,   // POST /api/wifi
    WifiConfirm   // POST /api/wifi/confirm
};

struct Route {
    bool post;    // POST, else GET and HEAD
    const char* path;
    Kind kind;
};

// The whole table (for the tests and the documentation check).
const Route* table(size_t* count);
// The route of a request; method "GET", "HEAD" or "POST" (the guard refuses the others before),
// path without the query. NotFound for unknown paths and for a path with the other method.
Kind find(const char* method, const char* path);
// The channel of Channel1/Channel2, else 0.
uint8_t channelOf(Kind k);

// Path classes of RequestGuard (A-705, A-706, A-713).
bool isApi(const char* path);  // /api/...
bool isWs(const char* path);   // /ws
// The WiFi settings and their sub-paths: need a session with a web password, also for GET.
bool isWifi(const char* path);
// POST /api/login and /api/logout need no session.
bool sessionFree(bool post, const char* path);

} // namespace WebRoutes

// A request body as ESPAsyncWebServer hands it over in chunks (index, len, total). The body is
// kept in one heap block in the request's _tempObject (allocated here with calloc, freed by the
// library with free()), NUL-terminated after total bytes. Bodies above maxTotal and chunks
// beyond total are not kept (the guard answers 413 from Content-Length).
namespace WebBody {

// One chunk; *slot is the request's _tempObject. index 0 starts a new body. false when the
// chunk is not kept (too large, beyond total, a gap, out of memory).
bool chunk(void** slot, const uint8_t* data, size_t len, size_t index, size_t total, size_t maxTotal);
// The body text (NUL-terminated) or nullptr without a body.
const char* text(const void* slot);
// Bytes received in order from index 0 (embedded NUL bytes included).
size_t length(const void* slot);
// Overwrites the kept bytes (passwords) without freeing the block.
void wipe(void* slot);

} // namespace WebBody
