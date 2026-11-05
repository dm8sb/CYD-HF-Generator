#include "status_text.h"

#include <constants.h>
#include <stdio.h>
#include <string.h>
#include <utils/frequency_utils.h>

namespace StatusText {

Text channel(ChannelStatus s) {
    switch (s) {
        case ChannelStatus::On: return {"ON", Tone::Ok};
        case ChannelStatus::Error: return {"! ERR", Tone::Error};
        case ChannelStatus::NoDevice: return {"! NO HW", Tone::Error};
        case ChannelStatus::Off: break;
    }
    return {"OFF", Tone::Muted};
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

}
