#pragma once

#include <Arduino.h>
#include <types.h>

// Board RGB LED (active low). Shows the active channel page: CH1 red, CH2 green, CH3 blue, else off.
namespace StatusLed {
void begin();
void showPage(PageId page);
// Identify (W-E28): all colors on (white) or off.
void showAll(bool on);
}
