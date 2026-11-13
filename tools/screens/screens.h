// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Host screen renderer (tools/render_images.py screens): the real GUI (src/gui) on the host
// with the fakes of test/fakes. host_stubs.cpp replaces the hardware classes the GUI holds
// (DisplayPanel, TouchDriver, WifiManager, StatusLed, BuildInfo); main.cpp sets sample state,
// navigates and writes the frames. Not part of the firmware or the host tests.

#include <stdint.h>
#include <net/wifi_manager.h>

namespace Screens {
static constexpr int W = 320;
static constexpr int H = 240;

// RGB565 frame as LVGL flushed it (LV_COLOR_16_SWAP 0).
extern uint16_t frame[H * W];

// Scripted touch: read by TouchDriver::readLvgl().
struct Touch {
    bool pressed;
    int16_t x;
    int16_t y;
};
extern Touch touch;

// Copied into the WifiManager by begin() (sample values, never real networks).
extern WifiManager::Status wifiStatus;

// Build line of the CONFIG info card (set from the command line).
extern const char* buildBranch;
extern const char* buildCommit;
extern const char* buildTime;
}
