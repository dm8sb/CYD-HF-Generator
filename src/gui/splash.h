// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Start screen at boot: title, key data and three running square waves in the channel colors
// (CH2 at twice the frequency, CH3 90 degrees behind CH1 like the IQ pair). It lies on the LVGL
// system layer above MAIN and the dialogs, so the boot goes on behind it; it fades out after
// durationMs or at the first tap.
namespace Splash {
void show(uint32_t durationMs);
bool shown();
}
