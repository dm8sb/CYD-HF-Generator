#include "wifi_policy.h"

namespace WifiPolicy {

Targets decide(const Inputs& in) {
    Targets t {false, false, false, false, false};
    switch (in.mode) {
        case WifiMode::Off:
            break;
        case WifiMode::AccessPoint:
            // An open AP goes off after 15 min without a station and stays off (W-S2).
            t.ap = !(in.apOpen && in.openApTimedOut);
            t.apOpen = in.apOpen;
            break;
        case WifiMode::Client:
            t.sta = true;
            // W-S3 / D14: never an AP while the client has an address.
            if (in.staHasIp || in.msWithoutIp < FALLBACK_AFTER_MS) break;
            if (in.apOpen && !in.openFallbackConfirmed) {
                t.needsOpenConfirmation = true;
                break;
            }
            if (in.apOpen && in.openApTimedOut) break;
            t.ap = true;
            t.apOpen = in.apOpen;
            t.fallback = true;
            break;
    }
    return t;
}

int chooseSlot(const bool stored[2], const int rssi[2], int lastFailed, uint8_t next) {
    if (!stored[0] && !stored[1]) return -1;
    const bool seen0 = stored[0] && rssi[0] > NOT_SEEN;
    const bool seen1 = stored[1] && rssi[1] > NOT_SEEN;
    if (seen0 && seen1) {
        if (lastFailed == 0) return 1;
        if (lastFailed == 1) return 0;
        return rssi[1] > rssi[0] ? 1 : 0;
    }
    if (seen0) return 0;
    if (seen1) return 1;
    // None seen: the stored ones in turn.
    const uint8_t first = next % 2;
    return stored[first] ? first : 1 - first;
}

}
