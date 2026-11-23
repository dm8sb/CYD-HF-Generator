// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target rest_wifi: RestWifi::parseChange, merge, wipe and buildSettings (remote/rest_wifi.*),
// POST and GET /api/wifi (S55, W-E9, W-S1, W-S12, D23).
// Input: byte 0 the current settings (bits 0-1 mode, bit 2 network in slot 0, bit 3 in slot 1,
// bit 4 static IP on, bit 5 AP password set, bit 6 open AP), byte 1 bit 0 raw mode; the rest
// either drives a generator of bodies (structured mode) or is the body itself (raw mode).
// The generator writes a JSON object of known keys and wrong ones (unknown keys, wrong types,
// strings too long, values outside the rules, more than two entries) and computes the expected
// answer with an independent model of the documented rules (rest_wifi.h): the first problem
// in key order, -104 type, -108 unknown key or a final "off" with other keys, -109 nothing,
// -222 more than two entries, -224 values, -221 an implausible static IP, 413 above 1 KB.
//
// Invariants:
// - structured mode: parseChange and merge give exactly the model's status and code, and on
//   success exactly the model's settings (keys stored as given, a PMK in lower case, a new SSID
//   without password open, an unchanged one keeps its key, "" clears a static IP field);
// - a refused change leaves next == cur byte for byte; a refused parse leaves the Change zeroed
//   (passwords wiped), wipe() zeroes it;
// - every accepted result passes NetConfig's own checks and keeps the web password;
// - no answer echoes a secret (raw mode too): the GET answer has exactly its documented fields,
//   keys only as set/open/none, the AP password only as passwordSet, and the reply info is a
//   fixed text, never a pointer into the body or the Change.

#include "fuzz_common.h"

#include <string>
#include <vector>
#include <ArduinoJson.h>
#include <net/net_settings.h>
#include <remote/rest_wifi.h>
#include <utils/ipv4.h>

