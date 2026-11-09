#include "rest_wifi.h"

#include <stdio.h>
#include <string.h>
#include "rest_json_detail.h"
#include <utils/sanitize.h>

namespace RestWifi {

using RestJson::Reply;
using RestJson::detail::integer;
using RestJson::detail::keyIs;
using RestJson::detail::text;

namespace {

Reply ok() { return Reply {200, 0, nullptr}; }
Reply bad(int16_t code, const char* info = nullptr) { return Reply {400, code, info}; }

void wipeBytes(void* p, size_t n) {
    volatile uint8_t* b = (volatile uint8_t*)p;
    for (size_t i = 0; i < n; ++i) b[i] = 0;
}

// A JSON string copied into out (at most maxLen bytes, else -224).
Reply copyText(JsonVariantConst v, char* out, size_t outSize, size_t maxLen) {
    JsonString s;
    const Reply r = text(v, &s);
    if (!r.ok()) return r;
    if (s.size() > maxLen || s.size() >= outSize) return bad(RestJson::ILLEGAL_PARAMETER_VALUE);
    memcpy(out, s.c_str(), s.size());
    out[s.size()] = '\0';
    return ok();
}

Reply boolean(JsonVariantConst v, bool* out) {
    if (!v.is<bool>()) return bad(RestJson::DATA_TYPE_ERROR);
    *out = v.as<bool>();
    return ok();
}

Reply slotOf(JsonVariantConst v, uint8_t* out) {
    int64_t n = 0;
    const Reply r = integer(v, &n);
    if (!r.ok()) return r;
    if (n != 0 && n != 1) return bad(RestJson::ILLEGAL_PARAMETER_VALUE);
    *out = (uint8_t)n;
    return ok();
}

Reply parseNetwork(JsonVariantConst v, NetworkEntry* e) {
    if (!v.is<JsonObjectConst>()) return bad(RestJson::DATA_TYPE_ERROR);
    bool slot = false, ssid = false;
    for (JsonPairConst kv : v.as<JsonObjectConst>()) {
        Reply r = ok();
        if (keyIs(kv.key(), "slot")) {
            r = slotOf(kv.value(), &e->slot);
            slot = true;
        } else if (keyIs(kv.key(), "ssid")) {
            r = copyText(kv.value(), e->ssid, sizeof(e->ssid), 32);
            ssid = true;
        } else if (keyIs(kv.key(), "password")) {
            r = copyText(kv.value(), e->password, sizeof(e->password), 64);
            e->hasPassword = true;
        } else {
            r = bad(RestJson::PARAMETER_NOT_ALLOWED);
        }
        if (!r.ok()) return r;
    }
    if (!slot || !ssid || e->ssid[0] == '\0') return bad(RestJson::ILLEGAL_PARAMETER_VALUE, "A network needs slot and SSID");
    return ok();
}

const char* const SIP_KEYS[4] = {"ip", "mask", "gateway", "dns"};

Reply parseStaticIp(JsonVariantConst v, Change* c) {
    if (!v.is<JsonObjectConst>()) return bad(RestJson::DATA_TYPE_ERROR);
    for (JsonPairConst kv : v.as<JsonObjectConst>()) {
        Reply r = ok();
        int idx = -1;
        for (int i = 0; i < 4 && idx < 0; ++i) {
            if (keyIs(kv.key(), SIP_KEYS[i])) idx = i;
        }
        if (keyIs(kv.key(), "enabled")) {
            r = boolean(kv.value(), &c->sipEnabled);
            c->sipHasEnabled = true;
        } else if (idx >= 0) {
            r = copyText(kv.value(), c->sip[idx], sizeof(c->sip[idx]), 15);
            c->sipHas[idx] = true;
        } else {
            r = bad(RestJson::PARAMETER_NOT_ALLOWED);
        }
        if (!r.ok()) return r;
    }
    c->hasStaticIp = true;
    return ok();
}

Reply parseAp(JsonVariantConst v, Change* c) {
    if (!v.is<JsonObjectConst>()) return bad(RestJson::DATA_TYPE_ERROR);
    for (JsonPairConst kv : v.as<JsonObjectConst>()) {
        Reply r = ok();
        if (keyIs(kv.key(), "open")) {
            r = boolean(kv.value(), &c->apOpen);
            c->apHasOpen = true;
        } else if (keyIs(kv.key(), "password")) {
            r = copyText(kv.value(), c->apPassword, sizeof(c->apPassword), 63);
            c->apHasPassword = true;
        } else {
            r = bad(RestJson::PARAMETER_NOT_ALLOWED);
        }
        if (!r.ok()) return r;
    }
    c->hasAp = true;
    return ok();
}

Reply parseMode(JsonVariantConst v, WifiMode* out) {
    JsonString s;
    const Reply r = text(v, &s);
    if (!r.ok()) return r;
    if (keyIs(s, "off")) *out = WifiMode::Off;
    else if (keyIs(s, "client")) *out = WifiMode::Client;
    else if (keyIs(s, "ap")) *out = WifiMode::AccessPoint;
    else return bad(RestJson::ILLEGAL_PARAMETER_VALUE);
    return ok();
}

Reply parseInto(const char* body, size_t len, Change* c) {
    JsonDocument doc;
    Reply r = RestJson::detail::load(body, len, doc);
    if (!r.ok()) return r;
    bool any = false;
    for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
        any = true;
        const JsonVariantConst v = kv.value();
        if (keyIs(kv.key(), "mode")) {
            r = parseMode(v, &c->mode);
            c->hasMode = true;
        } else if (keyIs(kv.key(), "networks")) {
            if (!v.is<JsonArrayConst>()) return bad(RestJson::DATA_TYPE_ERROR);
            JsonArrayConst a = v.as<JsonArrayConst>();
            if (a.size() > 2) return bad(RestJson::DATA_OUT_OF_RANGE);
            for (JsonVariantConst e : a) {
                NetworkEntry& n = c->nets[c->netCount];
                r = parseNetwork(e, &n);
                if (!r.ok()) return r;
                if (c->netCount == 1 && c->nets[0].slot == n.slot) return bad(RestJson::ILLEGAL_PARAMETER_VALUE, "Slot given twice");
                ++c->netCount;
            }
        } else if (keyIs(kv.key(), "delete")) {
            if (!v.is<JsonArrayConst>()) return bad(RestJson::DATA_TYPE_ERROR);
            JsonArrayConst a = v.as<JsonArrayConst>();
            if (a.size() > 2) return bad(RestJson::DATA_OUT_OF_RANGE);
            for (JsonVariantConst e : a) {
                uint8_t slot = 0;
                r = slotOf(e, &slot);
                if (!r.ok()) return r;
                c->deleteMask = (uint8_t)(c->deleteMask | (1u << slot));
            }
        } else if (keyIs(kv.key(), "name")) {
            r = copyText(v, c->name, sizeof(c->name), 32);
            c->hasName = true;
        } else if (keyIs(kv.key(), "staticIp")) {
            r = parseStaticIp(v, c);
        } else if (keyIs(kv.key(), "ap")) {
            r = parseAp(v, c);
        } else {
            r = bad(RestJson::PARAMETER_NOT_ALLOWED);
        }
        if (!r.ok()) return r;
    }
    if (!any) return bad(RestJson::MISSING_PARAMETER);
    return ok();
}

// Network key as NetConfig::setNetwork takes it: "" open, passphrase or 64 hex digits.
bool validKey(const char* k) { return k[0] == '\0' || NetConfig::validPassphrase(k) || NetConfig::isPmkHex(k); }

const char* keyKind(const WifiNetwork& n) {
    if (n.ssid[0] == '\0') return "none";
    return n.key[0] ? "set" : "open";
}

void ipText(uint32_t ip, char* out, size_t outSize) {
    if (ip == 0) out[0] = '\0';
    else NetConfig::formatIpv4(ip, out, outSize);
}

void clean(const char* in, size_t maxIn, char* out, size_t outSize) {
    size_t n = 0;
    while (in && n < maxIn && in[n]) ++n;
    Sanitize::text(in, n, out, outSize, 32);
}

} // namespace

