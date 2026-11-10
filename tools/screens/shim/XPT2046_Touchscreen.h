#pragma once
// Host screen renderer (tools/screens): placeholder for the touch driver's XPT2046 member;
// tools/screens/host_stubs.cpp feeds scripted taps to LVGL instead.
#include <stdint.h>
class XPT2046_Touchscreen {
public:
    XPT2046_Touchscreen(uint8_t, uint8_t) {}
};
