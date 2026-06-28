#include "i2c_bus.h"
#include <Wire.h>
#include <pins.h>
#include <utils/logger.h>

namespace I2CBus {
bool begin() {
    Serial.printf("[I2C] Initializing SDA=%d SCL=%d @ %luHz\n", PIN_I2C_SDA, PIN_I2C_SCL, (unsigned long)I2C_FREQ_HZ);
    bool ok = Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    Wire.setClock(I2C_FREQ_HZ);
    if (ok) Logger::info("I2C", "Bus init OK");
    else Logger::error("I2C", "Bus init failed");

    Logger::info("I2C", "Scanning bus...");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; ++addr) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission(true);
        if (err == 0) {
            Serial.printf("[I2C] Found device at 0x%02X\n", addr);
            found++;
        }
    }
    Logger::value("I2C", "Devices found", found);
    return ok;
}
}
