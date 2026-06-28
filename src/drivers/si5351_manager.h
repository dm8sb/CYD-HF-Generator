#pragma once

#include <Arduino.h>

class SI5351Manager {
public:
    bool begin(int32_t correction);
    bool setFrequency(uint8_t clk, uint64_t freq);
    bool setupIQPair(uint8_t masterClk, uint8_t followerClk, uint64_t freq, uint8_t masterDrive, uint8_t followerDrive, uint16_t phaseDeg);
    bool enableOutput(uint8_t clk, bool enable);
    bool setPhase(uint8_t clk, uint16_t phase);
    bool setDrive(uint8_t clk, uint8_t strength);
    bool canApplyPhase(uint64_t freq, uint16_t phaseDeg);
    void setCorrection(int32_t correction);
    uint8_t outputMask() const { return outputMask_; }

private:
    uint8_t driveToLib_(uint8_t ma) const;
    uint8_t phaseOffset_(uint16_t phaseDeg, int32_t div) const;
    void applyOutputMask_();
    bool probeI2C_();
    uint8_t readReg_(uint8_t reg, bool* ok);

    bool initialized_ = false;
    int32_t correction_ = 0;
    uint8_t outputMask_ = 0;
    uint8_t driveMa_[3] {4, 4, 4};
    uint64_t freq_[3] {0, 0, 0};
};
