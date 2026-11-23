// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h> // IDF 5
#else
#include <esp_system.h> // IDF 4.4
#endif
#include <dhcpserver/dhcpserver.h>
#include <bootloader_random.h>
#include <utils/logger.h>
#include <utils/ipv4.h>
#include "ip_address.h"
#include <utils/sanitize.h>
#include <constants.h>
#include <remote/lxi_ident.h>

namespace {

DNSServer dnsServer;

constexpr uint8_t AP_IP[4] = {192, 168, 4, 1};
constexpr uint8_t AP_MAX_STATIONS = 4;
constexpr uint8_t OPEN_AP_MAX_STATIONS = 2; // W-S2
constexpr uint32_t UI_SCAN_TIMEOUT_MS = 15000;
// Highest TX power level of esp_wifi_set_max_tx_power() (21 dBm); the driver limits it to the
// calibrated maximum of the PHY data (20 dBm on the ESP32).
constexpr int8_t TX_POWER_MAX = 84;


uint32_t hwRandom() { return esp_random(); }

// D38 / E-45: no modem sleep and full TX power after every start of the radio. With modem
// sleep the client wakes only at the beacon interval (pings of 220-390 ms, VISA timeouts).
// The Arduino core applies its stored sleep type at every STA start, so it is stored first;
// the driver calls cover a client that already runs and the access point.
void radioAwake() {
    WiFi.setSleep(WIFI_PS_NONE);
    if (esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK) Logger::error("WIFI", "Power save off failed");
    if (esp_wifi_set_max_tx_power(TX_POWER_MAX) != ESP_OK) Logger::error("WIFI", "TX power not set");
    int8_t power = 0;
    if (LOG_DEBUG && esp_wifi_get_max_tx_power(&power) == ESP_OK) Logger::debugValue("WIFI", "TX power (0.25 dBm)", power);
}

static_assert(WifiPolicy::Reason::AUTH_EXPIRE == WIFI_REASON_AUTH_EXPIRE, "reason");
static_assert(WifiPolicy::Reason::FOURWAY_HANDSHAKE_TIMEOUT == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT, "reason");
static_assert(WifiPolicy::Reason::NO_AP_FOUND == WIFI_REASON_NO_AP_FOUND, "reason");
static_assert(WifiPolicy::Reason::AUTH_FAIL == WIFI_REASON_AUTH_FAIL, "reason");
static_assert(WifiPolicy::Reason::HANDSHAKE_TIMEOUT == WIFI_REASON_HANDSHAKE_TIMEOUT, "reason");

// Written by the WiFi task (events), read in loop(); single aligned words. The client state is
// taken from these flags instead of polling the driver and lwIP in every loop() (AF11, PERF-1).
volatile uint16_t lastDisconnectReason = 0;
volatile bool staHasIp = false;    // GOT_IP seen, no disconnect or LOST_IP since
volatile uint32_t gotIpEvents = 0; // counts GOT_IP (a DHCP renewal may change the address)
void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            staHasIp = true;
            gotIpEvents = gotIpEvents + 1;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            lastDisconnectReason = info.wifi_sta_disconnected.reason;
            staHasIp = false;
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        case ARDUINO_EVENT_WIFI_STA_STOP: staHasIp = false; break;
        default: break;
    }
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
    WiFi.setSleep(WIFI_PS_NONE); // stored for every STA start (D38)
    WiFi.onEvent(onWifiEvent);
    NetConfig::apSsid(mac_, status_.apSsid, sizeof(status_.apSsid));
    apply(s);
}

void WifiManager::apply(const NetSettings& s) {
    settings_ = s;
    ++revision_;
    // Start over: the next update() builds the interfaces from the new settings.
    if (staOn_ || apOn_) {
        if (dnsOn_) dnsServer.stop();
        if (mdnsOn_) MDNS.end();
        WiFi.mode(WIFI_OFF);
    }
    dnsOn_ = mdnsOn_ = staOn_ = apOn_ = hadIp_ = scanning_ = false;
    openFallbackConfirmed_ = openApTimedOut_ = false;
    noIpSinceMs_ = apIdleSinceMs_ = millis();
    conn_.reset();
    char line[64];
    snprintf(line, sizeof(line), "Mode %s, name %s", modeName(s.mode), s.deviceName);
    Logger::info("WIFI", line);
}

void WifiManager::applyAp(const NetSettings& s) {
    memcpy(settings_.apPassword, s.apPassword, sizeof(settings_.apPassword));
    settings_.apOpen = s.apOpen;
    ++revision_;
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
    ++revision_;
}

