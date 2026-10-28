#pragma once

// Wire stand-in for the host unit tests: an SI5351A register model at 0x60, wired to
// CN1 straight or swapped. Every register write is logged, so tests can check what the
// driver sends.

#include <Arduino.h>
#include <pins.h>
#include <vector>

namespace Fake {

struct RegWrite {
    uint8_t reg;
    uint8_t value;
};

class Si5351Chip {
public:
    static constexpr uint8_t ADDR = 0x60;

    enum class Line : uint8_t { None, Sda, Scl };

    uint8_t regs[256];
    bool present = true;
    // Pins the module's SDA / SCL wires go to. Swap them for a reversed cable.
    int wiredSda = PIN_I2C_A;
    int wiredScl = PIN_I2C_B;
    // A line held low: every transfer times out (5); SCL low also costs the 13 ms HW timeout.
    Line lineLow = Line::None;
    // Weak pull-ups on the wrong assignment: every address ACKs, reads give 0xFF.
    bool ghostOnWrongPins = false;
    // Highest clock the bus works at (weak pull-ups), 0 = any. Faster transfers NACK.
    uint32_t maxClockHz = 0;
    // Another device at 0x60 instead of an SI5351: reads `foreignValue`, ignores writes (still logged).
    bool foreign = false;
    uint8_t foreignValue = 0x00;
    // Successful transfers left before the bus fails for good (-1 = never),
    // like a disconnected SDA line.
    int failAfter = -1;
    // Status reads that still report SYS_INIT after power-on.
    uint8_t initReads = 0;
    std::vector<RegWrite> writes;
    unsigned pllResets[2] = {0, 0}; // PLL A, PLL B
    unsigned failedTransfers = 0;   // attempted while failAfter == 0

    Si5351Chip() { powerOn(); }

    // Power-on state: all clocks powered down and disabled, nothing configured.
    void powerOn() {
        memset(regs, 0, sizeof(regs));
        regs[3] = 0xFF;
        for (uint8_t i = 16; i <= 23; ++i) regs[i] = 0x80;
        regs[183] = 0xD2;
        initReads = 2;
    }
    void reset() {
        powerOn();
        present = true;
        failAfter = -1;
        wiredSda = PIN_I2C_A;
        wiredScl = PIN_I2C_B;
        lineLow = Line::None;
        ghostOnWrongPins = false;
        maxClockHz = 0;
        foreign = false;
        foreignValue = 0x00;
        clearLog();
    }
    void clearLog() {
        writes.clear();
        pllResets[0] = pllResets[1] = 0;
        failedTransfers = 0;
    }
    unsigned writesTo(uint8_t reg) const {
        unsigned n = 0;
        for (const RegWrite& w : writes) n += w.reg == reg;
        return n;
    }
    unsigned totalPllResets() const { return pllResets[0] + pllResets[1]; }

    // Bus side, used by TwoWire.
    bool transfer(uint8_t addr) {
        if (failAfter == 0) ++failedTransfers;
        if (!present || addr != ADDR || failAfter == 0) return false;
        if (failAfter > 0) --failAfter;
        return true;
    }
    void swapWires() {
        const int sda = wiredSda;
        wiredSda = wiredScl;
        wiredScl = sda;
    }

    void write(uint8_t reg, uint8_t value) {
        writes.push_back(RegWrite {reg, value});
        if (foreign) return;
        if (reg == 177) {
            if (value & 0x20) ++pllResets[0];
            if (value & 0x80) ++pllResets[1];
            return; // self-clearing
        }
        regs[reg] = value;
    }
    uint8_t read(uint8_t reg) {
        if (foreign) return foreignValue;
        if (reg == 0 && initReads > 0) {
            --initReads;
            return regs[0] | 0x80;
        }
        return regs[reg];
    }
};

inline Si5351Chip& si5351() {
    static Si5351Chip chip;
    return chip;
}

// Set by any transfer on a bus that is not started; on the ESP32 that leaks the Wire lock.
inline bool& wireMisuse() {
    static bool misuse = false;
    return misuse;
}

} // namespace Fake