namespace fuzz_rest_wifi {

using RestJson::Reply;

const int16_t TYPE = -104, UNKNOWN = -108, MISSING = -109, CONFLICT = -221, RANGE = -222, VALUE = -224;
const char AP_SSID[] = "CYD-HF-1A2B";

struct Reader {
    const uint8_t* p;
    size_t n;
    uint8_t byte() {
        if (!n) return 0;
        --n;
        return *p++;
    }
    unsigned pick(unsigned count) { return byte() % count; }
};

// --- the model ------------------------------------------------------------------------------

struct Net {
    int slot = -1;
    std::string ssid;
    bool hasPassword = false;
    std::string password;
};

struct Model {
    int16_t parseError = 0; // first problem while reading the body
    int keys = 0;
    bool hasMode = false;
    WifiMode mode = WifiMode::Off;
    std::vector<Net> nets;
    unsigned deleteMask = 0;
    bool hasName = false;
    std::string name;
    bool hasStaticIp = false, sipHasEnabled = false, sipEnabled = false;
    bool sipHas[4] = {false, false, false, false};
    std::string sip[4];
    bool hasAp = false, apHasOpen = false, apOpen = false, apHasPassword = false;
    std::string apPassword;
};

void fail(Model& m, int16_t code) {
    if (!m.parseError) m.parseError = code;
}

std::string quote(const std::string& raw) {
    std::string s = "\"";
    for (size_t i = 0; i < raw.size(); ++i) {
        const unsigned char c = (unsigned char)raw[i];
        if (c == '"' || c == '\\') {
            s += '\\';
            s += (char)c;
        } else if (c < 0x20) {
            char b[8];
            snprintf(b, sizeof(b), "\\u%04x", c);
            s += b;
        } else if (c >= 0x80) {
            // Two-byte UTF-8 written as an escape (the model counts bytes).
            continue;
        } else {
            s += (char)c;
        }
    }
    return s + "\"";
}

// A string of len bytes from a small alphabet (blank, quote, backslash, a NUL, a two-byte
// UTF-8 character written as ä counts two bytes).
std::string textOf(Reader& in, size_t len, std::string* json) {
    static const char ALPHA[] = "abcXYZ019 -_.\"\\!~";
    std::string raw;
    std::string j = "\"";
    while (raw.size() < len) {
        const unsigned k = in.pick(24);
        if (k < sizeof(ALPHA) - 1) {
            const char c = ALPHA[k];
            raw += c;
            if (c == '"' || c == '\\') j += '\\';
            j += c;
        } else if (k == 22 && raw.size() + 2 <= len) {
            raw += "\xC3\xA4";
            j += "\\u00e4";
        } else if (k == 23 && in.pick(4) == 0) {
            raw += '\0';
            j += "\\u0000";
        } else {
            raw += 'q';
            j += 'q';
        }
    }
    *json = j + "\"";
    return raw;
}

bool hasNul(const std::string& s) { return s.find('\0') != std::string::npos; }

// A JSON string value read by copyText(maxLen): type, NUL, length.
int16_t textError(const std::string& raw, size_t maxLen) {
    if (hasNul(raw)) return VALUE;
    if (raw.size() > maxLen) return VALUE;
    return 0;
}

// Values of a wrong type for any field.
const char* const WRONG[] = {"1", "1.5", "true", "null", "[]", "{}", "-1"};
const char* const NOT_ARRAY[] = {"1", "true", "null", "{}", "\"0\""};

std::string slotValue(Reader& in, int* slot, int16_t* err) {
    switch (in.pick(8)) {
        case 0: *slot = 1; return "1";
        case 1: *err = VALUE; return "2";
        case 2: *err = TYPE; return "\"0\"";
        case 3: *err = TYPE; return "0.0";
        case 4: *err = TYPE; return "true";
        case 5: *err = VALUE; return "-1";
        default: *slot = 0; return "0";
    }
}

std::string passwordOf(Reader& in, std::string* raw) {
    static const size_t LENS[] = {0, 5, 8, 12, 63, 64, 64, 65};
    const size_t len = LENS[in.pick(8)];
    if (len == 64 && in.pick(2) == 0) {
        static const char HEX[] = "0123456789abcdefABCDEF";
        std::string h;
        for (size_t i = 0; i < 64; ++i) h += HEX[in.pick(22)];
        *raw = h;
        return "\"" + h + "\"";
    }
    std::string j;
    *raw = textOf(in, len, &j);
    return j;
}

std::string networkEntry(Reader& in, Model& m, Net* net) {
    if (in.pick(10) == 0) {
        fail(m, TYPE);
        return "\"net\"";
    }
    std::string s = "{";
    bool slotSeen = false, ssidSeen = false, passSeen = false, unknownSeen = false;
    int16_t err = 0;
    const unsigned fields = in.pick(5);
    for (unsigned f = 0; f < fields; ++f) {
        std::string kv;
        const unsigned k = in.pick(8);
        if (k < 3 && !slotSeen) {
            slotSeen = true;
            int16_t e = 0;
            kv = "\"slot\":" + slotValue(in, &net->slot, &e);
            if (!err) err = e;
        } else if (k < 6 && !ssidSeen) {
            ssidSeen = true;
            std::string j;
            if (in.pick(12) == 0) {
                j = WRONG[in.pick(7)];
                if (!err) err = TYPE;
            } else {
                static const size_t LENS[] = {0, 1, 5, 12, 31, 32, 33, 34};
                net->ssid = textOf(in, LENS[in.pick(8)], &j);
                const int16_t e = textError(net->ssid, 32);
                if (!err) err = e;
            }
            kv = "\"ssid\":" + j;
        } else if (k < 7 && !passSeen) {
            passSeen = true;
            net->hasPassword = true;
            std::string j;
            if (in.pick(12) == 0) {
                j = WRONG[in.pick(7)];
                if (!err) err = TYPE;
            } else {
                j = passwordOf(in, &net->password);
                const int16_t e = textError(net->password, 64);
                if (!err) err = e;
            }
            kv = "\"password\":" + j;
        } else if (!unknownSeen) {
            unknownSeen = true;
            static const char* const NAMES[] = {"\"key\"", "\"psk\"", "\"Slot\"", "\"\"", "\"bssid\""};
            kv = std::string(NAMES[in.pick(5)]) + ":" + "1";
            if (!err) err = UNKNOWN;
        } else {
            continue;
        }
        if (s.size() > 1) s += ",";
        s += kv;
    }
    s += "}";
    if (!err && (!slotSeen || !ssidSeen || net->ssid.empty() || net->slot < 0)) err = VALUE;
    if (err) fail(m, err);
    return s;
}

std::string networks(Reader& in, Model& m) {
    if (in.pick(10) == 0) {
        fail(m, TYPE);
        return NOT_ARRAY[in.pick(5)];
    }
    const unsigned count = in.pick(4);
    std::string s = "[";
    const int16_t before = m.parseError;
    std::vector<Net> nets;
    for (unsigned i = 0; i < count; ++i) {
        Net n;
        if (i) s += ",";
        s += networkEntry(in, m, &n);
        // A second entry for the same slot (checked after the entry itself).
        if (!m.parseError && nets.size() == 1 && nets[0].slot == n.slot) fail(m, VALUE);
        nets.push_back(n);
    }
    s += "]";
    // More than two entries: refused before any entry is read.
    if (count > 2) {
        m.parseError = before ? before : RANGE;
    } else if (!m.parseError) {
        m.nets = nets;
    }
    return s;
}

std::string deletes(Reader& in, Model& m) {
    if (in.pick(10) == 0) {
        fail(m, TYPE);
        return "\"0\"";
    }
    const unsigned count = in.pick(4);
    std::string s = "[";
    int16_t err = 0;
    unsigned mask = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (i) s += ",";
        int slot = -1;
        int16_t e = 0;
        s += slotValue(in, &slot, &e);
        if (!err) err = e;
        if (!e) mask |= 1u << slot;
    }
    s += "]";
    if (count > 2) err = RANGE;
    if (err) fail(m, err);
    else m.deleteMask |= mask;
    return s;
}

