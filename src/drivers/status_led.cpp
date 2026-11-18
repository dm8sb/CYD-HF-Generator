// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "status_led.h"
#include "led_color.h"
#include <pins.h>

namespace StatusLed {

namespace {
LedColor::Rgb shown {false, false, false};

void write(const LedColor::Rgb& c) {
    digitalWrite(PIN_LED_R, c.r ? LOW : HIGH);
    digitalWrite(PIN_LED_G, c.g ? LOW : HIGH);
    digitalWrite(PIN_LED_B, c.b ? LOW : HIGH);
    shown = c;
}
}

void begin() {
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    write(LedColor::of(PageId::Main, false, false));
}

void update(PageId page, bool identifying, bool lit) {
    const LedColor::Rgb c = LedColor::of(page, identifying, lit);
    if (c != shown) write(c);
}

}
