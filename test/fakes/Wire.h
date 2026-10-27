#pragma once

// Wire stand-in for the host unit tests: an SI5351A register model at 0x60.
// Every register write is logged, so tests can check what the driver sends.

#include <Arduino.h>
#include <vector>

namespace Fake {

struct RegWrite {
    uint8_t reg;
    uint8_t value;
};

class Si5351Chip {
public:
    static constexpr uint8_t ADDR = 0x60;

    uint8_t regs[256];
    bool present = true;
    // Successful transfers left before the bus fails for good (-1 = never),
    // like a disconnected SDA line.
    int failAfter = -1;
    // Status reads that still report SYS_INIT after power-on.
    uint8_t initReads = 0;
    std::vector<RegWrite> writes;
    unsigned pllResets[2] = {0, 0}; // PLL A, PLL B

    Si5351Chip() { powerOn(); }

    // Power-on state: all clocks powered down and disabled, nothing configured.
    void powerOn() {
        memset(regs, 0, sizeof(regs));
        regs[3] = 0xFF;
        for (uint8_t i = 16; i <= 23; ++i) regs[i] = 0x80;
        initReads = 2;
    }
    void reset() {
        powerOn();
        present = true;
        failAfter = -1;
        clearLog();
    }
    void clearLog() {
        writes.clear();
        pllResets[0] = pllResets[1] = 0;
    }
    unsigned writesTo(uint8_t reg) const {
        unsigned n = 0;
        for (const RegWrite& w : writes) n += w.reg == reg;
        return n;
    }
    unsigned totalPllResets() const { return pllResets[0] + pllResets[1]; }

    // Bus side, used by TwoWire.
    bool transfer(uint8_t addr) {
        if (!present || addr != ADDR || failAfter == 0) return false;
        if (failAfter > 0) --failAfter;
        return true;
    }
    void write(uint8_t reg, uint8_t value) {
        writes.push_back(RegWrite {reg, value});
        if (reg == 177) {
            if (value & 0x20) ++pllResets[0];
            if (value & 0x80) ++pllResets[1];
            return; // self-clearing
        }
        regs[reg] = value;
    }
    uint8_t read(uint8_t reg) {
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

} // namespace Fake

class TwoWire {
public:
    bool begin(int, int, uint32_t) { return true; }
    void setClock(uint32_t) {}

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
    // 0 = ACK, 2 = address NACK.
    uint8_t endTransmission(bool = true) {
        Fake::Si5351Chip& chip = Fake::si5351();
        if (!chip.transfer(addr_)) return 2;
        if (txLen_ == 0) return 0; // probe
        pointer_ = tx_[0];
        for (size_t i = 1; i < txLen_; ++i) chip.write(pointer_++, tx_[i]);
        return 0;
    }
    uint8_t requestFrom(uint8_t addr, uint8_t len, uint8_t) {
        Fake::Si5351Chip& chip = Fake::si5351();
        rxLen_ = rxPos_ = 0;
        if (!chip.transfer(addr)) return 0;
        for (uint8_t i = 0; i < len && rxLen_ < sizeof(rx_); ++i) rx_[rxLen_++] = chip.read(pointer_++);
        return (uint8_t)rxLen_;
    }
    int read() { return rxPos_ < rxLen_ ? rx_[rxPos_++] : -1; }

private:
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
