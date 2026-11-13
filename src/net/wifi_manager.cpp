// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <esp_netif.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h> // IDF 5
#else
#include <esp_system.h> // IDF 4.4
#endif
#include <dhcpserver/dhcpserver.h>
#include <bootloader_random.h>
#include <utils/logger.h>
#include <utils/sanitize.h>
#include <constants.h>
#include <remote/lxi_ident.h>

namespace {

DNSServer dnsServer;

constexpr uint8_t AP_IP[4] = {192, 168, 4, 1};
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t SCAN_INTERVAL_MS = 15000;
constexpr uint8_t AP_MAX_STATIONS = 4;
constexpr uint8_t OPEN_AP_MAX_STATIONS = 2; // W-S2
constexpr uint32_t UI_SCAN_TIMEOUT_MS = 15000;

IPAddress toIp(uint32_t v) { return IPAddress((uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v); }
uint32_t fromIp(const IPAddress& a) { return (uint32_t)a[0] << 24 | (uint32_t)a[1] << 16 | (uint32_t)a[2] << 8 | a[3]; }

uint32_t hwRandom() { return esp_random(); }

// Last disconnect reason of the client (WiFi task writes, loop() reads; one byte).
volatile uint8_t lastDisconnectReason = 0;
void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) lastDisconnectReason = info.wifi_sta_disconnected.reason;
}

const char* authName(wifi_auth_mode_t a) {
    switch (a) {
        case WIFI_AUTH_OPEN: return "open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "other";
    }
}

const char* modeName(WifiMode m) {
    switch (m) {
        case WifiMode::Client: return "Client";
        case WifiMode::AccessPoint: return "Access point";
        default: return "Off";
    }
}

// The AP's DHCP server offers no router (no default route through the device, W-S3) but
// itself as DNS server (captive DNS).
void configureApDhcp() {
    esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) {
        Logger::error("WIFI", "AP netif not found; DHCP options unchanged");
        return;
    }
    esp_netif_dhcps_stop(ap);
    dhcps_offer_t router = 0;
    esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &router, sizeof(router));
    esp_netif_dns_info_t dns {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ESP_IP4TOADDR(AP_IP[0], AP_IP[1], AP_IP[2], AP_IP[3]);
    esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns);
    dhcps_offer_t offerDns = OFFER_DNS;
    esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offerDns, sizeof(offerDns));
    if (esp_netif_dhcps_start(ap) != ESP_OK) Logger::error("WIFI", "AP DHCP server start failed");
}

}

void WifiManager::begin(const NetSettings& s, const uint8_t mac[6], SaveFn save) {
    memcpy(mac_, mac, 6);
    save_ = save;
    WiFi.persistent(false); // credentials only in "hfnet" as PMK (W-S14)
    WiFi.setAutoReconnect(false);
    WiFi.onEvent(onWifiEvent);
    NetConfig::apSsid(mac_, status_.apSsid, sizeof(status_.apSsid));
    apply(s);
}

void WifiManager::apply(const NetSettings& s) {
    settings_ = s;
    // Start over: the next update() builds the interfaces from the new settings.
    if (staOn_ || apOn_) {
        if (dnsOn_) dnsServer.stop();
        if (mdnsOn_) MDNS.end();
        WiFi.mode(WIFI_OFF);
    }
    dnsOn_ = mdnsOn_ = staOn_ = apOn_ = hadIp_ = attempting_ = scanning_ = false;
    openFallbackConfirmed_ = openApTimedOut_ = false;
    noIpSinceMs_ = apIdleSinceMs_ = millis();
    lastScanMs_ = 0;
    nextSlot_ = 0;
    lastFailedSlot_ = attemptSlot_ = -1;
    char line[64];
    snprintf(line, sizeof(line), "Mode %s, name %s", modeName(s.mode), s.deviceName);
    Logger::info("WIFI", line);
}

void WifiManager::applyAp(const NetSettings& s) {
    memcpy(settings_.apPassword, s.apPassword, sizeof(settings_.apPassword));
    settings_.apOpen = s.apOpen;
    openApTimedOut_ = false;
    // Stopped here, started again by the next update() with the new data.
    if (apOn_) stopAp_();
}

void WifiManager::newApPassword(char out[NetConfig::AP_PASSWORD_LEN + 1]) {
    // esp_random() is a true random source only while the radio (or the bootloader entropy
    // source) runs.
    const bool radio = staOn_ || apOn_;
    if (!radio) bootloader_random_enable();
    NetConfig::generateApPassword(hwRandom, out);
    if (!radio) bootloader_random_disable();
}

