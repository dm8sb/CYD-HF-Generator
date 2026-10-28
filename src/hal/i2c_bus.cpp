#include "i2c_bus.h"
#include <Wire.h>
#include <config.h>
#include <pins.h>
#include <utils/logger.h>

namespace I2CBus {

static bool open_ = false;
static I2CMap map_ = I2CMap::Default;

static int sdaFor(I2CMap m) { return m == I2CMap::Default ? PIN_I2C_A : PIN_I2C_B; }
static int sclFor(I2CMap m) { return m == I2CMap::Default ? PIN_I2C_B : PIN_I2C_A; }

// The internal pull-up stays on, modules without their own pull-ups need it to release a line.
static constexpr uint8_t OPEN_DRAIN_PULLUP = OUTPUT_OPEN_DRAIN | PULLUP;

static void clockLine(int pin, bool high) {
    digitalWrite(pin, high ? HIGH : LOW);
    delayMicroseconds(10); // rise time with ~45 kOhm
}

// A reset during a read (EN button, upload, brownout) can leave the SI5351 holding SDA
// low; it has no bus timeout. Nine clocks and a STOP release it. Which pin is SCL is
// unknown, so both are clocked in turn; on the real SDA this only looks like START/STOP.
static void recoverLines_() {
    pinMode(PIN_I2C_A, INPUT_PULLUP);
    pinMode(PIN_I2C_B, INPUT_PULLUP);
    delayMicroseconds(10);
    if (digitalRead(PIN_I2C_A) == HIGH && digitalRead(PIN_I2C_B) == HIGH) return;

    Logger::info("I2C", "Line held low; releasing bus");
    for (uint8_t i = 0; i < 2; ++i) {
        const int scl = i == 0 ? PIN_I2C_A : PIN_I2C_B;
        const int sda = i == 0 ? PIN_I2C_B : PIN_I2C_A;
        digitalWrite(scl, HIGH);
        pinMode(scl, OPEN_DRAIN_PULLUP);
        for (uint8_t n = 0; n < 9; ++n) {
            clockLine(scl, false);
            clockLine(scl, true);
        }
        // STOP: SDA rises while SCL is high.
        clockLine(scl, false);
        digitalWrite(sda, LOW);
        pinMode(sda, OPEN_DRAIN_PULLUP);
        clockLine(scl, true);
        clockLine(sda, true);
        pinMode(scl, INPUT_PULLUP);
        pinMode(sda, INPUT_PULLUP);
    }
}

static void scan_() {
    uint8_t found = 0;
    char line[48];
    for (uint8_t addr = 1; addr < 127; ++addr) {
        const uint8_t r = probe(addr);
        if (r == 5) {
            Logger::error("I2C", "Scan aborted: line held low");
            break;
        }
        if (r == 0) {
            snprintf(line, sizeof(line), "Found device at 0x%02X", addr);
            Logger::info("I2C", line);
            found++;
        }
    }
    Logger::value("I2C", "Devices found", found);
}

bool begin() {
    recoverLines_();
    open_ = false;
    const bool ok = open(I2CMap::Default);
    if (LOG_DEBUG && ok) scan_();
    return ok;
}

bool open(I2CMap m) {
    if (open_ && m == map_) {
        Wire.setClock(I2C_DETECT_HZ);
        return true;
    }
    // end() is a no-op before the first begin(); begin() on a running bus would keep the old
    // pins. A failed end() leaves the driver installed without buffers: treat the bus as closed.
    map_ = m;
    open_ = Wire.end() && Wire.begin(sdaFor(m), sclFor(m), I2C_DETECT_HZ);
    if (open_) {
        Wire.setTimeOut(I2C_TIMEOUT_MS);
    } else {
        Logger::error("I2C", "Bus init failed");
    }
    return open_;
}

void setRunClock() {
    if (open_) Wire.setClock(I2C_FREQ_HZ);
}

bool isOpen() { return open_; }
I2CMap map() { return map_; }
int sdaPin() { return sdaFor(map_); }
int sclPin() { return sclFor(map_); }

uint8_t probe(uint8_t addr) {
    if (!open_) return 4;
    Wire.beginTransmission(addr);
    return Wire.endTransmission(true);
}

}
