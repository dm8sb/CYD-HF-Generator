#pragma once

#include <Arduino.h>

// Which CN1 pin is SDA. Default: SDA = PIN_I2C_A, SCL = PIN_I2C_B.
enum class I2CMap : uint8_t { Default, Swapped };

// Owns Wire. Every transfer must check isOpen() first: in arduino-esp32 2.0.x a transfer
// on a bus that is not started leaks the Wire lock and the next one blocks forever.
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
}