void WifiManager::randomBytes(uint8_t* out, size_t n) {
    if (!out) return;
    const bool radio = staOn_ || apOn_;
    if (!radio) bootloader_random_enable();
    esp_fill_random(out, n);
    if (!radio) bootloader_random_disable();
}

void WifiManager::applyWebPassword(const char* record) {
    snprintf(settings_.webPassword, sizeof(settings_.webPassword), "%s", record ? record : "");
}

void WifiManager::startSta_() {
    staOn_ = true;
    hadIp_ = attempting_ = scanning_ = false;
    noIpSinceMs_ = millis();
    lastScanMs_ = 0;
    const StaticIp& ip = settings_.staticIp;
    if (ip.enabled && NetConfig::checkStaticIp(ip) == NetConfig::IpCheck::Ok) {
        WiFi.config(toIp(ip.ip), toIp(ip.gateway), toIp(ip.mask), toIp(ip.dns ? ip.dns : ip.gateway));
    } else {
        WiFi.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0), IPAddress((uint32_t)0));
    }
}

void WifiManager::stopSta_() {
    staOn_ = hadIp_ = attempting_ = scanning_ = false;
    WiFi.scanDelete();
}

void WifiManager::startAp_(bool open) {
    // The radio runs (WiFi.mode() was called), so esp_random() is a true random source.
    if (!open && !NetConfig::validPassphrase(settings_.apPassword)) {
        NetConfig::generateApPassword(hwRandom, settings_.apPassword);
        if (save_ && !save_(settings_)) Logger::error("WIFI", "Saving the AP password failed");
        Logger::info("WIFI", "AP password created");
    }
    const bool ok = WiFi.softAP(status_.apSsid, open ? nullptr : settings_.apPassword, 1, 0,
                                open ? OPEN_AP_MAX_STATIONS : AP_MAX_STATIONS);
    const IPAddress apIp(AP_IP[0], AP_IP[1], AP_IP[2], AP_IP[3]);
    WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
    configureApDhcp();
    dnsOn_ = dnsServer.start(53, "*", apIp);
    apOn_ = ok;
    apIsOpen_ = open;
    apIdleSinceMs_ = millis();
    char line[80];
    snprintf(line, sizeof(line), "%s AP %s %s%s", status_.fallback ? "Fallback" : "", status_.apSsid,
             open ? "open" : "WPA2", ok ? "" : " failed");
    Logger::info("WIFI", line[0] == ' ' ? line + 1 : line);
}

void WifiManager::stopAp_() {
    if (dnsOn_) dnsServer.stop();
    dnsOn_ = false;
    apOn_ = false;
    WiFi.softAPdisconnect(false);
    Logger::info("WIFI", hadIp_ ? "AP off (client has an address)" : "AP off");
}

void WifiManager::tryConnect_() {
    const WifiNetwork* nets = settings_.networks;
    const bool any = nets[0].ssid[0] || nets[1].ssid[0];
    if (!any) return;
    // Strongest access point per stored network in the last scan.
    int rssi[2] = {WifiPolicy::NOT_SEEN, WifiPolicy::NOT_SEEN};
    wifi_auth_mode_t auth[2] = {WIFI_AUTH_OPEN, WIFI_AUTH_OPEN};
    const int found = WiFi.scanComplete();
    for (int i = 0; i < found; ++i) {
        const String seen = WiFi.SSID(i);
        for (uint8_t k = 0; k < 2; ++k) {
            if (nets[k].ssid[0] && seen == nets[k].ssid && WiFi.RSSI(i) > rssi[k]) {
                rssi[k] = WiFi.RSSI(i);
                auth[k] = WiFi.encryptionType(i);
            }
        }
    }
    if (found >= 0) {
        for (uint8_t k = 0; k < 2; ++k) {
            if (rssi[k] == WifiPolicy::NOT_SEEN) continue;
            char ssid[33];
            Sanitize::text(nets[k].ssid, strnlen(nets[k].ssid, 32), ssid, sizeof(ssid)); // SEC-6
            char line[80];
            snprintf(line, sizeof(line), "Seen %s, RSSI %d dBm, %s", ssid, rssi[k], authName(auth[k]));
            Logger::info("WIFI", line);
        }
        if (rssi[0] == WifiPolicy::NOT_SEEN && rssi[1] == WifiPolicy::NOT_SEEN) {
            char line[64];
            snprintf(line, sizeof(line), "No stored network among %d seen", found);
            Logger::info("WIFI", line);
        }
        WiFi.scanDelete();
    }
    const bool stored[2] = {nets[0].ssid[0] != '\0', nets[1].ssid[0] != '\0'};
    const int slot = WifiPolicy::chooseSlot(stored, rssi, lastFailedSlot_, nextSlot_);
    if (slot < 0) return;
    nextSlot_ = (uint8_t)((slot + 1) % 2);
    attemptSlot_ = (int8_t)slot;
    const WifiNetwork& n = nets[slot];
    const bool open = n.key[0] == '\0';
    // W-S15: at least WPA2 unless the network was stored as open on purpose (W-E7 warning).
    WiFi.setMinSecurity(open ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK);
    lastDisconnectReason = 0;
    status_.lastFailure = Failure::None;
    WiFi.begin(n.ssid, open ? nullptr : n.key);
    attempting_ = true;
    attemptMs_ = millis();
    char ssid[33];
    Sanitize::text(n.ssid, strnlen(n.ssid, 32), ssid, sizeof(ssid)); // SEC-6
    char line[64];
    snprintf(line, sizeof(line), "Connecting to %s", ssid);
    Logger::info("WIFI", line);
}

