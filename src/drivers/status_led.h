// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <types.h>

// Board RGB LED (active low), driven by main in loop() (AF51b): the color comes from
// LedColor::of() (shown channel page, Identify), the pins are written only on a change.
namespace StatusLed {
void begin();
// page: Gui::page(); identifying / lit: AppController::identifying(), identifyLit().
void update(PageId page, bool identifying, bool lit);
}
