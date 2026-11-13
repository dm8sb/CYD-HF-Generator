// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <types.h>
#include <drivers/si5351_plan.h>

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
    // Reads back the output registers. Returns false and drops ready() if the chip
    // does not answer or lost its configuration (e.g. after a power interruption).
    bool checkAlive();
    // Crystal correction in 0.01 ppm, positive = crystal runs fast. Takes effect with the next setup call.
    void setCorrection(int32_t correction) { correction_ = correction; }
    // CONFIG options. They survive begin(); begin() writes the off state to the chip, spread
    // spectrum follows with the first PLL A setup. The tuning mode takes effect with the next setup call.
    void setTuningMode(TuningMode m) { tuningMode_ = m; }
    TuningMode tuningMode() const { return tuningMode_; }
    // PLL A only (AN619): CH1 and CH3 with master CH1. Takes effect with the next setup call on
    // PLL A: it plans PLL A so that the modulated VCO stays within 600..900 MHz (see si5351_plan.h),
    // writes the parameters and resets PLL A (short dropout, an IQ pair is realigned). FBA_INT
    // stays 0 while it is on. Stores the option only; I2C errors show up in the setup call.
    void setSpread(Spread s);
    // Level of disabled outputs (register 24). Written at once if the chip is ready; running
    // outputs are not touched. powerDown() depends on it.
    bool setOffState(OffState s);
    // Divider choice per tuning mode, see planStandalone(). Only changed registers are written.
    // Fractional: the PLL stays fixed (900 or 887.5 MHz) and only the MultiSynth changes, so
    // tuning does not interrupt the output; the PLL is reset only when its multiplier changes.
    // LowNoise: the PLL follows the frequency without a reset while the MultiSynth divider stays;
    // a new divider resets the PLL (each PLL change with LOW_NOISE_RESET_EACH_STEP).
    // Integer: the PLL is reset when its multiplier changes.
    bool setupStandalone(uint8_t clk, Pll pll, uint64_t freq, uint8_t driveMa);
    // Master and follower share one PLL and an integer MultiSynth divider (planIQ()). 90/270 deg use
    // the phase offset register, 180/270 deg additionally invert the follower output (CLK_INV).
    bool setupIQPair(uint8_t masterClk, uint8_t followerClk, Pll pll, uint64_t freq, uint8_t driveMa, uint16_t phaseDeg);
    bool enableOutput(uint8_t clk, bool enable);
    // Disables the output. With off state Low also powers it down (CLKx_PDN); with High and
    // High-Z the output driver stays powered, so that register 24 sets the level.
    bool powerDown(uint8_t clk);
    // Output frequency of the programmed PLL/MultiSynth values, assuming the correction is right.
    // With spread spectrum the unmodulated frequency.
    double actualHz(uint8_t clk) const { return clk <= 2 ? actualHz_[clk] : 0.0; }

    using Ratio = SynthRatio;

private:
    static constexpr uint8_t SSC_REG_COUNT = 13; // registers 149..161

    struct OutputCache {
        uint8_t ctrl;
        uint8_t ms[8];
        uint8_t phase;
        Ratio msRatio;
        bool msValid;
    };
    struct PllCache {
        uint8_t regs[8];
        Ratio ratio;
        uint8_t fbCtrl; // CLK6 / CLK7 control register with FBA_INT / FBB_INT
        bool valid;
    };

    uint64_t xtalQuarterHz_() const;
    // Spread spectrum that applies to a PLL, and whether the chip runs another option on it.
    Spread spreadOn_(Pll pll) const { return pll == Pll::A ? spread_ : Spread::Off; }
    bool spreadChanges_(Pll pll) const { return pll == Pll::A && sscApplied_ != spread_; }
    bool pllDiffers_(Pll pll, const uint8_t* regs) const;
    bool msDiffers_(uint8_t clk, const uint8_t* ms, uint8_t phase) const;
    // PLL parameters, FBx_INT and (PLL A) spread spectrum. No reset.
    bool writePll_(Pll pll, const Ratio& ratio);
    bool writeFbInt_(Pll pll, bool on);
    bool writeSpread_(const Ratio& pllA);
    bool writeOutput_(uint8_t clk, uint8_t ctrl, const uint8_t* ms, uint8_t phase, const Ratio& msRatio);
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
    TuningMode tuningMode_ = TuningMode::Fractional;
    Spread spread_ = Spread::Off;
    Spread sscApplied_ = Spread::Off; // option in registers 149..161
    OffState offState_ = OffState::Low;
    Error lastError_ = Error::None;
    bool idUnverified_ = false;
    int32_t correction_ = 0;
    uint8_t outputMask_ = 0;
    OutputCache out_[3] {};
    PllCache pll_[2] {};
    uint8_t ssc_[SSC_REG_COUNT] {};
    double actualHz_[3] {0, 0, 0};
};