void WifiManager::onGotIp_() {
    hadIp_ = true;
    lastFailedSlot_ = -1;
    attempting_ = false;
    char ip[16];
    NetConfig::formatIpv4(fromIp(WiFi.localIP()), ip, sizeof(ip));
    const String raw = WiFi.SSID();
    char ssid[33];
    Sanitize::text(raw.c_str(), raw.length(), ssid, sizeof(ssid)); // SEC-6
    char line[96];
    snprintf(line, sizeof(line), "Connected to %s, %s, RSSI %d dBm", ssid, ip, (int)WiFi.RSSI());
    Logger::info("WIFI", line);
}

void WifiManager::update() {
    const uint32_t now = millis();
    // Observe the client.
    if (staOn_) {
        const bool connected = WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0;
        if (connected && !hadIp_) {
            onGotIp_();
        } else if (!connected && hadIp_) {
            hadIp_ = false;
            noIpSinceMs_ = now;
            Logger::info("WIFI", "Connection lost; searching");
        }
        if (!connected) {
            if (attempting_ && now - attemptMs_ >= CONNECT_TIMEOUT_MS) {
                attempting_ = false;
                WiFi.disconnect(false);
                char line[64];
                snprintf(line, sizeof(line), "Network not reached (reason %u)", (unsigned)lastDisconnectReason);
                Logger::info("WIFI", line);
                // 201 no AP found; 2 auth expired, 15 4-way handshake timeout, 202 auth failed,
                // 204 handshake timeout: most often a wrong password.
                lastFailedSlot_ = attemptSlot_;
                const uint8_t r = lastDisconnectReason;
                status_.lastFailure = r == 201 ? Failure::NotFound
                                      : (r == 2 || r == 15 || r == 202 || r == 204) ? Failure::WrongPassword
                                                                                   : Failure::Other;
            }
            if (!attempting_ && uiScan_ != ScanState::Running) {
                const int sc = WiFi.scanComplete();
                if (sc >= 0 || (scanning_ && sc == WIFI_SCAN_FAILED)) {
                    scanning_ = false;
                    tryConnect_();
                } else if (sc != WIFI_SCAN_RUNNING && (lastScanMs_ == 0 || now - lastScanMs_ >= SCAN_INTERVAL_MS)) {
                    lastScanMs_ = now;
                    scanning_ = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
                    if (!scanning_) tryConnect_();
                }
            }
        }
    }
    // Scan of the WIFI page.
    if (uiScan_ == ScanState::Running) {
        const int sc = WiFi.scanComplete();
        if (sc >= 0) finishUiScan_(sc);
        else if (sc == WIFI_SCAN_FAILED || now - uiScanStartMs_ >= UI_SCAN_TIMEOUT_MS) finishUiScan_(-1);
    }
    // Open AP without station (W-S2).
    if (apOn_) {
        if (WiFi.softAPgetStationNum() > 0) apIdleSinceMs_ = now;
        else if (apIsOpen_ && now - apIdleSinceMs_ >= WifiPolicy::OPEN_AP_IDLE_OFF_MS) openApTimedOut_ = true;
    }
    const WifiPolicy::Inputs in {settings_.mode, hadIp_, now - noIpSinceMs_, settings_.apOpen, openFallbackConfirmed_,
                                 openApTimedOut_};
    applyTargets_(WifiPolicy::decide(in));
    if (dnsOn_) dnsServer.processNextRequest();
    updateStatus_();
}

void WifiManager::startScan() {
    if (uiScan_ == ScanState::Running) return;
    WiFi.scanDelete();
    // Scanning needs the client interface; without it, switch it on for the scan only.
    uiScanTempMode_ = !staOn_;
    if (uiScanTempMode_) {
        if (!apOn_) WiFi.setHostname(settings_.deviceName);
        WiFi.mode(apOn_ ? WIFI_AP_STA : WIFI_STA);
    }
    uiScanStartMs_ = millis();
    uiScan_ = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING ? ScanState::Running : ScanState::Failed;
    if (uiScan_ == ScanState::Failed) finishUiScan_(-1);
    else Logger::info("WIFI", "Scan started");
}

