// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <types.h>

// What the board RGB LED shows (AF51b): the channel page on the display (CH1 red, CH2 green,
// CH3 blue, else off); while Identify runs (E-613) all three colors blink with the backlight.
// Pure, host-tested in test_status_view; StatusLed writes it.
namespace LedColor {

struct Rgb {
    bool r, g, b;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Rgb& o) const { return !(*this == o); }
};

// page: the page the GUI shows; identifying / lit: AppController::identifying(), identifyLit().
inline Rgb of(PageId page, bool identifying, bool lit) {
    if (identifying) return Rgb {lit, lit, lit};
    return Rgb {page == PageId::CH1, page == PageId::CH2, page == PageId::CH3};
}

}
