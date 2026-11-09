#include "net_settings.h"

#include <stdio.h>
#include <string.h>
#include "request_guard.h"

static_assert(sizeof(((NetSettings*)nullptr)->webPassword) == WebPassword::RECORD_SIZE, "web password record size");

namespace NetConfig {

namespace {

void copy(char* dst, size_t size, const char* src) {
    snprintf(dst, size, "%s", src ? src : "");
}

const char* const KEY_SSID[2] = {"n0_ssid", "n1_ssid"};
const char* const KEY_PMK[2] = {"n0_key", "n1_key"};

}

void defaultDeviceName(const uint8_t mac[6], char* out, size_t outSize) {
    snprintf(out, outSize, "cyd-hf-%02x%02x", mac[4], mac[5]);
}

void apSsid(const uint8_t mac[6], char* out, size_t outSize) {
    snprintf(out, outSize, "CYD-HF-%02X%02X", mac[4], mac[5]);
}

void defaults(NetSettings& s, const uint8_t mac[6]) {
    memset(&s, 0, sizeof(s));
    s.mode = WifiMode::Off;
    s.apOpen = false;
    defaultDeviceName(mac, s.deviceName, sizeof(s.deviceName));
}

bool validDeviceName(const char* n) {
    if (!n) return false;
    const size_t len = strlen(n);
    if (len < 1 || len > 32 || n[0] == '-' || n[len - 1] == '-') return false;
    for (size_t i = 0; i < len; ++i) {
        const char c = n[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return true;
}

bool validSsid(const char* ssid) {
    if (!ssid) return false;
    const size_t len = strlen(ssid);
    return len >= 1 && len <= 32;
}

bool validPassphrase(const char* p) {
    if (!p) return false;
    const size_t len = strlen(p);
    if (len < 8 || len > 63) return false;
    for (size_t i = 0; i < len; ++i) {
        if (p[i] < 0x20 || p[i] > 0x7E) return false;
    }
    return true;
}

bool isPmkHex(const char* k) {
    if (!k || strlen(k) != 64) return false;
    for (int i = 0; i < 64; ++i) {
        const char c = k[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

void generateApPassword(uint32_t (*rand32)(), char out[AP_PASSWORD_LEN + 1]) {
    // 57 characters without 0, O, 1, l, I (W-E3); rejection sampling keeps it uniform.
    static const char ALPHABET[] = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static const uint32_t N = sizeof(ALPHABET) - 1;
    static const uint32_t LIMIT = 0xFFFFFFFFu - (0xFFFFFFFFu % N);
    for (size_t i = 0; i < AP_PASSWORD_LEN; ++i) {
        uint32_t r;
        do {
            r = rand32();
        } while (r >= LIMIT);
        out[i] = ALPHABET[r % N];
    }
    out[AP_PASSWORD_LEN] = '\0';
}

KeyResult setNetwork(NetSettings& s, uint8_t slot, const char* ssid, const char* passphrase) {
    if (slot > 1) return KeyResult::BadSsid;
    if (!validSsid(ssid)) return KeyResult::BadSsid;
    WifiNetwork n {};
    copy(n.ssid, sizeof(n.ssid), ssid);
    if (!passphrase || !*passphrase) {
        n.key[0] = '\0';
    } else if (isPmkHex(passphrase)) {
        for (int i = 0; i < 64; ++i) {
            const char c = passphrase[i];
            n.key[i] = (c >= 'A' && c <= 'F') ? (char)(c + 32) : c;
        }
        n.key[64] = '\0';
    } else if (validPassphrase(passphrase)) {
        copy(n.key, sizeof(n.key), passphrase);
    } else {
        return KeyResult::BadPassphrase;
    }
    s.networks[slot] = n;
    return KeyResult::Ok;
}

void clearNetwork(NetSettings& s, uint8_t slot) {
    if (slot <= 1) memset(&s.networks[slot], 0, sizeof(WifiNetwork));
}

IpCheck checkStaticIp(const StaticIp& c) {
    const uint32_t m = c.mask;
    // Contiguous ones, /1 .. /30.
    if (m == 0 || ((~m + 1) & ~m) != 0 || (~m) < 3) return IpCheck::BadMask;
    if (c.ip == 0 || c.gateway == 0) return IpCheck::ZeroAddress;
    if (c.ip == c.gateway) return IpCheck::SameAsGateway;
    if ((c.ip & m) != (c.gateway & m)) return IpCheck::OutsideSubnet;
    for (uint32_t a : {c.ip, c.gateway}) {
        if ((a & ~m) == 0) return IpCheck::NetworkAddress;
        if ((a & ~m) == ~m) return IpCheck::BroadcastAddress;
    }
    return IpCheck::Ok;
}

const char* ipCheckText(IpCheck c) {
    switch (c) {
        case IpCheck::Ok: return "OK";
        case IpCheck::BadMask: return "The mask must be /1 to /30, e.g. 255.255.255.0.";
        case IpCheck::ZeroAddress: return "Address and gateway must be set.";
        case IpCheck::SameAsGateway: return "The address equals the gateway.";
        case IpCheck::OutsideSubnet: return "The gateway is not in the subnet of the address.";
        case IpCheck::NetworkAddress: return "Address or gateway is the network address.";
        case IpCheck::BroadcastAddress: return "Address or gateway is the broadcast address.";
    }
    return "Not accepted.";
}

bool parseIpv4(const char* t, uint32_t* out) {
    if (!t || !out) return false;
    uint32_t v = 0;
    for (int part = 0; part < 4; ++part) {
        if (*t < '0' || *t > '9') return false;
        uint32_t n = 0;
        int digits = 0;
        while (*t >= '0' && *t <= '9') {
            n = n * 10 + (uint32_t)(*t++ - '0');
            if (++digits > 3) return false;
        }
        if (n > 255 || (digits > 1 && t[-digits] == '0')) return false; // no leading zeros (octal)
        v = (v << 8) | n;
        if (part < 3 && *t++ != '.') return false;
    }
    if (*t) return false;
    *out = v;
    return true;
}

void formatIpv4(uint32_t ip, char* out, size_t outSize) {
    snprintf(out, outSize, "%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)((ip >> 16) & 255),
             (unsigned)((ip >> 8) & 255), (unsigned)(ip & 255));
}

bool webPasswordSet(const NetSettings& s) { return s.webPassword[0] != '\0'; }

bool setWebPassword(NetSettings& s, const char* password, size_t len, uint32_t (*rand32)()) {
    char record[sizeof(s.webPassword)];
    if (!WebPassword::hash(password, len, rand32, record)) return false;
    memcpy(s.webPassword, record, sizeof(record));
    return true;
}

void clearWebPassword(NetSettings& s) { memset(s.webPassword, 0, sizeof(s.webPassword)); }

bool load(Storage& st, NetSettings& s, const uint8_t mac[6]) {
    defaults(s, mac);
    const uint8_t mode = st.getU8("mode", 0xFF);
    if (mode == 0xFF) return false; // nothing stored
    s.mode = mode <= (uint8_t)WifiMode::AccessPoint ? (WifiMode)mode : WifiMode::Off;
    for (uint8_t i = 0; i < 2; ++i) {
        WifiNetwork n {};
        st.getString(KEY_SSID[i], n.ssid, sizeof(n.ssid));
        st.getString(KEY_PMK[i], n.key, sizeof(n.key));
        // A network needs an SSID; its key is empty (open), a passphrase or a PMK.
        if (validSsid(n.ssid) && (n.key[0] == '\0' || validPassphrase(n.key) || isPmkHex(n.key))) s.networks[i] = n;
    }
    char ap[sizeof(s.apPassword)];
    if (st.getString("ap_pw", ap, sizeof(ap)) && validPassphrase(ap)) copy(s.apPassword, sizeof(s.apPassword), ap);
    s.apOpen = st.getU8("ap_open", 0) == 1;
    char name[sizeof(s.deviceName)];
    if (st.getString("name", name, sizeof(name)) && validDeviceName(name)) copy(s.deviceName, sizeof(s.deviceName), name);
    StaticIp ip {};
    ip.enabled = st.getU8("sip_en", 0) == 1;
    ip.ip = st.getU32("sip_ip", 0);
    ip.mask = st.getU32("sip_mask", 0);
    ip.gateway = st.getU32("sip_gw", 0);
    ip.dns = st.getU32("sip_dns", 0);
    // An implausible static IP is kept as values but switched off (DHCP).
    if (ip.enabled && checkStaticIp(ip) != IpCheck::Ok) ip.enabled = false;
    s.staticIp = ip;
    // A damaged record is dropped (no password) rather than locking the web login for good.
    char web[sizeof(s.webPassword)];
    if (st.getString("web_pw", web, sizeof(web)) && WebPassword::wellFormed(web)) copy(s.webPassword, sizeof(s.webPassword), web);
    return true;
}

bool save(Storage& st, const NetSettings& s) {
    bool ok = st.putU8("mode", (uint8_t)s.mode);
    for (uint8_t i = 0; i < 2; ++i) {
        ok &= st.putString(KEY_SSID[i], s.networks[i].ssid);
        ok &= st.putString(KEY_PMK[i], s.networks[i].key);
    }
    ok &= st.putString("ap_pw", s.apPassword);
    ok &= st.putU8("ap_open", s.apOpen ? 1 : 0);
    ok &= st.putString("name", s.deviceName);
    ok &= st.putU8("sip_en", s.staticIp.enabled ? 1 : 0);
    ok &= st.putU32("sip_ip", s.staticIp.ip);
    ok &= st.putU32("sip_mask", s.staticIp.mask);
    ok &= st.putU32("sip_gw", s.staticIp.gateway);
    ok &= st.putU32("sip_dns", s.staticIp.dns);
    ok &= st.putString("web_pw", s.webPassword);
    return ok;
}

namespace {
bool appendEscaped(char* out, size_t outSize, size_t* pos, const char* text) {
    for (const char* p = text; *p; ++p) {
        const bool special = *p == '\\' || *p == ';' || *p == ',' || *p == ':' || *p == '"';
        if (*pos + (special ? 2 : 1) >= outSize) return false;
        if (special) out[(*pos)++] = '\\';
        out[(*pos)++] = *p;
    }
    out[*pos] = '\0';
    return true;
}
bool appendRaw(char* out, size_t outSize, size_t* pos, const char* text) {
    const size_t n = strlen(text);
    if (*pos + n >= outSize) return false;
    memcpy(out + *pos, text, n + 1);
    *pos += n;
    return true;
}
}

bool wifiQrText(const char* ssid, const char* password, bool open, char* out, size_t outSize) {
    if (!out || outSize == 0 || !ssid) return false;
    size_t pos = 0;
    out[0] = '\0';
    bool ok = appendRaw(out, outSize, &pos, open ? "WIFI:T:nopass;S:" : "WIFI:T:WPA;S:") &&
              appendEscaped(out, outSize, &pos, ssid);
    if (ok && !open) ok = appendRaw(out, outSize, &pos, ";P:") && appendEscaped(out, outSize, &pos, password ? password : "");
    if (ok) ok = appendRaw(out, outSize, &pos, ";;");
    if (!ok) out[0] = '\0';
    return ok;
}

bool eraseAll() {
    Storage net;
    Storage driver;
    const bool a = net.begin(NAMESPACE) && net.clear();
    const bool b = driver.begin(DRIVER_NAMESPACE) && driver.clear();
    return a && b;
}

}
