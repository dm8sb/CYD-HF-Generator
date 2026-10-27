#pragma once

#include <Arduino.h>

class SI5351Manager {
public:
    enum class Pll : uint8_t { A = 0, B = 1 };

    bool begin(int32_t correction);
    bool ready() const { return initialized_; }
    // Correction in 0.01 ppm. Takes effect with the next setup call; does not re-init the chip.
    void setCorrection(int32_t correction);
    bool setupStandalone(uint8_t clk, Pll pll, uint64_t freq, uint8_t driveMa);
    // Master and follower share one PLL. 0/90 deg use the phase offset register,
    // 180/270 deg additionally invert the follower output (CLK_INV).
    bool setupIQPair(uint8_t masterClk, uint8_t followerClk, Pll pll, uint64_t freq, uint8_t driveMa, uint16_t phaseDeg);
    bool enableOutput(uint8_t clk, bool enable);
    // Expected output frequency from the programmed PLL/MultiSynth values, assuming the correction is right.
    double actualHz(uint8_t clk) const { return clk <= 2 ? actualHz_[clk] : 0.0; }
    uint8_t outputMask() const { return outputMask_; }

private:
    bool writeReg_(uint8_t reg, uint8_t value);
    bool writeRegs_(uint8_t reg, const uint8_t* data, uint8_t len);
    uint8_t readReg_(uint8_t reg, bool* ok);
    bool setupPll_(Pll pll, int32_t mult, int32_t num, int32_t denom);
    void applyOutputMask_();
    bool probeI2C_();

    bool initialized_ = false;
    int32_t correction_ = 0;
    uint8_t outputMask_ = 0;
    double actualHz_[3] {0, 0, 0};
};
