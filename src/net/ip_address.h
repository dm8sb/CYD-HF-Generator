// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <IPAddress.h>
#include <stdint.h>

// Device side of utils/ipv4.h: Arduino's IPAddress <-> host-order uint32_t
// (a.b.c.d = a << 24 | b << 16 | c << 8 | d). Not part of the host tests.
inline uint32_t fromIpAddress(const IPAddress& a) {
    return (uint32_t)a[0] << 24 | (uint32_t)a[1] << 16 | (uint32_t)a[2] << 8 | a[3];
}

inline IPAddress toIpAddress(uint32_t v) {
    return IPAddress((uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v);
}
