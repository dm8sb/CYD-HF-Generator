#include "scan_list.h"

#include <string.h>
#include <utils/sanitize.h>

void ScanList::add(const char* ssid, size_t len, int8_t rssi, bool secure) {
    if (!ssid || len == 0) return;
    if (len > 32) len = 32;
    // The same SSID again (several access points): keep the strongest.
    for (uint8_t i = 0; i < count_; ++i) {
        ScanEntry& e = entries_[i];
        if (strlen(e.ssid) == len && memcmp(e.ssid, ssid, len) == 0) {
            if (rssi <= e.rssi) return;
            // Remove and insert again below at the new position.
            memmove(&entries_[i], &entries_[i + 1], (count_ - i - 1) * sizeof(ScanEntry));
            --count_;
            break;
        }
    }
    // Position by signal, strongest first; a full list drops the weakest.
    uint8_t pos = 0;
    while (pos < count_ && entries_[pos].rssi >= rssi) ++pos;
    if (pos >= MAX) return;
    const uint8_t moved = (uint8_t)((count_ < MAX ? count_ : MAX - 1) - pos);
    memmove(&entries_[pos + 1], &entries_[pos], moved * sizeof(ScanEntry));
    if (count_ < MAX) ++count_;
    ScanEntry& e = entries_[pos];
    memset(&e, 0, sizeof(e));
    memcpy(e.ssid, ssid, len);
    e.ssid[len] = '\0';
    Sanitize::text(ssid, len, e.shown, sizeof(e.shown));
    e.rssi = rssi;
    e.secure = secure;
}