std::string nameValue(Reader& in, Model& m) {
    static const char* const NAMES[] = {"bench-1", "a", "lab-generator-7", "Bad_Name", "-x", "x-", "",
                                        "abcdefghijklmnopqrstuvwxyz012345", "abcdefghijklmnopqrstuvwxyz0123456"};
    const unsigned k = in.pick(11);
    if (k >= 9) {
        fail(m, TYPE);
        return WRONG[in.pick(7)];
    }
    const std::string v = NAMES[k];
    m.hasName = true;
    m.name = v;
    if (v.size() > 32) fail(m, VALUE);
    return quote(v);
}

const char* const SIP_KEYS[4] = {"ip", "mask", "gateway", "dns"};
const char* const ADDRESSES[] = {"10.0.0.20", "255.255.255.0", "10.0.0.1", "10.0.0.2", "", "1.2.3",
                                 "999.1.1.1", "10.0.0.255", "0.0.0.0", "255.255.0.255", "10.0.0.0",
                                 "10.0.1.1", "255.255.255.252", "010.0.0.1", "1234567890123456", "255.255.255.255"};

std::string staticIp(Reader& in, Model& m) {
    if (in.pick(10) == 0) {
        fail(m, TYPE);
        return "\"10.0.0.20\"";
    }
    std::string s = "{";
    bool seen[6] = {false, false, false, false, false, false};
    int16_t err = 0;
    const unsigned fields = in.pick(6);
    for (unsigned f = 0; f < fields; ++f) {
        const unsigned k = in.pick(6);
        if (seen[k]) continue;
        seen[k] = true;
        std::string kv;
        if (k == 0) {
            const unsigned v = in.pick(5);
            m.sipHasEnabled = true;
            if (v == 4) {
                kv = "\"enabled\":1";
                if (!err) err = TYPE;
            } else {
                m.sipEnabled = v != 0;
                kv = std::string("\"enabled\":") + (m.sipEnabled ? "true" : "false");
            }
        } else if (k <= 4) {
            const int idx = (int)k - 1;
            m.sipHas[idx] = true;
            if (in.pick(12) == 0) {
                kv = std::string("\"") + SIP_KEYS[idx] + "\":" + WRONG[in.pick(7)];
                if (!err) err = TYPE;
            } else {
                m.sip[idx] = ADDRESSES[in.pick(16)];
                kv = std::string("\"") + SIP_KEYS[idx] + "\":" + quote(m.sip[idx]);
                if (!err && m.sip[idx].size() > 15) err = VALUE;
            }
        } else {
            kv = "\"gw\":\"10.0.0.1\"";
            if (!err) err = UNKNOWN;
        }
        if (s.size() > 1) s += ",";
        s += kv;
    }
    s += "}";
    if (err) fail(m, err);
    else m.hasStaticIp = true;
    return s;
}

