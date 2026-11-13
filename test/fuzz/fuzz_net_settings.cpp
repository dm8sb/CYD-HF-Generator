// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target net_settings: NetConfig::parseIpv4/formatIpv4, checkStaticIp, setNetwork,
// wifiQrText and the name/passphrase checks (W-S12, W-S14, W-S16, W-E3).
// Input: byte 0 selects the function, the rest is its input (C strings cut at NUL).
// Invariants:
// - parseIpv4 accepts exactly the canonical dotted quads: formatIpv4 gives the text back, and
//   parseIpv4(formatIpv4(x)) == x for any x.
// - checkStaticIp Ok only for a contiguous /1../30 mask, address and gateway different, in one
//   subnet, neither network nor broadcast address.
// - setNetwork stores exactly what it accepted (SSID 1..32 bytes; key empty, 8..63 printable
//   characters or 64 hex digits in lower case) and leaves the settings alone otherwise.
// - wifiQrText never writes past outSize; on success the S/P fields unescape to the input.
// - validDeviceName and validPassphrase agree with independent checks.

#include "fuzz_common.h"

#include <ctype.h>
#include <string>
#include <vector>
#include <net/net_settings.h>

namespace fuzz_net_settings {

std::string cstr(const uint8_t* d, size_t n) {
    std::string s(reinterpret_cast<const char*>(d), n);
    const size_t nul = s.find('\0');
    if (nul != std::string::npos) s.resize(nul);
    return s;
}

uint32_t u32(const uint8_t* d) { return (uint32_t)d[0] << 24 | (uint32_t)d[1] << 16 | (uint32_t)d[2] << 8 | d[3]; }

void ipv4(const uint8_t* d, size_t n) {
    const std::string text = cstr(d, n);
    uint32_t v = 0xDEADBEEF;
    if (NetConfig::parseIpv4(text.c_str(), &v)) {
        char back[16];
        NetConfig::formatIpv4(v, back, sizeof(back));
        FUZZ_CHECK(text == back);
    } else {
        FUZZ_CHECK(v == 0xDEADBEEF);
    }
    if (n >= 4) {
        char t[16];
        NetConfig::formatIpv4(u32(d), t, sizeof(t));
        uint32_t w = 0;
        FUZZ_CHECK(NetConfig::parseIpv4(t, &w) && w == u32(d));
    }
    FUZZ_CHECK(!NetConfig::parseIpv4(nullptr, &v));
    FUZZ_CHECK(!NetConfig::parseIpv4(text.c_str(), nullptr));
}

void staticIp(const uint8_t* d, size_t n) {
    if (n < 12) return;
    StaticIp ip {};
    ip.enabled = true;
    ip.ip = u32(d);
    ip.mask = u32(d + 4);
    ip.gateway = u32(d + 8);
    // Masks are mostly prefixes in practice; byte 12 makes one from a prefix length.
    if (n > 12 && (d[12] & 0x80)) ip.mask = (d[12] & 63) >= 32 ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> (d[12] & 63));
    int prefix = 0;
    while (prefix < 32 && (ip.mask & (0x80000000u >> prefix))) ++prefix;
    const bool contiguous = prefix == 32 ? ip.mask == 0xFFFFFFFFu : ip.mask == (prefix ? ~(0xFFFFFFFFu >> prefix) : 0);
    const uint32_t host = ~ip.mask;
    const bool ok = contiguous && prefix >= 1 && prefix <= 30 && ip.ip != 0 && ip.gateway != 0 && ip.ip != ip.gateway &&
                    (ip.ip & ip.mask) == (ip.gateway & ip.mask) && (ip.ip & host) != 0 && (ip.ip & host) != host &&
                    (ip.gateway & host) != 0 && (ip.gateway & host) != host;
    FUZZ_CHECK((NetConfig::checkStaticIp(ip) == NetConfig::IpCheck::Ok) == ok);
}

