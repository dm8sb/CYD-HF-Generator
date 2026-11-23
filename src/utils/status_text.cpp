// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "status_text.h"

#include <constants.h>
#include <stdio.h>
#include <string.h>
#include <utils/frequency_utils.h>
#include <utils/ipv4.h>
#include <utils/sanitize.h>

namespace StatusText {

Text channel(ChannelStatus s) {
    switch (s) {
        case ChannelStatus::On: return {"ON", Tone::Ok};
        case ChannelStatus::Error: return {"! ERR", Tone::Error};
        case ChannelStatus::NoDevice: return {"! NO HW", Tone::Error};
        case ChannelStatus::FreqTooLow:
        case ChannelStatus::FreqTooHigh: return {"WAIT", Tone::Warn};
        case ChannelStatus::Off: break;
    }
    return {"OFF", Tone::Muted};
}

static_assert(FREQ_MIN_HZ == 8000ULL && FREQ_MAX_HZ == 160000000ULL, "coupledWait() names the range");

const char* coupledWait(int64_t targetHz) {
    if (targetHz < (int64_t)FREQ_MIN_HZ) return "Waiting: CH1 + offset below 8 kHz";
    if (targetHz > (int64_t)FREQ_MAX_HZ) return "Waiting: CH1 + offset above 160 MHz";
    return "";
}

Text ch3(Ch3Status s) {
    switch (s) {
        case Ch3Status::Active: return {"ON", Tone::Ok};
        case Ch3Status::MasterOff:
        case Ch3Status::FreqTooLow:
        case Ch3Status::FreqTooHigh: return {"WAIT", Tone::Warn};
        case Ch3Status::Error: return {"! ERR", Tone::Error};
        case Ch3Status::NoDevice: return {"! NO HW", Tone::Error};
        case Ch3Status::Off: break;
    }
    return {"OFF", Tone::Muted};
}

static void append(char* out, size_t outSize, size_t& n, const char* token) {
    if (n >= outSize) return;
    const int w = snprintf(out + n, outSize - n, "%s%s", n ? " | " : "", token);
    if (w > 0) n += (size_t)w;
}

void outputErrorText(uint8_t ch, uint8_t master, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    static_assert(HW_CHECK_INTERVAL_MS == 2000, "the texts name the retry interval");
    if (ch == 3) {
        snprintf(out, outSize, "CH3: IQ setup failed, CLK2 stays off. CH%u keeps running. Retrying every 2 s.",
                 (unsigned)master);
    } else {
        snprintf(out, outSize, "CH%u: frequency setup failed. Retrying every 2 s.", (unsigned)ch);
    }
}

Tone ch3Info(Ch3Status s, uint8_t master, uint64_t iqMinHz, uint64_t iqMaxHz, char* out, size_t outSize) {
    if (!out || outSize == 0) return Tone::Muted;
    out[0] = '\0';
    char limit[12];
    switch (s) {
        case Ch3Status::Off:
            snprintf(out, outSize, "Off  |  follows CH%u", (unsigned)master);
            return Tone::Muted;
        case Ch3Status::Active: return Tone::Muted;
        case Ch3Status::MasterOff:
            snprintf(out, outSize, "Master CH%u is off: tap to switch on", (unsigned)master);
            return Tone::Warn;
        case Ch3Status::FreqTooLow:
            FrequencyUtils::formatIqLimit(iqMinHz, true, 3, false, limit, sizeof(limit));
            snprintf(out, outSize, "Waiting: CH%u below %s MHz", (unsigned)master, limit);
            return Tone::Warn;
        case Ch3Status::FreqTooHigh:
            FrequencyUtils::formatIqLimit(iqMaxHz, false, 3, false, limit, sizeof(limit));
            snprintf(out, outSize, "Waiting: CH%u above %s MHz", (unsigned)master, limit);
            return Tone::Warn;
        case Ch3Status::Error:
            snprintf(out, outSize, "IQ setup failed, retrying");
            return Tone::Error;
        case Ch3Status::NoDevice:
            snprintf(out, outSize, "SI5351 not found");
            return Tone::Error;
    }
    return Tone::Muted;
}

const char* rangeNote(uint64_t hz) {
    if (hz > 100000000ULL) return ">100MHz: low level";
    if (hz < 1000000ULL) return "<1MHz: harmonics";
    return "";
}

Tone modeSummary(TuningMode tuning, Spread spread, uint8_t driveMa, OffState off, char* out, size_t outSize) {
    if (!out || outSize == 0) return Tone::Muted;
    out[0] = '\0';
    size_t n = 0;
    if (off == OffState::High) append(out, outSize, n, "OFF=High");
    else if (off == OffState::HighZ) append(out, outSize, n, "OFF=High-Z");
    if (spread != Spread::Off) {
        char t[16];
        // Compact for the mode line: "SSC -0.5%" (the dropdown keeps "-0.5 %").
        snprintf(t, sizeof(t), "SSC %s", FrequencyUtils::spreadLabel(spread));
        char* sp = strstr(t, " %");
        if (sp) memmove(sp, sp + 1, strlen(sp));
        append(out, outSize, n, t);
    }
    if (tuning == TuningMode::LowNoise) append(out, outSize, n, "Low noise");
    else if (tuning == TuningMode::Integer) append(out, outSize, n, "Integer");
    if (driveMa != DEFAULT_DRIVE_MA) {
        char t[8];
        snprintf(t, sizeof(t), "%umA", (unsigned)driveMa);
        append(out, outSize, n, t);
    }
    return off == OffState::High ? Tone::Warn : Tone::Muted;
}

Tone wifiLine(const WifiLine& w, char* out, size_t outSize) {
    if (w.pendingS) {
        // AF16: the settings shown are the web change; they stay only when confirmed.
        snprintf(out, outSize, "Web change pending (%u s)\nConfirm in the browser", (unsigned)w.pendingS);
        return Tone::Warn;
    }
    if (w.mode == WifiMode::Off) {
        snprintf(out, outSize, "WiFi is Off. Choose Client in CONFIG > WiFi.");
        return Tone::Muted;
    }
    if (w.staConnected) {
        char ssid[33];
        char ip[16];
        Sanitize::text(w.ssid, strlen(w.ssid), ssid, sizeof(ssid));
        Ipv4::format(w.staIp, ip, sizeof(ip));
        snprintf(out, outSize, "Connected %s\n%s, %s.local", ssid, ip, w.deviceName);
        return Tone::Ok;
    }
    if (w.mode == WifiMode::AccessPoint) {
        snprintf(out, outSize, "Access point %s active", w.apSsid);
        return Tone::Muted;
    }
    if (w.apActive) {
        snprintf(out, outSize, "Fallback: AP %s active\nStill searching", w.apSsid);
        return Tone::Warn;
    }
    if (w.connecting) {
        snprintf(out, outSize, "Connecting...");
        return Tone::Muted;
    }
    switch (w.failure) {
        case WifiPolicy::Failure::WrongPassword: snprintf(out, outSize, "Wrong password"); return Tone::Error;
        case WifiPolicy::Failure::NotFound: snprintf(out, outSize, "Network not found"); return Tone::Warn;
        case WifiPolicy::Failure::Other: snprintf(out, outSize, "Connection failed"); return Tone::Warn;
        case WifiPolicy::Failure::None: break;
    }
    snprintf(out, outSize, w.networkStored ? "Searching..." : "No network stored. Tap Change.");
    return Tone::Muted;
}

}