void WifiManager::startSta_() {
    staOn_ = true;
    hadIp_ = scanning_ = false;
    staHasIp = false;
    noIpSinceMs_ = millis();
    conn_.reset();
    const StaticIp& ip = settings_.staticIp;
    if (ip.enabled && NetConfig::checkStaticIp(ip) == NetConfig::IpCheck::Ok) {
        WiFi.config(toIpAddress(ip.ip), toIpAddress(ip.gateway), toIpAddress(ip.mask),
                    toIpAddress(ip.dns ? ip.dns : ip.gateway));
    } else {
        WiFi.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0), IPAddress((uint32_t)0));
    }
}

void WifiManager::stopSta_() {
    staOn_ = hadIp_ = scanning_ = false;
    conn_.reset();
    WiFi.scanDelete();
}

void WifiManager::startAp_(bool open) {
    // The radio runs (WiFi.mode() was called), so esp_random() is a true random source.
    if (!open && !NetConfig::validPassphrase(settings_.apPassword)) {
        NetConfig::generateApPassword(hwRandom, settings_.apPassword);
        ++revision_;
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
    apStations_ = 0;
    stationsPolled_ = false;
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
    const char* const names[2] = {nets[0].ssid, nets[1].ssid};
    const int found = WiFi.scanComplete();
    for (int i = 0; i < found; ++i) {
        const String seen = WiFi.SSID(i);
        const uint8_t raised = WifiPolicy::mergeSeen(names, seen.c_str(), WiFi.RSSI(i), rssi);
        for (uint8_t k = 0; k < 2; ++k) {
            if (raised & (1u << k)) auth[k] = WiFi.encryptionType(i);
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
        const bool seen = rssi[0] != WifiPolicy::NOT_SEEN || rssi[1] != WifiPolicy::NOT_SEEN;
        conn_.scanResult(seen);
        if (!seen) {
            char line[80];
            snprintf(line, sizeof(line), "No stored network among %d seen; next scan in %lu s", found,
                     (unsigned long)(conn_.scanIntervalMs(apStations_ > 0) / 1000));
            Logger::info("WIFI", line);
        }
        WiFi.scanDelete();
    }
    const bool stored[2] = {nets[0].ssid[0] != '\0', nets[1].ssid[0] != '\0'};
    const int slot = conn_.choose(stored, rssi);
    if (slot < 0) return;
    const WifiNetwork& n = nets[slot];
    const bool open = n.key[0] == '\0';
    // W-S15: at least WPA2 unless the network was stored as open on purpose (W-E7 warning).
    WiFi.setMinSecurity(open ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK);
    lastDisconnectReason = 0;
    status_.lastFailure = Failure::None;
    ++revision_;
    WiFi.begin(n.ssid, open ? nullptr : n.key);
    conn_.attemptStarted(slot, millis());
    char ssid[33];
    Sanitize::text(n.ssid, strnlen(n.ssid, 32), ssid, sizeof(ssid)); // SEC-6
    char line[64];
    snprintf(line, sizeof(line), "Connecting to %s", ssid);
    Logger::info("WIFI", line);
}

// The addresses and the network name are read once per address (AF11), not in every loop().
void WifiManager::readAddresses_() {
    seenGotIp_ = gotIpEvents;
    staIp_ = fromIpAddress(WiFi.localIP());
    staMask_ = fromIpAddress(WiFi.subnetMask());
    staGateway_ = fromIpAddress(WiFi.gatewayIP());
    staDns_ = fromIpAddress(WiFi.dnsIP());
    wifi_ap_record_t ap {};
    memset(staSsid_, 0, sizeof(staSsid_));
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        memcpy(staSsid_, ap.ssid, 32);
        staRssi_ = ap.rssi;
    } else {
        staRssi_ = 0;
    }
    rssiPolledMs_ = millis();
}

void WifiManager::onGotIp_() {
    hadIp_ = true;
    conn_.connected();
    readAddresses_();
    char ip[16];
    Ipv4::format(staIp_, ip, sizeof(ip));
    char ssid[33];
    Sanitize::text(staSsid_, strnlen(staSsid_, 32), ssid, sizeof(ssid)); // SEC-6
    char line[96];
    snprintf(line, sizeof(line), "Connected to %s, %s, RSSI %d dBm", ssid, ip, (int)staRssi_);
    Logger::info("WIFI", line);
}

void WifiManager::update() {
    const uint32_t now = millis();
    // Observe the client.
    if (staOn_) {
        // Field reads only (status and event flag): the D14 check stays in every loop().
        const bool connected = staHasIp && WiFi.status() == WL_CONNECTED;
        if (connected && !hadIp_) {
            onGotIp_();
        } else if (connected && gotIpEvents != seenGotIp_) {
            readAddresses_(); // DHCP renewal with a new address
        } else if (!connected && hadIp_) {
            hadIp_ = false;
            noIpSinceMs_ = now;
            Logger::info("WIFI", "Connection lost; searching");
        }
        if (!connected) {
            if (conn_.poll(now) == WifiPolicy::Connector::Step::TimedOut) {
                WiFi.disconnect(false);
                char line[64];
                snprintf(line, sizeof(line), "Network not reached (reason %u)", (unsigned)lastDisconnectReason);
                Logger::info("WIFI", line);
                status_.lastFailure = WifiPolicy::classifyFailure(lastDisconnectReason);
                ++revision_;
            }
            const bool storedNets[2] = {settings_.networks[0].ssid[0] != '\0', settings_.networks[1].ssid[0] != '\0'};
            if (!conn_.attempting() && uiScan_ != ScanState::Running) {
                const int sc = WiFi.scanComplete();
                if (sc >= 0 || (scanning_ && sc == WIFI_SCAN_FAILED)) {
                    scanning_ = false;
                    tryConnect_();
                } else if (sc != WIFI_SCAN_RUNNING && conn_.scanDue(now, storedNets, apOn_ && apStations_ > 0)) {
                    conn_.scanStarted(now);
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
        if (WifiPolicy::pollDue(now, stationsPolledMs_, stationsPolled_)) {
            wifi_sta_list_t list {};
            apStations_ = esp_wifi_ap_get_sta_list(&list) == ESP_OK ? (uint8_t)list.num : 0;
            stationsPolledMs_ = now;
            stationsPolled_ = true;
        }
        if (apStations_ > 0) apIdleSinceMs_ = now;
        else if (apIsOpen_ && WifiPolicy::openApIdleExpired(now, apIdleSinceMs_)) openApTimedOut_ = true;
    }
    const WifiPolicy::Inputs in {settings_.mode, hadIp_, now - noIpSinceMs_, settings_.apOpen, openFallbackConfirmed_,
                                 openApTimedOut_};
    applyTargets_(WifiPolicy::decide(in));
    if (dnsOn_) dnsServer.processNextRequest();
    updateStatus_(now);
}

void WifiManager::startScan() {
    if (uiScan_ == ScanState::Running) return;
    WiFi.scanDelete();
    // Scanning needs the client interface; without it, switch it on for the scan only.
    uiScanTempMode_ = !staOn_;
    if (uiScanTempMode_) {
        if (!apOn_) WiFi.setHostname(settings_.deviceName);
        WiFi.mode(apOn_ ? WIFI_AP_STA : WIFI_STA);
        radioAwake();
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
    conn_.scanEnded(millis()); // the connect loop scans again later
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
    if (status_.fallback != t.fallback || status_.needsOpenConfirmation != t.needsOpenConfirmation) ++revision_;
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
    if (want != WIFI_OFF) radioAwake();
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

// Field copies only; signal strength at most once per second (AF11). revision() counts changes.
void WifiManager::updateStatus_(uint32_t now) {
    if (hadIp_ && WifiPolicy::pollDue(now, rssiPolledMs_, true)) {
        wifi_ap_record_t ap {};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) staRssi_ = ap.rssi;
        rssiPolledMs_ = now;
    }
    Status n = status_; // a copy has the same padding, so memcmp tells a change
    n.mode = settings_.mode;
    n.staConnected = hadIp_;
    if (hadIp_) {
        memcpy(n.ssid, staSsid_, sizeof(n.ssid));
        n.rssi = staRssi_;
        n.staIp = staIp_;
        n.staMask = staMask_;
        n.staGateway = staGateway_;
        n.staDns = staDns_;
    } else {
        memset(n.ssid, 0, sizeof(n.ssid));
        n.rssi = 0;
        n.staIp = n.staMask = n.staGateway = n.staDns = 0;
    }
    n.connecting = conn_.attempting();
    n.apActive = apOn_;
    n.apOpen = apOn_ && apIsOpen_;
    n.apStations = apOn_ ? apStations_ : 0;
    n.apIp = apOn_ ? (uint32_t)AP_IP[0] << 24 | (uint32_t)AP_IP[1] << 16 | (uint32_t)AP_IP[2] << 8 | AP_IP[3] : 0;
    if (memcmp(&n, &status_, sizeof(n)) != 0) {
        status_ = n;
        ++revision_;
    }
}
