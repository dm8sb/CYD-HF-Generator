// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

// LXI identification (A-78): the XML document of GET /lxi/identification and the identity
// strings of the mDNS TXT record _lxi._tcp. Pure logic, host-tested (test_vxi11). The document
// follows the LXI InstrumentIdentification 1.0 schema as far as the device has the values; it
// carries no MAC address of its own (the serial number is the one *IDN? shows) and no commit.
namespace LxiIdent {

static constexpr const char* MANUFACTURER = "DM8SB";
static constexpr const char* MODEL = "CYD HF Generator";
static constexpr const char* DESCRIPTION = "3-channel SI5351 HF square-wave generator";
static constexpr const char* HOMEPAGE = "https://github.com/dm8sb/CYD-HF-Generator";
static constexpr size_t DOCUMENT_SIZE = 2048;

struct Info {
    const char* serial = nullptr;   // *IDN? field 3
    const char* version = nullptr;  // firmware version (*IDN? field 4)
    const char* name = nullptr;     // device name (W-E5), announced as <name>.local
    uint32_t ip = 0;                // the address the request came in on, a.b.c.d as in StaticIp
    uint32_t mask = 0;
    uint32_t gateway = 0;
    bool dhcp = true;
    uint16_t scpiPort = 5025;
};

// The document; returns its length, 0 if it does not fit into outSize (NUL included).
size_t build(const Info& info, char* out, size_t outSize);

} // namespace LxiIdent
