#pragma once

#include <stddef.h>
#include <stdint.h>
#include <services/storage.h>

// Network settings (WEB-HMI, ARBEITSPLAN 3.3 and 3.7) in their own NVS namespace "hfnet".
// Pure logic plus Storage: validation, defaults from the MAC, WPA2 key as PMK (W-S14),
// random AP password (W-E3), device name (W-E5, W-S16), static IP plausibility (W-S12).

enum class WifiMode : uint8_t { Off = 0, Client = 1, AccessPoint = 2 };

struct WifiNetwork {
    char ssid[33]; // 1..32 bytes, "" = unused
    char key[65];  // 64 hex digits (PMK), "" = open network
};

struct StaticIp {
    bool enabled;
    uint32_t ip; // a.b.c.d as (a << 24) | (b << 16) | (c << 8) | d
    uint32_t mask;
    uint32_t gateway;
    uint32_t dns; // 0: the gateway
};

struct NetSettings {
    WifiMode mode;                         // W-E1, default Off (W-E20)
    WifiNetwork networks[2];               // W-E7: at most two
    char apPassword[64];                   // WPA2 passphrase, "" until the AP is first switched on
    bool apOpen;                           // only on request, with warning (W-E3, W-S2)
    char deviceName[33];                   // RFC 1123 (W-E5)
    StaticIp staticIp;                     // W-E4
};

namespace NetConfig {

static constexpr const char* NAMESPACE = "hfnet";
// The WiFi driver's own NVS namespace; erased by the factory reset (W-S14).
static constexpr const char* DRIVER_NAMESPACE = "nvs.net80211";
static constexpr size_t AP_PASSWORD_LEN = 12;

// Defaults: Off, no networks, no AP password yet, closed AP, name "cyd-hf-<last 4 hex of MAC>".
void defaults(NetSettings& s, const uint8_t mac[6]);
// "cyd-hf-1a2b" and the AP SSID "CYD-HF-1A2B" from the last two MAC bytes.
void defaultDeviceName(const uint8_t mac[6], char* out, size_t outSize);
void apSsid(const uint8_t mac[6], char* out, size_t outSize);

// RFC 1123 label: a-z, 0-9, '-', 1..32 characters, not starting or ending with '-'.
bool validDeviceName(const char* name);
// 1..32 bytes (any bytes except NUL; shown only through sanitize, S53).
bool validSsid(const char* ssid);
// WPA2 passphrase: 8..63 printable ASCII characters.
bool validPassphrase(const char* pass);
bool isPmkHex(const char* key);

// 12 characters from an alphabet without 0/O, 1/l/I (W-E3). rand32 must be a true random
// source (esp_random() with the radio on).
void generateApPassword(uint32_t (*rand32)(), char out[AP_PASSWORD_LEN + 1]);

enum class KeyResult : uint8_t { Ok, BadSsid, BadPassphrase };
// Network slot 0/1: an empty passphrase means an open network, 8..63 characters are turned
// into the PMK (the passphrase is never stored), 64 hex digits are taken as PMK.
KeyResult setNetwork(NetSettings& s, uint8_t slot, const char* ssid, const char* passphrase);
void clearNetwork(NetSettings& s, uint8_t slot);

enum class IpCheck : uint8_t { Ok, BadMask, ZeroAddress, SameAsGateway, OutsideSubnet, NetworkAddress, BroadcastAddress };
// W-S12: contiguous mask /1../30, address and gateway in one subnet, different, neither the
// network nor the broadcast address.
IpCheck checkStaticIp(const StaticIp& ip);
bool parseIpv4(const char* text, uint32_t* out);
void formatIpv4(uint32_t ip, char* out, size_t outSize);

// NVS "hfnet". load() replaces invalid values by the defaults; false if nothing usable was
// stored (defaults then). save() writes everything.
bool load(Storage& storage, NetSettings& s, const uint8_t mac[6]);
bool save(Storage& storage, const NetSettings& s);
// Factory reset (W-S14): erases "hfnet" and the WiFi driver's namespace.
bool eraseAll();

}