void WifiManager::finishUiScan_(int found) {
    scanList_.clear();
    for (int i = 0; i < found; ++i) {
        const String ssid = WiFi.SSID(i);
        scanList_.add(ssid.c_str(), ssid.length(), (int8_t)WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    uiScan_ = found >= 0 ? ScanState::Done : ScanState::Failed;
    if (uiScanTempMode_) WiFi.mode(apOn_ ? WIFI_AP : WIFI_OFF);
    uiScanTempMode_ = false;
    lastScanMs_ = millis(); // the connect loop scans again later
    char line[48];
    snprintf(line, sizeof(line), found >= 0 ? "Scan: %d networks" : "Scan failed", found);
    Logger::info("WIFI", line);
}

// DNS-SD (W-E12, A-78): raw SCPI, VXI-11 (the portmapper port, as LXI defines it), LXI and the
// web page, so that `lxi discover -m` and other LXI tools find the device. The TXT record
// names manufacturer and model, but no version, serial number or MAC (W-S16).
static void announceServices() {
    MDNS.addService("scpi-raw", "tcp", SCPI_TCP_PORT);
    MDNS.addService("vxi-11", "tcp", 111);
    MDNS.addService("lxi", "tcp", 80);
    MDNS.addServiceTxt("lxi", "tcp", "txtvers", "1");
    MDNS.addServiceTxt("lxi", "tcp", "Manufacturer", LxiIdent::MANUFACTURER);
    MDNS.addServiceTxt("lxi", "tcp", "Model", LxiIdent::MODEL);
    MDNS.addService("http", "tcp", 80);
}

void WifiManager::applyTargets_(const WifiPolicy::Targets& t) {
    status_.fallback = t.fallback;
    status_.needsOpenConfirmation = t.needsOpenConfirmation;
    const wifi_mode_t want = t.sta && t.ap ? WIFI_AP_STA : t.sta ? WIFI_STA : t.ap ? WIFI_AP : WIFI_OFF;
    const wifi_mode_t have = staOn_ && apOn_ ? WIFI_AP_STA : staOn_ ? WIFI_STA : apOn_ ? WIFI_AP : WIFI_OFF;
    if (want == have && (!t.ap || t.apOpen == apIsOpen_)) return;
    // The AP goes first: with a client address it must not run a moment longer (D14).
    if (apOn_ && (!t.ap || t.apOpen != apIsOpen_)) stopAp_();
    if (staOn_ && !t.sta) stopSta_();
    if (have == WIFI_OFF && want != WIFI_OFF) WiFi.setHostname(settings_.deviceName); // before the netif starts
    WiFi.mode(want);
    if (t.sta && !staOn_) startSta_();
    if (t.ap && !apOn_) startAp_(t.apOpen);
    const bool mdnsWanted = want != WIFI_OFF;
    if (mdnsWanted && !mdnsOn_) {
        mdnsOn_ = MDNS.begin(settings_.deviceName);
        if (!mdnsOn_) Logger::error("WIFI", "mDNS start failed");
        else announceServices();
    } else if (!mdnsWanted && mdnsOn_) {
        MDNS.end();
        mdnsOn_ = false;
    }
    if (want == WIFI_OFF) Logger::info("WIFI", "Radio off");
}

void WifiManager::updateStatus_() {
    status_.mode = settings_.mode;
    status_.staConnected = hadIp_;
    if (hadIp_) {
        snprintf(status_.ssid, sizeof(status_.ssid), "%s", WiFi.SSID().c_str());
        status_.rssi = (int8_t)WiFi.RSSI();
        status_.staIp = fromIp(WiFi.localIP());
        status_.staMask = fromIp(WiFi.subnetMask());
        status_.staGateway = fromIp(WiFi.gatewayIP());
        status_.staDns = fromIp(WiFi.dnsIP());
    } else {
        status_.ssid[0] = '\0';
        status_.rssi = 0;
        status_.staIp = status_.staMask = status_.staGateway = status_.staDns = 0;
    }
    status_.connecting = attempting_;
    status_.apActive = apOn_;
    status_.apOpen = apOn_ && apIsOpen_;
    status_.apStations = apOn_ ? (uint8_t)WiFi.softAPgetStationNum() : 0;
    status_.apIp = apOn_ ? (uint32_t)AP_IP[0] << 24 | (uint32_t)AP_IP[1] << 16 | (uint32_t)AP_IP[2] << 8 | AP_IP[3] : 0;
}
