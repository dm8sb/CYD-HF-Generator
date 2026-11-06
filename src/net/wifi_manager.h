#pragma once

#include <stdint.h>
#include "net_settings.h"
#include "wifi_policy.h"

// Drives the radio from NetSettings (ARBEITSPLAN 3.3/3.4): Off, Client with up to two networks
// and the fallback AP, Access point. Runs in loop(): WiFi events only set flags, so nothing
// is called from the WiFi task. Not host-compiled; the decisions are in WifiPolicy.
//
// Client: asynchronous scan, the strongest stored network wins; if none is seen (hidden SSID)
// the stored networks are tried in turn. No WiFiMulti: its run() blocks for seconds and would
// trip the loop watchdog. AP: WPA2 with the random password (created on first use while the
// radio runs, so esp_random() is a true RNG), DHCP without router option, captive DNS, never
// together with a client address (D14).
class WifiManager {
public:
    struct Status {
        WifiMode mode;
        bool staConnected;
        char ssid[33];   // connected network
        int8_t rssi;
        uint32_t staIp;  // a.b.c.d as in StaticIp
        bool apActive;
        bool apOpen;
        bool fallback;
        bool needsOpenConfirmation;
        uint8_t apStations;
        uint32_t apIp;
        char apSsid[33];
    };

    // saveSettings is called when the manager changed the settings (AP password created).
    typedef bool (*SaveFn)(const NetSettings& s);

    void begin(const NetSettings& s, const uint8_t mac[6], SaveFn save);
    // New settings (from the WiFi pages); restarts what changed.
    void apply(const NetSettings& s);
    // The touch confirmed an open fallback AP (W-S2).
    void confirmOpenFallback() { openFallbackConfirmed_ = true; }
    void update();
    const Status& status() const { return status_; }
    const NetSettings& settings() const { return settings_; }

private:
    void applyTargets_(const WifiPolicy::Targets& t);
    void startAp_(bool open);
    void stopAp_();
    void startSta_();
    void stopSta_();
    void tryConnect_();
    void onGotIp_();
    void updateStatus_();

    NetSettings settings_ {};
    uint8_t mac_[6] {};
    SaveFn save_ = nullptr;
    Status status_ {};
    bool staOn_ = false;
    bool apOn_ = false;
    bool apIsOpen_ = false;
    bool hadIp_ = false;
    bool mdnsOn_ = false;
    bool dnsOn_ = false;
    bool openFallbackConfirmed_ = false;
    bool openApTimedOut_ = false;
    uint32_t noIpSinceMs_ = 0;
    uint32_t apIdleSinceMs_ = 0;
    uint32_t lastScanMs_ = 0;
    uint32_t attemptMs_ = 0;
    bool attempting_ = false;
    uint8_t nextSlot_ = 0;
    bool scanning_ = false;
};
