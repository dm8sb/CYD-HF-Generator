#pragma once

#include <Arduino.h>

// Register-level driver for the SI5351A (25 MHz crystal, CLK0..CLK2), see Skyworks AN619.
// Register writes are cached, so repeated setup calls only send what changed.
class SI5351Manager {
public:
    enum class Pll : uint8_t { A = 0, B = 1 };
    // Why the last begin() failed. NotSi5351: something answers at 0x60 but fails the
    // read-only identity check (no write). NoReadback: it passed, but did not keep the
    // output enable register (the only write it received).
    enum class Error : uint8_t { None, NoAck, NotSi5351, NoReadback, Io };
    static constexpr uint8_t I2C_ADDR = 0x60;

    // Probes, checks the identity and initializes the chip with all outputs off.
    // Call again to reconnect. Needs an open bus (I2CBus).
    bool begin(int32_t correction);
    bool ready() const { return ready_; }
    // Read-only part of the identity check (registers 0 and 183), no write.
    bool identify();
    Error lastError() const { return lastError_; }
    // Register 183 bits 5:0 differ from the documented reset value (e.g. a clone).
    bool idUnverified() const { return idUnverified_; }
    uint8_t revId() const { return revId_; }
    // Reads back the output registers. Returns false and drops ready() if the chip
    // does not answer or lost its configuration (e.g. after a power interruption).
    bool checkAlive();
    // Crystal correction in 0.01 ppm, positive = crystal runs fast. Takes effect with the next setup call.
    void setCorrection(int32_t correction) { correction_ = correction; }
    // Below ~110 MHz the PLL stays fixed (900 or 887.5 MHz) and only the MultiSynth changes,
    // so tuning does not interrupt the output. The PLL is reset only when its multiplier changes.
    // Error vs. freq <= 0.5 Hz (assuming the correction is right).
    bool setupStandalone(uint8_t clk, Pll pll, uint64_t freq, uint8_t driveMa);
    // Master and follower share one PLL and an integer MultiSynth divider. 90/270 deg use the
    // phase offset register, 180/270 deg additionally invert the follower output (CLK_INV).
    bool setupIQPair(uint8_t masterClk, uint8_t followerClk, Pll pll, uint64_t freq, uint8_t driveMa, uint16_t phaseDeg);
    bool enableOutput(uint8_t clk, bool enable);
    // Disables the output and powers its MultiSynth down.
    bool powerDown(uint8_t clk);
    // Output frequency of the programmed PLL/MultiSynth values, assuming the correction is right.
    double actualHz(uint8_t clk) const { return clk <= 2 ? actualHz_[clk] : 0.0; }

    struct Ratio {
        uint32_t a;
        uint32_t b;
        uint32_t c; // value = a + b / c
    };

private:
    struct OutputCache {
        uint8_t ctrl;
        uint8_t ms[8];
        uint8_t phase;
        bool msValid;
    };
    struct PllCache {
        uint8_t regs[8];
        bool valid;
    };

    uint64_t xtalQuarterHz_() const;
    double synthesizedHz_(const Ratio& pll, const Ratio& ms, uint8_t rLog2) const;
    bool pllDiffers_(Pll pll, const uint8_t* regs) const;
    bool msDiffers_(uint8_t clk, const uint8_t* ms, uint8_t phase) const;
    bool writePll_(Pll pll, const uint8_t* regs);
    bool writeOutput_(uint8_t clk, uint8_t ctrl, const uint8_t* ms, uint8_t phase);
    bool resetPll_(Pll pll);
    bool writeReg_(uint8_t reg, uint8_t value);
    bool writeRegs_(uint8_t reg, const uint8_t* data, uint8_t len);
    bool readRegs_(uint8_t reg, uint8_t* data, uint8_t len);
    bool probe_();
    // Marks the chip as lost after a failed I2C transfer; returns ok.
    bool finish_(bool ok);

    bool identify_(uint8_t& status);
    bool fail_(Error e);

    bool ready_ = false;
    Error lastError_ = Error::None;
    bool idUnverified_ = false;
    uint8_t revId_ = 0;
    int32_t correction_ = 0;
    uint8_t outputMask_ = 0;
    OutputCache out_[3] {};
    PllCache pll_[2] {};
    double actualHz_[3] {0, 0, 0};
};