class TwoWire {
public:
    unsigned beginCalls = 0;
    unsigned endCalls = 0;
    unsigned setClockCalls = 0;
    uint32_t clockHz = 0;
    uint16_t timeOutMs = 50;
    bool failBegin = false;

    // Like arduino-esp32 2.0.x: begin() on a running bus keeps the old pins.
    bool begin(int sda, int scl, uint32_t hz) {
        ++beginCalls;
        if (begun_) return true;
        if (failBegin || sda == scl) return false;
        sda_ = sda;
        scl_ = scl;
        clockHz = hz;
        begun_ = true;
        return true;
    }
    bool end() {
        ++endCalls;
        begun_ = false;
        return true;
    }
    void setClock(uint32_t hz) {
        ++setClockCalls;
        clockHz = hz;
    }
    void setTimeOut(uint16_t ms) { timeOutMs = ms; }
    bool begun() const { return begun_; }
    int sda() const { return sda_; }
    int scl() const { return scl_; }
    void resetCounters() { beginCalls = endCalls = setClockCalls = 0; }

    void beginTransmission(uint8_t addr) {
        addr_ = addr;
        txLen_ = 0;
    }
    size_t write(uint8_t b) {
        if (txLen_ >= sizeof(tx_)) return 0;
        tx_[txLen_++] = b;
        return 1;
    }
    size_t write(const uint8_t* data, size_t len) {
        size_t n = 0;
        while (n < len && write(data[n])) ++n;
        return n;
    }
    // 0 = ACK, 2 = address NACK, 4 = bus not started, 5 = timeout.
    uint8_t endTransmission(bool = true) {
        const uint8_t r = address_(addr_);
        if (r != 0) return r;
        if (txLen_ == 0) return 0; // probe
        pointer_ = tx_[0];
        if (ghost_) return 0;
        Fake::Si5351Chip& chip = Fake::si5351();
        for (size_t i = 1; i < txLen_; ++i) chip.write(pointer_++, tx_[i]);
        return 0;
    }
    uint8_t requestFrom(uint8_t addr, uint8_t len, uint8_t) {
        rxLen_ = rxPos_ = 0;
        if (address_(addr) != 0) return 0;
        Fake::Si5351Chip& chip = Fake::si5351();
        for (uint8_t i = 0; i < len && rxLen_ < sizeof(rx_); ++i) rx_[rxLen_++] = ghost_ ? 0xFF : chip.read(pointer_++);
        return (uint8_t)rxLen_;
    }
    int read() { return rxPos_ < rxLen_ ? rx_[rxPos_++] : -1; }

private:
    uint8_t address_(uint8_t addr) {
        Fake::Si5351Chip& chip = Fake::si5351();
        ghost_ = false;
        if (!begun_) {
            Fake::wireMisuse() = true;
            return 4;
        }
        if (chip.lineLow != Fake::Si5351Chip::Line::None) {
            if (chip.lineLow == Fake::Si5351Chip::Line::Scl) Fake::advanceMs(13);
            return 5;
        }
        if (sda_ != chip.wiredSda || scl_ != chip.wiredScl) {
            // The chip never sees a valid address frame.
            ghost_ = chip.ghostOnWrongPins;
            return ghost_ ? 0 : 2;
        }
        if (chip.maxClockHz != 0 && clockHz > chip.maxClockHz) return 2;
        return chip.transfer(addr) ? 0 : 2;
    }

    bool begun_ = false;
    int sda_ = -1;
    int scl_ = -1;
    bool ghost_ = false;
    uint8_t addr_ = 0;
    uint8_t pointer_ = 0;
    uint8_t tx_[32] {};
    size_t txLen_ = 0;
    uint8_t rx_[32] {};
    size_t rxLen_ = 0;
    size_t rxPos_ = 0;
};

inline TwoWire& fakeWire() {
    static TwoWire wire;
    return wire;
}
#define Wire fakeWire()
