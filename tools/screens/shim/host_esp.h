// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
// Host screen renderer (tools/screens): on the device ESP comes with Arduino.h; the fake core
// of the host tests has none, so this header is force-included (-include in platformio.ini).
// The heap values of the CONFIG info card are fixed sample numbers.
#ifdef __cplusplus
#include <stdint.h>
struct HostEsp {
    uint32_t getFreeHeap() const;
    uint32_t getMinFreeHeap() const;
};
extern HostEsp ESP;
#endif