std::string ap(Reader& in, Model& m) {
    if (in.pick(10) == 0) {
        fail(m, TYPE);
        return "true";
    }
    std::string s = "{";
    bool seen[3] = {false, false, false};
    int16_t err = 0;
    const unsigned fields = in.pick(4);
    for (unsigned f = 0; f < fields; ++f) {
        const unsigned k = in.pick(3);
        if (seen[k]) continue;
        seen[k] = true;
        std::string kv;
        if (k == 0) {
            const unsigned v = in.pick(5);
            m.apHasOpen = true;
            if (v == 4) {
                kv = "\"open\":\"yes\"";
                if (!err) err = TYPE;
            } else {
                m.apOpen = v == 1;
                kv = std::string("\"open\":") + (m.apOpen ? "true" : "false");
            }
        } else if (k == 1) {
            m.apHasPassword = true;
            std::string raw;
            const std::string j = passwordOf(in, &raw);
            m.apPassword = raw;
            kv = "\"password\":" + j;
            const int16_t e = textError(raw, 63);
            if (!err) err = e;
        } else {
            kv = "\"ssid\":\"x\"";
            if (!err) err = UNKNOWN;
        }
        if (s.size() > 1) s += ",";
        s += kv;
    }
    s += "}";
    if (err) fail(m, err);
    else m.hasAp = true;
    return s;
}

std::string modeValue(Reader& in, Model& m) {
    switch (in.pick(7)) {
        case 0: m.hasMode = true; m.mode = WifiMode::Off; return "\"off\"";
        case 1: m.hasMode = true; m.mode = WifiMode::Client; return "\"client\"";
        case 2: m.hasMode = true; m.mode = WifiMode::AccessPoint; return "\"ap\"";
        case 3: fail(m, VALUE); return "\"Client\"";
        case 4: fail(m, VALUE); return "\"\"";
        case 5: fail(m, TYPE); return "2";
        default: fail(m, TYPE); return "null";
    }
}

std::string body(Reader& in, Model& m) {
    std::string s = "{";
    bool used[7] = {false, false, false, false, false, false, false};
    const unsigned count = in.pick(5);
    int unknown = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned k = in.pick(7);
        if (k < 6 && used[k]) k = 6;
        used[k] = k < 6;
        std::string kv;
        switch (k) {
            case 0: kv = "\"mode\":" + modeValue(in, m); break;
            case 1: kv = "\"networks\":" + networks(in, m); break;
            case 2: kv = "\"delete\":" + deletes(in, m); break;
            case 3: kv = "\"name\":" + nameValue(in, m); break;
            case 4: kv = "\"staticIp\":" + staticIp(in, m); break;
            case 5: kv = "\"ap\":" + ap(in, m); break;
            default: {
                static const char* const NAMES[] = {"pass", "password", "Mode", "", "wifi", "networks "};
                char key[24];
                snprintf(key, sizeof(key), "\"%s%d\"", NAMES[in.pick(6)], unknown++);
                kv = std::string(key) + ":" + WRONG[in.pick(7)];
                fail(m, UNKNOWN);
            }
        }
        if (i) s += ",";
        s += kv;
        ++m.keys;
    }
    s += "}";
    if (!m.parseError && m.keys == 0) m.parseError = MISSING;
    // D23: "off" is final and the only key.
    if (!m.parseError && m.hasMode && m.mode == WifiMode::Off && m.keys > 1) m.parseError = UNKNOWN;
    return s;
}

// --- merge model ----------------------------------------------------------------------------

