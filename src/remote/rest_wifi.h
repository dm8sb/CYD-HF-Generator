#pragma once

#include <stddef.h>
#include <stdint.h>
#include <net/net_settings.h>
#include <net/scan_list.h>
#include "rest_json.h"

// REST mapping of the WiFi settings (S55, ARBEITSPLAN 3.6, W-E9, W-S1, W-S12): POST /api/wifi
// into a Change (types and sizes, in the async_tcp task), the Change onto the current settings
// (values, with the rules of NetConfig, in loop()), GET /api/wifi and the scan answer. Keys are
// write-only: no answer ever holds a WiFi key, the AP password or the web password. Pure and
// host-tested; error codes as in rest_json.h.
//
// POST /api/wifi body, every key optional, at least one:
//   "mode": "off" | "client" | "ap"
//   "networks": [{"slot":0|1,"ssid":"1..32 bytes","password":"" | 8..63 printable | 64 hex}] (<= 2)
//       password "" = open network; without "password" a changed SSID becomes open, an
//       unchanged one keeps its key
//   "delete": [0|1, ...] (<= 2), applied before "networks"
//   "name": RFC 1123 device name
//   "staticIp": {"enabled":bool,"ip","mask","gateway","dns":"a.b.c.d" or "" (dns)}; missing
//       fields keep their value; checked with NetConfig::checkStaticIp when enabled
//   "ap": {"open":bool,"password":"8..63 printable"}
// Errors: wrong JSON type -104, unknown key -108, nothing given -109, more than two entries
// -222, values outside the lists or rules (slot, SSID length, password, name, address text,
// mask) -224, static IP not plausible (same as gateway, outside the subnet, network or
// broadcast address, zero) -221 with the reason.
namespace RestWifi {

struct NetworkEntry {
    uint8_t slot;
    char ssid[33];
    bool hasPassword;
    char password[65];
};

struct Change {
    bool hasMode;
    WifiMode mode;
    uint8_t netCount;
    NetworkEntry nets[2];
    uint8_t deleteMask; // bit per slot
    bool hasName;
    char name[33];
    bool hasStaticIp;
    bool sipHasEnabled;
    bool sipEnabled;
    bool sipHas[4];     // ip, mask, gateway, dns
    char sip[4][16];
    bool hasAp;
    bool apHasOpen;
    bool apOpen;
    bool apHasPassword;
    char apPassword[64];
};

RestJson::Reply parseChange(const char* body, size_t len, Change* out);
// The change on top of cur (next = cur first); all-or-nothing: on an error next is cur.
RestJson::Reply merge(const Change& c, const NetSettings& cur, NetSettings* next);
// Mode Off over the web is final (ARBEITSPLAN 3.6 point 2): no new connection to confirm over.
inline bool isFinal(const NetSettings& next) { return next.mode == WifiMode::Off; }
// POST /api/wifi/confirm: the body is {}.
RestJson::Reply parseConfirm(const char* body, size_t len);
// Overwrites the passwords of a Change.
void wipe(Change* c);

// GET /api/wifi: {"mode","networks":[{"ssid","key":"set|open|none"} x2],
// "ap":{"ssid","open","passwordSet"},"name","staticIp":{"enabled","ip","mask","gateway","dns"}}.
// SSIDs through Sanitize. 0 if it does not fit.
size_t buildSettings(const NetSettings& s, const char* apSsid, char* out, size_t outSize);
static constexpr size_t SETTINGS_SIZE = 512;

// GET /api/wifi/scan: {"state":"running|done|failed","networks":[{"ssid","rssi","secure"}]};
// networks only with "done" (list may be nullptr otherwise), shown (sanitized) SSIDs.
size_t buildScan(const char* state, const ScanList* list, char* out, size_t outSize);
static constexpr size_t SCAN_SIZE = 2048; // 15 entries with escaped 32-byte SSIDs

}
