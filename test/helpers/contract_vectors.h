// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Shared contract vectors (AF34, TEST-1): test/contract/vectors.json, read here by the host tests
// and by tools/contract_check.py against tools/web_mock.py. evaluate() chains the pure parts as
// net/web_server.cpp does: RequestGuard::check, then the route (WebRoutes::find) and the REST
// parser of a POST. The device is 192.168.1.50 without a web password; pio test runs in the
// project directory.

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <net/request_guard.h>
#include <net/web_routes.h>
#include <remote/rest_json.h>
#include <remote/rest_wifi.h>

namespace ContractVectors {

static const char* const TOKEN = "0123456789abcdef0123456789abcdef";
static const char* const HOST = "192.168.1.50";
static const uint32_t DEVICE = (192u << 24) | (168u << 16) | (1u << 8) | 50u;

struct Vector {
    std::string stage, method, path, query, body, contentType, name;
    bool hasBody = false, token = false;
    std::vector<std::pair<std::string, std::string>> headers;
    uint16_t status = 0;
    int code = 0;
};

struct Outcome {
    uint16_t status;
    int code;
};

inline bool load(std::vector<Vector>* out, std::string* error) {
    FILE* f = fopen("test/contract/vectors.json", "rb");
    if (!f) {
        *error = "test/contract/vectors.json not found";
        return false;
    }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    fclose(f);
    JsonDocument doc;
    if (deserializeJson(doc, text)) {
        *error = "vectors.json is not JSON";
        return false;
    }
    for (JsonObjectConst v : doc["vectors"].as<JsonArrayConst>()) {
        Vector x;
        x.stage = v["stage"] | "";
        x.method = v["method"] | "";
        const std::string target = v["path"] | "";
        const size_t q = target.find('?');
        x.path = target.substr(0, q);
        if (q != std::string::npos) x.query = target.substr(q + 1);
        x.hasBody = v["body"].is<const char*>();
        x.body = v["body"] | "";
        x.contentType = v["contentType"] | "application/json";
        x.token = v["token"] | false;
        for (JsonPairConst h : v["headers"].as<JsonObjectConst>()) x.headers.push_back(std::make_pair(h.key().c_str(), h.value().as<std::string>()));
        x.status = v["status"] | 0;
        x.code = v["code"] | 0;
        x.name = x.method + " " + target + " " + x.body;
        out->push_back(x);
    }
    return !out->empty();
}

// RequestGuard::check as the server feeds it (headers looked up case-insensitively there).
inline GuardDecision guardDecision(const Vector& v) {
    GuardConfig c;
    c.staIp = DEVICE;
    c.deviceName = "cyd-hf-1a2b";
    c.token = TOKEN;
    RequestGuard g;
    g.configure(c);
    GuardRequest r;
    r.method = v.method.c_str();
    r.path = v.path.c_str();
    r.query = v.query.c_str();
    r.host = HOST;
    std::string length = std::to_string(v.body.size());
    if (v.hasBody) {
        r.contentType = v.contentType.c_str();
        r.contentLength = length.c_str();
    }
    if (v.token) r.token = TOKEN;
    for (size_t i = 0; i < v.headers.size(); ++i) {
        const std::string& k = v.headers[i].first;
        const char* val = v.headers[i].second.c_str();
        if (k == "Host") r.host = val;
        else if (k == "Origin") r.origin = val;
        else if (k == "Sec-Fetch-Site") r.secFetchSite = val;
        else if (k == "Content-Length") r.contentLength = val;
        else if (k == "Content-Type") r.contentType = val;
        else if (k == "Transfer-Encoding") r.transferEncoding = val;
        else if (k == "X-CYD-Token") r.token = val;
        else if (k == "Cookie") r.cookie = val;
        else if (k == "Authorization") r.authorization = val;
        else if (k == "Upgrade") r.upgrade = val;
    }
    r.headerBytes = 300;
    r.clientIp = DEVICE + 1;
    r.localIp = DEVICE;
    r.freeHeap = 100000;
    return g.check(r, 1000);
}

// Guard, route and REST parser (no command is executed: a valid write counts as 200).
inline Outcome evaluate(const Vector& v) {
    const GuardDecision d = guardDecision(v);
    if (!d.allowed()) return Outcome {d.status(), RestJson::EXECUTION_ERROR};
    const WebRoutes::Kind route = WebRoutes::find(v.method.c_str(), v.path.c_str());
    const char* body = v.body.c_str();
    const size_t len = v.body.size();
    RestJson::Commands cmds {};
    RestJson::Reply rep {200, 0, nullptr};
    switch (route) {
        case WebRoutes::Kind::NotFound: return Outcome {404, RestJson::EXECUTION_ERROR};
        case WebRoutes::Kind::Channel1:
        case WebRoutes::Kind::Channel2: rep = RestJson::parseChannel(WebRoutes::channelOf(route), body, len, &cmds); break;
        case WebRoutes::Kind::Ch3: rep = RestJson::parseCh3(body, len, &cmds); break;
        case WebRoutes::Kind::Config: rep = RestJson::parseConfig(body, len, &cmds); break;
        case WebRoutes::Kind::Save: rep = RestJson::parseAction(CommandType::SaveNow, body, len, &cmds); break;
        case WebRoutes::Kind::Identify: rep = RestJson::parseAction(CommandType::Identify, body, len, &cmds); break;
        case WebRoutes::Kind::Login: {
            char pw[80];
            size_t pwLen = 0;
            rep = RestJson::parseLogin(body, len, pw, sizeof(pw), &pwLen);
            break;
        }
        case WebRoutes::Kind::WifiChange: {
            RestWifi::Change c;
            rep = RestWifi::parseChange(body, len, &c);
            RestWifi::wipe(&c);
            break;
        }
        case WebRoutes::Kind::WifiConfirm: rep = RestWifi::parseConfirm(body, len); break;
        default: break; // GET routes answer 200; logout is not in the vectors
    }
    return Outcome {rep.status, rep.ok() ? 0 : rep.code};
}

} // namespace ContractVectors