Reply parseChange(const char* body, size_t len, Change* out) {
    if (!out) return bad(RestJson::SYNTAX_ERROR);
    memset(out, 0, sizeof(*out));
    const Reply r = parseInto(body, len, out);
    if (!r.ok()) wipe(out);
    return r;
}

Reply merge(const Change& c, const NetSettings& cur, NetSettings* next) {
    if (!next) return bad(RestJson::SYNTAX_ERROR);
    NetSettings n = cur;
    Reply r = ok();
    if (c.hasMode) n.mode = c.mode;
    for (uint8_t slot = 0; slot < 2; ++slot) {
        if (c.deleteMask & (1u << slot)) NetConfig::clearNetwork(n, slot);
    }
    for (uint8_t i = 0; i < c.netCount && r.ok(); ++i) {
        const NetworkEntry& e = c.nets[i];
        const WifiNetwork& old = n.networks[e.slot];
        if (!NetConfig::validSsid(e.ssid)) {
            r = bad(RestJson::ILLEGAL_PARAMETER_VALUE, "SSID with 1 to 32 bytes");
        } else if (e.hasPassword) {
            if (!validKey(e.password) || NetConfig::setNetwork(n, e.slot, e.ssid, e.password) != NetConfig::KeyResult::Ok) {
                r = bad(RestJson::ILLEGAL_PARAMETER_VALUE, "Password with 8 to 63 characters");
            }
        } else if (strcmp(old.ssid, e.ssid) != 0) {
            NetConfig::setNetwork(n, e.slot, e.ssid, ""); // a new SSID without password: open
        }
    }
    if (r.ok() && c.hasName) {
        if (!NetConfig::validDeviceName(c.name)) r = bad(RestJson::ILLEGAL_PARAMETER_VALUE, "Device name: a-z, 0-9 and -, max. 32");
        else snprintf(n.deviceName, sizeof(n.deviceName), "%s", c.name);
    }
    if (r.ok() && c.hasStaticIp) {
        StaticIp ip = n.staticIp;
        if (c.sipHasEnabled) ip.enabled = c.sipEnabled;
        uint32_t* fields[4] = {&ip.ip, &ip.mask, &ip.gateway, &ip.dns};
        for (int i = 0; i < 4 && r.ok(); ++i) {
            if (!c.sipHas[i]) continue;
            uint32_t v = 0;
            if (c.sip[i][0] == '\0') *fields[i] = 0;
            else if (NetConfig::parseIpv4(c.sip[i], &v)) *fields[i] = v;
            else r = bad(RestJson::ILLEGAL_PARAMETER_VALUE, "Static IP: enter addresses as a.b.c.d");
        }
        if (r.ok() && ip.enabled) {
            const NetConfig::IpCheck k = NetConfig::checkStaticIp(ip);
            if (k == NetConfig::IpCheck::BadMask) r = bad(RestJson::ILLEGAL_PARAMETER_VALUE, NetConfig::ipCheckText(k));
            else if (k != NetConfig::IpCheck::Ok) r = bad(RestJson::SETTINGS_CONFLICT, NetConfig::ipCheckText(k));
        }
        if (r.ok()) n.staticIp = ip;
    }
    if (r.ok() && c.hasAp) {
        if (c.apHasOpen) n.apOpen = c.apOpen;
        if (c.apHasPassword) {
            if (!NetConfig::validPassphrase(c.apPassword)) r = bad(RestJson::ILLEGAL_PARAMETER_VALUE, "AP password with 8 to 63 characters");
            else snprintf(n.apPassword, sizeof(n.apPassword), "%s", c.apPassword);
        }
    }
    if (r.ok()) *next = n;
    else *next = cur;
    wipeBytes(&n, sizeof(n));
    return r;
}

