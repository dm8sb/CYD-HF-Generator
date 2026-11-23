// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Explicit saves (SYSTem:SAVE, POST /api/save) per source (AF17, E-402, SEC-V-3): at most one
// NVS save per MIN_INTERVAL_MS and source; a request within the interval is merged silently and
// carried out MIN_INTERVAL_MS after the previous save (so at the latest that long after the
// request). Without it, "FREQ ...;:SYST:SAVE" in a loop wrote the flash about 10 times per
// second from anyone in the network. Pure; host-tested through the AppController.
class SaveLimiter {
public:
    static constexpr uint32_t MIN_INTERVAL_MS = 10000;
    static constexpr uint8_t SOURCES = 8; // >= the number of Source values
    enum class Decision : uint8_t { SaveNow, Deferred };

    Decision request(uint8_t source, uint32_t nowMs) {
        Slot& s = slots_[source < SOURCES ? source : SOURCES - 1];
        if (!s.saved || nowMs - s.lastMs >= MIN_INTERVAL_MS) {
            s.saved = true;
            s.lastMs = nowMs;
            s.pending = false;
            return Decision::SaveNow;
        }
        s.pending = true;
        return Decision::Deferred;
    }

    // A merged save is due now (one save covers every source that is due).
    bool due(uint32_t nowMs) {
        bool any = false;
        for (Slot& s : slots_) {
            if (s.pending && nowMs - s.lastMs >= MIN_INTERVAL_MS) {
                s.pending = false;
                s.lastMs = nowMs;
                any = true;
            }
        }
        return any;
    }

private:
    struct Slot {
        bool saved;
        bool pending;
        uint32_t lastMs;
    };
    Slot slots_[SOURCES] {};
};
