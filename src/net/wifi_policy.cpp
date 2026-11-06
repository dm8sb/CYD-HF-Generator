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

}
