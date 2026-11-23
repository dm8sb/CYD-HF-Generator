// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

// IPv4 addresses as host-order uint32_t (a.b.c.d = a << 24 | b << 16 | c << 8 | d) and their
// dotted-quad text. The one parser and formatter of the firmware (CQ-10): network settings,
// Host header, REST, SCPI/VXI-11 logs and the LXI identification all use it. Pure, host-tested
// and fuzzed; the conversion from Arduino's IPAddress stays in the device code (net/ip_address.h).
namespace Ipv4 {

// "255.255.255.255" plus NUL.
constexpr size_t TEXT_SIZE = 16;

// Accepts exactly the canonical dotted quad: four decimal parts 0..255, no sign, no blanks, no
// leading zeros (would be octal elsewhere), nothing after the fourth part. Writes *out only on
// success; false for a null pointer.
bool parse(const char* text, size_t len, uint32_t* out);
bool parse(const char* text, uint32_t* out); // NUL-terminated

// Writes the dotted quad NUL-terminated (truncated like snprintf when outSize < TEXT_SIZE).
// Returns true if it fit completely.
bool format(uint32_t ip, char* out, size_t outSize);

} // namespace Ipv4
