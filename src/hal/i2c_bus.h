// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>

// Which CN1 pin is SDA. Default: SDA = PIN_I2C_A, SCL = PIN_I2C_B.
enum class I2CMap : uint8_t { Default, Swapped };

// Owns Wire: the only file that calls it (C-14). Every transfer checks isOpen() first: in
// arduino-esp32 2.0.x and 3.x a transfer on a bus that is not started (or after end()) takes
// the Wire lock without giving it back, and the next begin() or end() blocks forever.
namespace I2CBus {
// Releases a line held low by the SI5351, then opens the default mapping at I2C_DETECT_HZ.
bool begin();
// Opens the bus with the given mapping at I2C_DETECT_HZ. Keeps the driver if the
// mapping is already open, so a short dropout does not re-initialize the bus.
bool open(I2CMap m);
void setRunClock();
bool isOpen();
I2CMap map();
int sdaPin();
int sclPin();
// Address-only write: 0 = ACK, 2 = NACK, 5 = timeout (line held low), 4 = bus not open.
uint8_t probe(uint8_t addr);
// Register block write (reg, then data) with STOP. false: bus not open, no ACK or timeout.
bool writeRegs(uint8_t addr, uint8_t reg, const uint8_t* data, uint8_t len);
// Register pointer write with repeated START, then len bytes. Both cores send the pointer only
// together with the read (endTransmission(false) just marks it), so a missing device shows as
// a short read. false: bus not open or fewer than len bytes.
bool readRegs(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t len);
}
