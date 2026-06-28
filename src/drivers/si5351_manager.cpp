#include "si5351_manager.h"
#include <si5351.h>
#include <Wire.h>
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>
#include <utils/frequency_utils.h>

bool SI5351Manager::begin(int32_t correction) {
    correction_ = correction;
    Serial.printf("[SI5351] Init start (explicit SDA=%d SCL=%d)\n", PIN_I2C_SDA, PIN_I2C_SCL);

    if (!probeI2C_()) {
        initialized_ = false;
        Logger::error("SI5351", "No I2C ACK at address 0x60 before init. Check configured SDA/SCL pins, VCC, GND, pullups and module address.");
        return false;
    }

    si5351_Init(correction_, PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(I2C_FREQ_HZ);

    if (!probeI2C_()) {
        initialized_ = false;
        Logger::error("SI5351", "No I2C ACK at address 0x60 after init");
        return false;
    }

    si5351_EnableOutputs(0);
    outputMask_ = 0;
    initialized_ = true;

    bool ok = false;
    uint8_t status = readReg_(0, &ok);
    if (ok) {
        Logger::value("SI5351", "Device status reg0", status);
        Logger::value("SI5351", "SYS_INIT bit", status & 0x80 ? 1 : 0);
        Logger::value("SI5351", "LOL_B bit", status & 0x40 ? 1 : 0);
        Logger::value("SI5351", "LOL_A bit", status & 0x20 ? 1 : 0);
        Logger::value("SI5351", "LOS bit", status & 0x10 ? 1 : 0);
    } else {
        Logger::error("SI5351", "Could not read status register 0");
    }

    Logger::info("SI5351", "Init OK; all outputs High-Z");
    return true;
}

bool SI5351Manager::probeI2C_() {
    Wire.beginTransmission(0x60);
    uint8_t err = Wire.endTransmission(true);
    Logger::value("SI5351", "I2C probe 0x60 error", err);
    return err == 0;
}

uint8_t SI5351Manager::readReg_(uint8_t reg, bool* ok) {
    if (ok) *ok = false;
    Wire.beginTransmission(0x60);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission(false);
    if (err != 0) {
        Logger::value("SI5351", "ReadReg address phase error", err);
        return 0;
    }
    int n = Wire.requestFrom((uint8_t)0x60, (uint8_t)1, (uint8_t)true);
    if (n != 1 || !Wire.available()) {
        Logger::value("SI5351", "ReadReg bytes", n);
        return 0;
    }
    if (ok) *ok = true;
    return Wire.read();
}

void SI5351Manager::setCorrection(int32_t correction) {
    correction_ = correction;
    if (initialized_) {
        Logger::info("SI5351", "Re-init to apply correction");
        si5351_Init(correction_, PIN_I2C_SDA, PIN_I2C_SCL);
        Wire.setClock(I2C_FREQ_HZ);
        applyOutputMask_();
    }
}

uint8_t SI5351Manager::driveToLib_(uint8_t ma) const {
    switch (ma) {
        case 2: return SI5351_DRIVE_STRENGTH_2MA;
        case 6: return SI5351_DRIVE_STRENGTH_6MA;
        case 8: return SI5351_DRIVE_STRENGTH_8MA;
        case 4:
        default: return SI5351_DRIVE_STRENGTH_4MA;
    }
}

uint8_t SI5351Manager::phaseOffset_(uint16_t phaseDeg, int32_t div) const {
    if (phaseDeg == 0) return 0;
    uint32_t off = ((uint32_t)phaseDeg * 4UL * (uint32_t)div + 180UL) / 360UL;
    return (uint8_t)off;
}

bool SI5351Manager::canApplyPhase(uint64_t freq, uint16_t phaseDeg) {
    if (!(phaseDeg == 0 || phaseDeg == 90 || phaseDeg == 180 || phaseDeg == 270)) return false;
    if (phaseDeg == 0) return true;
    si5351PLLConfig_t pll;
    si5351OutputConfig_t out;
    si5351_CalcIQ((int32_t)FrequencyUtils::nearestSi5351Hz(freq), &pll, &out);
    uint32_t off = ((uint32_t)phaseDeg * 4UL * (uint32_t)out.div + 180UL) / 360UL;
    if (off > 127) {
        Logger::error("SI5351", "Requested phase exceeds 7-bit phase register at this frequency");
        return false;
    }
    return true;
}

bool SI5351Manager::setFrequency(uint8_t clk, uint64_t freq) {
    if (!initialized_ || clk > 2) return false;
    freq = FrequencyUtils::nearestSi5351Hz(freq);
    Logger::value("SI5351", "Disable-before-frequency CLK", clk);
    enableOutput(clk, false);

    si5351PLLConfig_t pll;
    si5351OutputConfig_t out;
    si5351_Calc((int32_t)freq, &pll, &out);
    si5351PLL_t pllId = (clk == 1) ? SI5351_PLL_B : SI5351_PLL_A;
    int rc = si5351_SetupOutput(clk, pllId, (si5351DriveStrength_t)driveToLib_(driveMa_[clk]), &out, 0);
    si5351_SetupPLL(pllId, &pll);
    freq_[clk] = freq;
    Logger::valueU64("SI5351", clk == 0 ? "Set Freq CLK0" : (clk == 1 ? "Set Freq CLK1" : "Set Freq CLK2"), freq);
    Logger::value("SI5351", "SetupOutput rc", rc);
    if (!probeI2C_()) {
        Logger::error("SI5351", "I2C probe failed after setFrequency");
        return false;
    }
    return rc == 0;
}

bool SI5351Manager::setupIQPair(uint8_t masterClk, uint8_t followerClk, uint64_t freq, uint8_t masterDrive, uint8_t followerDrive, uint16_t phaseDeg) {
    if (!initialized_ || masterClk > 2 || followerClk > 2 || masterClk == followerClk) return false;
    freq = FrequencyUtils::nearestSi5351Hz(freq);
    Logger::info("SI5351", "Disable IQ pair before reconfigure");
    enableOutput(masterClk, false);
    enableOutput(followerClk, false);

    driveMa_[masterClk] = masterDrive;
    driveMa_[followerClk] = followerDrive;

    si5351PLLConfig_t pll;
    si5351OutputConfig_t out;
    si5351_CalcIQ((int32_t)freq, &pll, &out);
    si5351PLL_t pllId = (masterClk == 1) ? SI5351_PLL_B : SI5351_PLL_A;
    if (!canApplyPhase(freq, phaseDeg)) return false;
    uint8_t phase = phaseOffset_(phaseDeg, out.div);

    int rc1 = si5351_SetupOutput(masterClk, pllId, (si5351DriveStrength_t)driveToLib_(masterDrive), &out, 0);
    int rc2 = si5351_SetupOutput(followerClk, pllId, (si5351DriveStrength_t)driveToLib_(followerDrive), &out, phase);
    si5351_SetupPLL(pllId, &pll);
    freq_[masterClk] = freq;
    freq_[followerClk] = freq;
    Logger::valueU64("SI5351", "Set IQ frequency", freq);
    Logger::value("SI5351", "IQ phase deg", phaseDeg);
    Logger::value("SI5351", "IQ phase register", phase);
    if (!probeI2C_()) {
        Logger::error("SI5351", "I2C probe failed after setupIQPair");
        return false;
    }
    return rc1 == 0 && rc2 == 0;
}

bool SI5351Manager::enableOutput(uint8_t clk, bool enable) {
    if (!initialized_ || clk > 2) return false;
    if (enable) outputMask_ |= (1U << clk);
    else outputMask_ &= ~(1U << clk);
    applyOutputMask_();
    Logger::value("SI5351", enable ? "Enable CLK" : "Disable CLK", clk);
    return true;
}

void SI5351Manager::applyOutputMask_() {
    si5351_EnableOutputs(outputMask_);
    Logger::value("SI5351", "Output mask", outputMask_);
}

bool SI5351Manager::setPhase(uint8_t clk, uint16_t phase) {
    Logger::value("SI5351", "Phase requested CLK", clk);
    Logger::value("SI5351", "Phase deg", phase);
    // Phase requires a shared PLL and known divider, therefore AppController calls setupIQPair().
    return initialized_ && clk <= 2;
}

bool SI5351Manager::setDrive(uint8_t clk, uint8_t strength) {
    if (clk > 2) return false;
    driveMa_[clk] = strength;
    Logger::value("SI5351", "Drive mA cached", strength);
    // Do not reprogram the SI5351 here. Frequency/IQ setup applies cached drive.
    return true;
}