bool passphrase(const std::string& p) {
    if (p.size() < 8 || p.size() > 63) return false;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] < 0x20 || p[i] > 0x7E) return false;
    }
    return true;
}
bool pmk(const std::string& p) {
    if (p.size() != 64) return false;
    for (size_t i = 0; i < 64; ++i) {
        const char c = p[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}
bool deviceName(const std::string& n) {
    if (n.empty() || n.size() > 32 || n[0] == '-' || n[n.size() - 1] == '-') return false;
    for (size_t i = 0; i < n.size(); ++i) {
        const char c = n[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return true;
}
// Canonical dotted quad without leading zeros.
bool quad(const std::string& s, uint32_t* out) {
    uint32_t v = 0;
    size_t i = 0;
    for (int part = 0; part < 4; ++part) {
        size_t j = i;
        while (j < s.size() && s[j] >= '0' && s[j] <= '9') ++j;
        if (j == i || j - i > 3 || (j - i > 1 && s[i] == '0')) return false;
        const unsigned x = (unsigned)atoi(s.substr(i, j - i).c_str());
        if (x > 255) return false;
        v = v << 8 | x;
        if (part < 3) {
            if (j >= s.size() || s[j] != '.') return false;
            ++j;
        }
        i = j;
    }
    if (i != s.size()) return false;
    *out = v;
    return true;
}

struct Expect {
    int16_t code = 0;
    NetSettings next;
};

void setNet(NetSettings& n, int slot, const std::string& ssid, const std::string& key) {
    memset(&n.networks[slot], 0, sizeof(WifiNetwork));
    memcpy(n.networks[slot].ssid, ssid.data(), ssid.size());
    std::string k = key;
    if (pmk(k)) {
        for (size_t i = 0; i < k.size(); ++i) {
            if (k[i] >= 'A' && k[i] <= 'F') k[i] = (char)(k[i] + 32);
        }
    }
    memcpy(n.networks[slot].key, k.data(), k.size());
}

Expect mergeModel(const Model& m, const NetSettings& cur) {
    Expect e;
    NetSettings n = cur;
    if (m.hasMode) n.mode = m.mode;
    const bool netGiven = !m.nets.empty(); // an empty "networks" array does not count
    if (n.mode == WifiMode::Off && (netGiven || m.deleteMask || m.hasName || m.hasStaticIp || m.hasAp)) {
        e.code = UNKNOWN;
        return e;
    }
    for (int slot = 0; slot < 2; ++slot) {
        if (m.deleteMask & (1u << slot)) memset(&n.networks[slot], 0, sizeof(WifiNetwork));
    }
    for (size_t i = 0; i < m.nets.size(); ++i) {
        const Net& x = m.nets[i];
        if (x.hasPassword) {
            if (!(x.password.empty() || passphrase(x.password) || pmk(x.password))) {
                e.code = VALUE;
                return e;
            }
            setNet(n, x.slot, x.ssid, x.password);
        } else if (std::string(n.networks[x.slot].ssid) != x.ssid) {
            setNet(n, x.slot, x.ssid, "");
        }
    }
    if (m.hasName) {
        if (!deviceName(m.name)) {
            e.code = VALUE;
            return e;
        }
        snprintf(n.deviceName, sizeof(n.deviceName), "%s", m.name.c_str());
    }
    if (m.hasStaticIp) {
        StaticIp ip = n.staticIp;
        if (m.sipHasEnabled) ip.enabled = m.sipEnabled;
        uint32_t* f[4] = {&ip.ip, &ip.mask, &ip.gateway, &ip.dns};
        for (int i = 0; i < 4; ++i) {
            if (!m.sipHas[i]) continue;
            uint32_t v = 0;
            if (m.sip[i].empty()) *f[i] = 0;
            else if (quad(m.sip[i], &v)) *f[i] = v;
            else {
                e.code = VALUE;
                return e;
            }
        }
        if (ip.enabled) {
            const NetConfig::IpCheck k = NetConfig::checkStaticIp(ip);
            if (k == NetConfig::IpCheck::BadMask) {
                e.code = VALUE;
                return e;
            }
            if (k != NetConfig::IpCheck::Ok) {
                e.code = CONFLICT;
                return e;
            }
        }
        n.staticIp = ip;
    }
    if (m.hasAp) {
        if (m.apHasOpen) n.apOpen = m.apOpen;
        if (m.apHasPassword) {
            if (!passphrase(m.apPassword)) {
                e.code = VALUE;
                return e;
            }
            snprintf(n.apPassword, sizeof(n.apPassword), "%s", m.apPassword.c_str());
        }
    }
    e.next = n;
    return e;
}

// --- checks ---------------------------------------------------------------------------------

NetSettings current(uint8_t b) {
    NetSettings s;
    memset(&s, 0, sizeof(s));
    s.mode = (WifiMode)((b & 3) % 3);
    if (b & 4) {
        snprintf(s.networks[0].ssid, sizeof(s.networks[0].ssid), "%s", "Lab");
        snprintf(s.networks[0].key, sizeof(s.networks[0].key), "%s", "labpassword");
    }
    if (b & 8) {
        snprintf(s.networks[1].ssid, sizeof(s.networks[1].ssid), "%s", "abc");
        snprintf(s.networks[1].key, sizeof(s.networks[1].key), "%s",
                 "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    }
    snprintf(s.deviceName, sizeof(s.deviceName), "%s", "cyd-hf-1a2b");
    if (b & 16) {
        s.staticIp.enabled = true;
        s.staticIp.ip = (10u << 24) | 20u;
        s.staticIp.mask = 0xFFFFFF00u;
        s.staticIp.gateway = (10u << 24) | 1u;
        s.staticIp.dns = (10u << 24) | 2u;
    }
    if (b & 32) snprintf(s.apPassword, sizeof(s.apPassword), "%s", "abcdefghjkmn");
    s.apOpen = (b & 64) != 0;
    snprintf(s.webPassword, sizeof(s.webPassword), "%s", "pbkdf2-sha1$10000$00$00");
    return s;
}

bool zeroed(const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; ++i) {
        if (b[i]) return false;
    }
    return true;
}

bool sameSettings(const NetSettings& a, const NetSettings& b) {
    for (int i = 0; i < 2; ++i) {
        if (strcmp(a.networks[i].ssid, b.networks[i].ssid) || strcmp(a.networks[i].key, b.networks[i].key)) return false;
    }
    return a.mode == b.mode && a.apOpen == b.apOpen && !strcmp(a.apPassword, b.apPassword) &&
           !strcmp(a.deviceName, b.deviceName) && a.staticIp.enabled == b.staticIp.enabled && a.staticIp.ip == b.staticIp.ip &&
           a.staticIp.mask == b.staticIp.mask && a.staticIp.gateway == b.staticIp.gateway && a.staticIp.dns == b.staticIp.dns &&
           !strcmp(a.webPassword, b.webPassword);
}

bool oneOf(JsonVariantConst v, const char* a, const char* b, const char* c) {
    if (!v.is<const char*>()) return false;
    const char* s = v.as<const char*>();
    return !strcmp(s, a) || !strcmp(s, b) || (c && !strcmp(s, c));
}

bool addressText(JsonVariantConst v) {
    if (!v.is<const char*>()) return false;
    uint32_t ip = 0;
    const std::string s = v.as<const char*>();
    return s.empty() || (quad(s, &ip) && ip != 0);
}

// No answer carries a secret. The GET answer has exactly its documented fields; the only free
// texts are the SSIDs and the device name (not secret), keys appear as set/open/none and the AP
// password as passwordSet. The reply info is a fixed text, never a pointer into the body or
// the Change.
void noEcho(const NetSettings& s, const Reply& r, const std::string& body, const RestWifi::Change& c) {
    if (r.info) {
        FUZZ_CHECK(!(r.info >= body.data() && r.info < body.data() + body.size()));
        FUZZ_CHECK(!(r.info >= (const char*)&c && r.info < (const char*)&c + sizeof(c)));
    }
    static char out[RestWifi::SETTINGS_SIZE];
    const size_t n = RestWifi::buildSettings(s, AP_SSID, out, sizeof(out));
    FUZZ_CHECK(n > 0 && n < sizeof(out) && strlen(out) == n);
    JsonDocument doc;
    FUZZ_CHECK(!deserializeJson(doc, out, n));
    JsonObjectConst o = doc.as<JsonObjectConst>();
    FUZZ_CHECK(o.size() == 5);
    FUZZ_CHECK(oneOf(o["mode"], "off", "client", "ap"));
    JsonArrayConst nets = o["networks"].as<JsonArrayConst>();
    FUZZ_CHECK(nets.size() == 2);
    for (JsonObjectConst e : nets) {
        FUZZ_CHECK(e.size() == 2 && e["ssid"].is<const char*>());
        FUZZ_CHECK(oneOf(e["key"], "set", "open", "none"));
    }
    JsonObjectConst a = o["ap"].as<JsonObjectConst>();
    FUZZ_CHECK(a.size() == 3 && a["ssid"].is<const char*>() && a["open"].is<bool>() && a["passwordSet"].is<bool>());
    FUZZ_CHECK(a["passwordSet"].as<bool>() == (s.apPassword[0] != '\0'));
    FUZZ_CHECK(o["name"].is<const char*>());
    JsonObjectConst ip = o["staticIp"].as<JsonObjectConst>();
    FUZZ_CHECK(ip.size() == 5 && ip["enabled"].is<bool>());
    for (int i = 0; i < 4; ++i) FUZZ_CHECK(addressText(ip[SIP_KEYS[i]]));
}

void validResult(const NetSettings& next, const NetSettings& cur) {
    FUZZ_CHECK(NetConfig::validDeviceName(next.deviceName));
    for (int i = 0; i < 2; ++i) {
        if (next.networks[i].ssid[0]) {
            FUZZ_CHECK(NetConfig::validSsid(next.networks[i].ssid));
            FUZZ_CHECK(NetConfig::validNetworkKey(next.networks[i].key));
        } else {
            FUZZ_CHECK(next.networks[i].key[0] == '\0');
        }
    }
    if (next.staticIp.enabled) FUZZ_CHECK(NetConfig::checkStaticIp(next.staticIp) == NetConfig::IpCheck::Ok);
    FUZZ_CHECK(next.apPassword[0] == '\0' || NetConfig::validPassphrase(next.apPassword));
    FUZZ_CHECK(strcmp(next.webPassword, cur.webPassword) == 0);
}

int run(const uint8_t* data, size_t size) {
    if (size < 2) return 0;
    const NetSettings cur = current(data[0]);
    const bool raw = (data[1] & 1) != 0;
    Reader in {data + 2, size - 2};

    std::string text;
    Model m;
    if (raw) text.assign((const char*)data + 2, size - 2);
    else text = body(in, m);

    RestWifi::Change c;
    memset(&c, 0xA5, sizeof(c));
    const Reply p = RestWifi::parseChange(text.data(), text.size(), &c);
    NetSettings next;
    memset(&next, 0x5A, sizeof(next));
    if (!p.ok()) {
        FUZZ_CHECK(zeroed(&c, sizeof(c))); // passwords wiped
        FUZZ_CHECK(p.code < 0);
        if (!raw) {
            if (text.size() > RestJson::MAX_BODY) FUZZ_CHECK(p.status == 413);
            else FUZZ_CHECK(p.status == 400 && p.code == m.parseError);
        }
        noEcho(cur, p, text, c);
        return 0;
    }
    if (!raw) FUZZ_CHECK(text.size() <= RestJson::MAX_BODY && m.parseError == 0);
    const Reply r = RestWifi::merge(c, cur, &next);
    if (r.ok()) {
        validResult(next, cur);
        noEcho(next, r, text, c);
    } else {
        FUZZ_CHECK(memcmp(&next, &cur, sizeof(cur)) == 0);
        FUZZ_CHECK(r.status == 400 && r.code < 0);
        noEcho(cur, r, text, c);
    }
    if (!raw) {
        const Expect e = mergeModel(m, cur);
        FUZZ_CHECK(r.code == e.code);
        if (r.ok()) FUZZ_CHECK(sameSettings(next, e.next));
    }
    RestWifi::wipe(&c);
    FUZZ_CHECK(zeroed(&c, sizeof(c)));
    return 0;
}

}

FUZZ_TARGET(rest_wifi, fuzz_rest_wifi::run)