bool printable(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] < 0x20 || s[i] > 0x7E) return false;
    }
    return true;
}
bool hex64(const std::string& s) {
    if (s.size() != 64) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

void network(const uint8_t* d, size_t n) {
    if (n < 1) return;
    const uint8_t slot = d[0] % 3; // 2 is invalid
    // SSID and passphrase separated by the first NUL.
    const std::string rest(reinterpret_cast<const char*>(d + 1), n - 1);
    const size_t sep = rest.find('\0');
    const std::string ssid = rest.substr(0, sep);
    const std::string pass = sep == std::string::npos ? std::string() : cstr(d + 1 + sep + 1, n - 1 - sep - 1);

    NetSettings s;
    memset(&s, 0x5A, sizeof(s));
    NetSettings before;
    memcpy(&before, &s, sizeof(s));
    const NetConfig::KeyResult r = NetConfig::setNetwork(s, slot, ssid.c_str(), pass.c_str());
    if (r != NetConfig::KeyResult::Ok) {
        FUZZ_CHECK(memcmp(&before, &s, sizeof(s)) == 0);
        if (slot > 1 || ssid.empty() || ssid.size() > 32) FUZZ_CHECK(r == NetConfig::KeyResult::BadSsid);
        else FUZZ_CHECK(r == NetConfig::KeyResult::BadPassphrase && !pass.empty() && !hex64(pass) &&
                        !(pass.size() >= 8 && pass.size() <= 63 && printable(pass)));
        return;
    }
    FUZZ_CHECK(slot <= 1 && !ssid.empty() && ssid.size() <= 32);
    const WifiNetwork& w = s.networks[slot];
    FUZZ_CHECK(memchr(w.ssid, 0, sizeof(w.ssid)) && ssid == w.ssid);
    FUZZ_CHECK(memchr(w.key, 0, sizeof(w.key)));
    const std::string key = w.key;
    if (pass.empty()) {
        FUZZ_CHECK(key.empty());
    } else if (hex64(pass)) {
        FUZZ_CHECK(key.size() == 64);
        for (size_t i = 0; i < 64; ++i) FUZZ_CHECK(key[i] == (char)tolower((unsigned char)pass[i]));
    } else {
        FUZZ_CHECK(key == pass && pass.size() >= 8 && pass.size() <= 63 && printable(pass));
    }
    // The other slot is untouched.
    FUZZ_CHECK(memcmp(&s.networks[1 - slot], &before.networks[1 - slot], sizeof(WifiNetwork)) == 0);
}

// Reads one escaped field up to an unescaped ';'; false if there is none.
bool field(const std::string& t, size_t* pos, std::string* out) {
    out->clear();
    while (*pos < t.size()) {
        const char c = t[(*pos)++];
        if (c == ';') return true;
        if (c == '\\') {
            if (*pos >= t.size()) return false;
            out->push_back(t[(*pos)++]);
        } else {
            FUZZ_CHECK(c != ',' && c != ':' && c != '"'); // specials are always escaped
            out->push_back(c);
        }
    }
    return false;
}

void qr(const uint8_t* d, size_t n) {
    if (n < 2) return;
    const bool open = d[0] & 1;
    const size_t outSize = d[1] % 160;
    const std::string rest(reinterpret_cast<const char*>(d + 2), n - 2);
    const size_t sep = rest.find('\0');
    const std::string ssid = rest.substr(0, sep);
    const std::string pass = sep == std::string::npos ? std::string() : cstr(d + 2 + sep + 1, n - 2 - sep - 1);

    std::vector<char> out(outSize ? outSize : 1, '#');
    const bool ok = NetConfig::wifiQrText(ssid.c_str(), pass.c_str(), open, out.data(), outSize);
    if (outSize == 0) {
        FUZZ_CHECK(!ok && out[0] == '#');
        return;
    }
    if (!ok) {
        FUZZ_CHECK(out[0] == '\0');
        return;
    }
    const std::string t(out.data());
    FUZZ_CHECK(t.size() < outSize);
    const std::string head = open ? "WIFI:T:nopass;S:" : "WIFI:T:WPA;S:";
    FUZZ_CHECK(t.compare(0, head.size(), head) == 0);
    size_t pos = head.size();
    std::string s;
    FUZZ_CHECK(field(t, &pos, &s) && s == ssid);
    if (!open) {
        FUZZ_CHECK(t.compare(pos, 2, "P:") == 0);
        pos += 2;
        std::string p;
        FUZZ_CHECK(field(t, &pos, &p) && p == pass);
    }
    FUZZ_CHECK(pos + 1 == t.size() && t[pos] == ';');
}

void names(const uint8_t* d, size_t n) {
    const std::string s = cstr(d, n);
    bool name = !s.empty() && s.size() <= 32 && s[0] != '-' && s[s.size() - 1] != '-';
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) name = false;
    }
    FUZZ_CHECK(NetConfig::validDeviceName(s.c_str()) == name);
    FUZZ_CHECK(NetConfig::validPassphrase(s.c_str()) == (s.size() >= 8 && s.size() <= 63 && printable(s)));
    FUZZ_CHECK(NetConfig::validSsid(s.c_str()) == (!s.empty() && s.size() <= 32));
    FUZZ_CHECK(NetConfig::isPmkHex(s.c_str()) == hex64(s));
    FUZZ_CHECK(!NetConfig::validDeviceName(nullptr) && !NetConfig::validPassphrase(nullptr));
}

int run(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    const uint8_t* d = data + 1;
    const size_t n = size - 1;
    switch (data[0] % 5) {
        case 0: ipv4(d, n); break;
        case 1: staticIp(d, n); break;
        case 2: network(d, n); break;
        case 3: qr(d, n); break;
        default: names(d, n); break;
    }
    return 0;
}

} // namespace fuzz_net_settings

FUZZ_TARGET(net_settings, fuzz_net_settings::run)