Reply parseConfirm(const char* body, size_t len) {
    JsonDocument doc;
    const Reply r = RestJson::detail::load(body, len, doc);
    if (!r.ok()) return r;
    if (doc.as<JsonObjectConst>().size() != 0) return bad(RestJson::PARAMETER_NOT_ALLOWED);
    return ok();
}

void wipe(Change* c) {
    if (c) wipeBytes(c, sizeof(*c));
}

size_t buildSettings(const NetSettings& s, const char* apSsid, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    JsonDocument doc;
    doc["mode"] = s.mode == WifiMode::Client ? "client" : s.mode == WifiMode::AccessPoint ? "ap" : "off";
    char ssid[2][33];
    JsonArray nets = doc["networks"].to<JsonArray>();
    for (uint8_t i = 0; i < 2; ++i) {
        clean(s.networks[i].ssid, sizeof(s.networks[i].ssid), ssid[i], sizeof(ssid[i]));
        JsonObject o = nets.add<JsonObject>();
        o["ssid"] = (const char*)ssid[i];
        o["key"] = keyKind(s.networks[i]);
    }
    char ap[33], name[33];
    clean(apSsid, 32, ap, sizeof(ap));
    clean(s.deviceName, sizeof(s.deviceName), name, sizeof(name));
    JsonObject a = doc["ap"].to<JsonObject>();
    a["ssid"] = (const char*)ap;
    a["open"] = s.apOpen;
    a["passwordSet"] = s.apPassword[0] != '\0';
    doc["name"] = (const char*)name;
    char ips[4][16];
    const uint32_t v[4] = {s.staticIp.ip, s.staticIp.mask, s.staticIp.gateway, s.staticIp.dns};
    JsonObject sip = doc["staticIp"].to<JsonObject>();
    sip["enabled"] = s.staticIp.enabled;
    for (int i = 0; i < 4; ++i) {
        ipText(v[i], ips[i], sizeof(ips[i]));
        sip[SIP_KEYS[i]] = (const char*)ips[i];
    }
    if (doc.overflowed() || measureJson(doc) >= outSize) return 0;
    return serializeJson(doc, out, outSize);
}

size_t buildScan(const char* state, const ScanList* list, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    JsonDocument doc;
    doc["state"] = state ? state : "failed";
    JsonArray nets = doc["networks"].to<JsonArray>();
    if (list && state && strcmp(state, "done") == 0) {
        for (uint8_t i = 0; i < list->count(); ++i) {
            const ScanEntry& e = list->at(i);
            JsonObject o = nets.add<JsonObject>();
            o["ssid"] = (const char*)e.shown; // sanitized at the scan (W-S9)
            o["rssi"] = e.rssi;
            o["secure"] = e.secure;
        }
    }
    if (doc.overflowed() || measureJson(doc) >= outSize) return 0;
    return serializeJson(doc, out, outSize);
}

}
