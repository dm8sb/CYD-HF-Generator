// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <types.h>
#include <net/net_settings.h>
#include <net/wifi_policy.h>

// State texts of MAIN and the page headers (audit U-1): both show the actual state, the
// switches show the request. Errors start with "!", so they differ from ON also without color
// (audit U-19); the GUI fills every state except OFF with its tone color. Pure functions, host-tested in test_input_logic.
namespace StatusText {

enum class Tone : uint8_t { Muted, Ok, Warn, Error };

struct Text {
    const char* text;
    Tone tone;
};

Text channel(ChannelStatus s);
// Why a coupled CH2 waits (E-52): "Waiting: CH1 + offset below 8 kHz" / "... above 160 MHz"
// for a target (CH1 + offset in Hz) outside FREQ_MIN_HZ..FREQ_MAX_HZ, "" inside.
const char* coupledWait(int64_t targetHz);
Text ch3(Ch3Status s);

// Settings that change the signal of a channel compared to the defaults (audit U-2), e.g.
// "OFF=High | SSC -0.5% | Low noise | 8mA" (compact, no space before units); empty with the defaults. spread: what applies
// to this channel (PLL A: CH1, CH3 with master CH1). Tone::Warn with off state High (a
// switched-off output carries DC), otherwise Tone::Muted. Returns the tone.
Tone modeSummary(TuningMode tuning, Spread spread, uint8_t driveMa, OffState off, char* out, size_t outSize);

// Dialog text of an output error (audit U-21): channel, what is off, what keeps running and the
// retry. ch 1/2: standalone output; ch 3: IQ pair with master (1/2).
void outputErrorText(uint8_t ch, uint8_t master, char* out, size_t outSize);

// Status line of the CH3 page (AF48): why CH3 waits, its error, or "Off  |  follows CHn"; ""
// while active (the page then shows the output line). iqMinHz/iqMaxHz are the effective limits
// (narrower with spread spectrum), shown in MHz with 3 decimals rounded towards the inside.
// Tone::Warn while waiting, Tone::Error for an error, otherwise Tone::Muted.
Tone ch3Info(Ch3Status s, uint8_t master, uint64_t iqMinHz, uint64_t iqMaxHz, char* out, size_t outSize);

// Hint for the channel page outside 1-100 MHz (E-16), short enough for the mode line next to
// a few special modes: "" inside the range.
const char* rangeNote(uint64_t hz);

// Status line of the WIFI page (AF47): what the WiFi does now and why the last attempt failed.
// Foreign names (ssid) are sanitized here. Tone: Ok connected, Warn for a pending web change,
// the fallback AP, a missing network or a failed connection, Error for a wrong password.
struct WifiLine {
    uint16_t pendingS;         // a web network change waits this long (AF16), 0 = none
    WifiMode mode;
    bool staConnected;
    const char* ssid;          // connected network
    uint32_t staIp;
    const char* deviceName;
    const char* apSsid;
    bool apActive;
    bool connecting;
    WifiPolicy::Failure failure;
    bool networkStored;        // at least one network is stored
};
Tone wifiLine(const WifiLine& w, char* out, size_t outSize);

}
