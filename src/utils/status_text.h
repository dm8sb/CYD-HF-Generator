// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <types.h>

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
Text ch3(Ch3Status s);

// Settings that change the signal of a channel compared to the defaults (audit U-2), e.g.
// "OFF=High | SSC -0.5% | Low noise | 8mA" (compact, no space before units); empty with the defaults. spread: what applies
// to this channel (PLL A: CH1, CH3 with master CH1). Tone::Warn with off state High (a
// switched-off output carries DC), otherwise Tone::Muted. Returns the tone.
Tone modeSummary(TuningMode tuning, Spread spread, uint8_t driveMa, OffState off, char* out, size_t outSize);

// Dialog text of an output error (audit U-21): channel, what is off, what keeps running and the
// retry. ch 1/2: standalone output; ch 3: IQ pair with master (1/2).
void outputErrorText(uint8_t ch, uint8_t master, char* out, size_t outSize);

// Hint for the channel page outside 1-100 MHz (E-16), short enough for the mode line next to
// a few special modes: "" inside the range.
const char* rangeNote(uint64_t hz);

}
