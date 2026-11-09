#pragma once

#include <stddef.h>
#include <stdint.h>

// Networks found by a scan for the WIFI page (ARBEITSPLAN 3.4, W-S9): hidden networks dropped,
// one entry per SSID (the strongest), sorted by signal, at most MAX entries, each with the raw
// SSID (to connect) and a sanitized one (to show). Pure, host-tested.
struct ScanEntry {
    char ssid[33];  // as received (1..32 bytes), used to connect
    char shown[33]; // Sanitize::text of ssid, the only form that reaches LVGL
    int8_t rssi;
    bool secure;    // not an open network
};

class ScanList {
public:
    static constexpr uint8_t MAX = 15;

    void clear() { count_ = 0; }
    // One scan result; len is the SSID length in bytes (0 = hidden: ignored).
    void add(const char* ssid, size_t len, int8_t rssi, bool secure);
    uint8_t count() const { return count_; }
    const ScanEntry& at(uint8_t i) const { return entries_[i]; }

private:
    ScanEntry entries_[MAX] {};
    uint8_t count_ = 0;
};
